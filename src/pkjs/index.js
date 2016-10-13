var Clay = require('pebble-clay'),
	clayConfig = require('./config'),
	clay = new Clay(clayConfig,null,{autoHandleEvents:false}),
	mpdUpdateTime = 5;

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
	time:'',
	Artist:'',
	Title:'',
	Album:'',
	file:''
};

function setVol(d){
	d += mpdState.volume;
	if(d < 0){
		return 0;
	}
	if(d > 100){
		return 100;
	}
	return d;
}

function isSameState(state){
	for(var k in state){
		if(k == 'time'){
			if(mpdState.state == 'play' && parseInt(state.time.split(':')[0])+mpdUpdateTime != parseInt(mpdState.time.split(':')[0])){
				return false;
			}
		}else if(state[k] != mpdState[k]){
			return false;
		}
	}
	return true;
}

function mpdRequest(commands,callback){
	var config = JSON.parse(localStorage.getItem('clay-settings')),
		http = new XMLHttpRequest(),
		handleReply = function(){
			if(http.readyState >= 3){
				var s = http.response?http.response:http.responseText;
				if(s){
					var lines = s.split('\n'),
						oldMpdState = JSON.parse(JSON.stringify(mpdState)); // we need to clone it
					
					mpdState.Artist = '';
					mpdState.Title = '';
					mpdState.Album = '';
					for(var i = 0;i < lines.length;i++){
						if(lines[i] == 'OK'){
							http.abort();
							break;
						}
						var matches = lines[i].match(/^(\w+): (.*)/);
						if(matches && mpdState[matches[1]]!==undefined){
							mpdState[matches[1]] = mpdState[matches[1]].constructor(matches[2]);
						}
					}
					if(!isSameState(oldMpdState)){
						console.log('sending out new state...');
						
						var title = mpdState.Title,
							artist = mpdState.Artist;
						if(!title){
							title = mpdState.file.split('/');
							title = title[title.length-1].split('.')[0];
						}
						if(mpdState.Album && title.length < 28){
							title += ' – '+mpdState.Album;
						}
						if(!artist){
							artist = 'Unkown';
						}
						
						Pebble.sendAppMessage({
							state:['play','pause','stop'].indexOf(mpdState.state),
							artist:artist.substring(0,20),
							title:title.substring(0,30),
							time:parseInt(mpdState.time.split(':')[1]),
							pos:parseInt(mpdState.time.split(':')[0])
						});
					}
					if(callback){
						callback();
					}
				}
			}
		};
	http.open('POST','http://'+config.host+':'+config.port,true);
	http.setRequestHeader('Content-Type','text/plain');
	http.onreadystatechange = handleReply;
	http.ontimeout = handleReply;
	if(config.passwd){
		commands.unshift('password '+config.passwd);
	}
	commands.unshift('command_list_begin');
	commands.push('status');
	commands.push('currentsong');
	commands.push('command_list_end');
	
	http.send(commands.join('\n')+'\n');
	http.timeout = 2000;
};


function getInfo(){
	mpdRequest([]);
}
Pebble.addEventListener('ready',function(){
	var config = JSON.parse(localStorage.getItem('clay-settings'))
	Pebble.sendAppMessage({JSReady:config?parseInt(config.app_timeout):120});
	getInfo();
	setInterval(getInfo,mpdUpdateTime*1000);
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
	getInfo();
});
Pebble.addEventListener('appmessage',function(e){
	switch(e.payload[0]){
		case 0: // pause
			mpdRequest(['pause 1']);
			break;
		case 1: // play
			mpdRequest(['pause 0','play']);
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
