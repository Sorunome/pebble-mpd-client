var Clay = require('pebble-clay'),
	clayConfig = require('./config'),
	clay = new Clay(clayConfig,null,{autoHandleEvents:false}),
	mpdUpdateTime = 5,
	connectedToMpd = false,
	timer = false,
	sock = false,
	sock_sendBuffer = [],
	sock_connected = false;

mpdState = {
	volume:0,
	repeat:false,
	random:false,
	single:false,
	consume:false,
	playlist:0,
	playlistlength:0,
	xfade:0,
	state:'loading',
	song:0,
	songid:0,
	time:0,
	pos:0,
	artist:'',
	title:'',
	album:'',
	file:''
};

function sock_send(o){
	var s = JSON.stringify(o);
	console.log('>> '+s)
	if(sock_connected){
		sock.send(s);
	}else{
		sock_sendBuffer.push(s);
	}
}

function pebble_send_status(s){
	Pebble.sendAppMessage({
		state:-1,
		artist:'',
		title:s,
		time:0,
		pos:0
	});
}

function setVol(d){
	d += mpdState.volume;
	
	if(d < 0){
		mpdState.volume = 0;
		return 0;
	}
	if(d > 100){
		mpdState.volume = 100;
		return 100;
	}
	mpdState.volume = d;
	return d;
}

function isSameState(state){
	for(var k in state){
		if(k == 'pos'){
			if(mpdState.state == 'play' && Math.abs(state.pos+mpdUpdateTime-mpdState.pos) > 1){
				console.log('wat');
				return false;
			}
		}else if(state[k] != mpdState[k]){
			return false;
		}
	}
	return true;
}

function mpdRequest_direct(commands){
	var config = JSON.parse(localStorage.getItem('clay-settings')),
		http = new XMLHttpRequest(),
		handleReply = function(){
			console.log('readystatechange '+http.readyState);
			if(http.readyState >= 3){
				var s = http.response?http.response:http.responseText;
				if(s){
					var lines = s.split('\n'),
						oldMpdState = JSON.parse(JSON.stringify(mpdState)), // we need to clone it
						ignorePos = false;
					console.log(JSON.stringify(lines));
					mpdState.artist = '';
					mpdState.title = '';
					mpdState.album = '';
					for(var i = 0;i < lines.length;i++){
						if(lines[i] == 'OK'){
							http.abort();
							break;
						}
						var matches = lines[i].match(/^(\w+): (.*)/i);
						console.log(JSON.stringify(matches));
						if(matches){
							var key = matches[1].toLowerCase();
							if(mpdState[key] !== undefined){
								if(key == 'time'){
									if(matches[2].indexOf(':') != -1){
										var p = matches[2].split(':');
										mpdState.pos = parseInt(p[0]);
										matches[2] = p[1];
										ignorePos = true;
									}
								}
								if((key == 'pos' && !ignorePos) || key != 'pos'){
									mpdState[key] = mpdState[key].constructor(matches[2]);
								}
							}
							
						}
					}
					if(!isSameState(oldMpdState)){
						console.log('!!!!!sending out new state...!!!!!');
						
						var title = mpdState.title,
							artist = mpdState.artist;
						if(!title){
							title = mpdState.file.split('/');
							title = title[title.length-1].split('.')[0];
						}
						if(mpdState.album && title.length < 28){
							title += ' – '+mpdState.album;
						}
						if(!artist){
							artist = 'Unkown';
						}
						console.log('Title: '+title);
						console.log('Artist: '+artist);
						
						connectedToMpd = true;
						Pebble.sendAppMessage({
							state:['play','pause','stop'].indexOf(mpdState.state),
							artist:artist.substring(0,20),
							title:title.substring(0,30),
							time:mpdState.time,
							pos:mpdState.pos
						});
					}
				}
			}
		},
		clearHeaders = ['Content-Type','Connection','Accept','User-Agent','Accept-Language','Accept-Encoding','Content-Length'];
		
	if(config.passwd){
		commands.unshift('password '+config.passwd);
	}
	commands.unshift('command_list_begin');
	commands.push('status');
	commands.push('currentsong');
	commands.push('command_list_end');
	
	console.log('>> '+JSON.stringify(commands));
	
	var s = commands.join('\n')+'\n';
	//http.open(commands.join('\n')+'\n','http://'+config.host+':'+config.port,true);
	try{
		http.open(s,'http://'+config.host+':'+config.port,true);
		console.log('using http method hack');
		s = '';
	}catch(e){
		console.log('fallback to POST');
		http.open('POST','http://'+config.host+':'+config.port,true);
	}
	for(var i = 0; i < clearHeaders.length; i++){
		http.setRequestHeader(clearHeaders[i],'');
	}
	
	http.onreadystatechange = handleReply;
	http.ontimeout = handleReply;
	http.onloadstart = function(){
		if(!connectedToMpd){
			setTimeout(function(){
				if(!connectedToMpd){
					connectedToMpd = true;
					Pebble.sendAppMessage({
						state:['play','pause','stop'].indexOf('stop'),
						artist:'',
						title:'———',
						time:0,
						pos:0
					});
					http.abort();
				}
			},3000);
		}
	};
	console.log(s);
	http.send(s);
	http.timeout = 2000;
};

function mpdRequest(commands){
	var config = JSON.parse(localStorage.getItem('clay-settings'));
	if(config.proxy_use){
		if(commands.length == 0){
			return;
		}
		sock_send({
			'action':'commands',
			'commands':commands
		});
	}else{
		mpdRequest_direct(commands);
	}
}

function getInfo(){
	mpdRequest([]);
}

function startUpdates(){
	if(timer){
		clearInterval(timer);
		timer = false;
	}
	if(sock){
		sock.close();
	}
	var config = JSON.parse(localStorage.getItem('clay-settings'));
	if(config.proxy_use){
		console.log('Using proxy');
		sock_sendBuffer = [];
		sock_connected = false;
		sock = new WebSocket('ws://'+config.proxy_host+':6601');
		sock.onopen = function(e){
			sock_connected = true;
			for(var i = 0; i < sock_sendBuffer.length; i++){
				sock.send(sock_sendBuffer[i]);
			}
			sock_sendBuffer = [];
		};
		sock.onmessage = function(e){
			try{
				console.log('NEW MESSAGE');
				console.log(e.data);
				var data = JSON.parse(e.data);
				switch(data.action){
					case 'state':
						console.log(JSON.stringify(data.state));
						mpdState.volume = data.state.volume;
						mpdState.state = ['play','pause','stop'][data.state.state]
						Pebble.sendAppMessage(data.state);
						break;
					case 'invalid_pwd':
						sock.onclose = function(){};
						pebble_send_status('Invalid password');
						break;
				}
			}catch(e){
				console.log('Websocket Recieve error!');
			}
		};
		sock.onclose = function(e){
			console.log('Websocket closed!');
			pebble_send_status('Connection closed');
		};
		sock.onerror = function(e){
			console.log('Websocket error!');
			pebble_send_status('Connection error');
		};
		sock_send({
			'action':'ident',
			'passwd':config.proxy_passwd
		});
		sock_send({
			'action':'mpd_server',
			'host':config.host,
			'port':parseInt(config.port),
			'passwd':config.passwd
		});
	}else{
		console.log('Attempting direct connection');
		timer = setInterval(getInfo,mpdUpdateTime*1000);
	}
	getInfo();
}


Pebble.addEventListener('ready',function(){
	var config = JSON.parse(localStorage.getItem('clay-settings'));
	if(config){
		Pebble.sendAppMessage({JSReady:config?parseInt(config.app_timeout):120});
		startUpdates();
	}else{
		pebble_send_status('Configure via app');
	}
});
Pebble.addEventListener('showConfiguration',function(e){
	Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed',function(e){
	if(e && !e.response){
		return;
	}
	// flatten the settings for storage
	var config = {},
		settings = JSON.parse(e.response);
	Object.keys(settings).forEach(function(key) {
		if(typeof settings[key] === 'object' && settings[key]){
			config[key] = settings[key].value;
		}else{
			config[key] = settings[key];
		}
	});
	Pebble.sendAppMessage({JSReady:parseInt(config.app_timeout)});
	localStorage.setItem('clay-settings', JSON.stringify(config));
	startUpdates();
});
Pebble.addEventListener('appmessage',function(e){
	switch(e.payload[0]){
		case 0: // pause
			mpdRequest(['pause 1']);
			break;
		case 1: // play
			if (mpdState.state === 'pause') {
				mpdRequest(['pause 0']);
			} else {
				mpdRequest(['pause 0','play']);
			}
			break;
		case 2: // stop
			mpdRequest(['stop']);
			break;
		case 3: // previous
			mpdRequest(['previous']);
			break;
		case 4: // next
			mpdRequest(['next'])
			break;
		case 5: // vol_up
			mpdRequest(['setvol '+setVol(5)]);
			break;
		case 6: // vol_down
			mpdRequest(['setvol '+setVol(-5)]);
			break;
		case 255: // getinfo
			getInfo();
			break;
	}
});
