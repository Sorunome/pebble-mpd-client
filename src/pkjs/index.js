var Clay = require('pebble-clay'),
	clayConfig = require('./config'),
	clay = new Clay(clayConfig,null,{autoHandleEvents:false});

console.log();
mpdState = {
	volume:0,
	repeat:false,
	random:false,
	single:false,
	consume:false,
	playlist:0,
	playlistlength:0,
	xfade:0,
	state:'stop',
	song:0,
	songid:0
};

function dumpMpdState(){
	for(var k in mpdState){
		console.log(k+': '+mpdState[k]);
	}
}
function mpdRequest(commands,callback){
	var config = JSON.parse(localStorage.getItem('clay-settings')),
		http = new XMLHttpRequest(),
		handleReply = function(){
			if(http.readyState >= 3){
				var s = http.response?http.response:http.responseText;
				if(s){
					var lines = s.split('\n');
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
					dumpMpdState();
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
	commands.unshift('password '+config.passwd);
	commands.unshift('command_list_begin');
	commands.push('status');
	commands.push('command_list_end');
	console.log(commands.join('\n')+'\n');
	http.send(commands.join('\n')+'\n');
	http.timeout = 2000;
	console.log('sent!');
};

Pebble.addEventListener('ready',function(){
	console.log('js ready, i guess');
	Pebble.sendAppMessage({JSReady:1});
});
function getInfo(){
	mpdRequest([],function(){
		console.log(mpdState.state);
		console.log(['play','pause','stop'].indexOf(mpdState.state));
		Pebble.sendAppMessage({
			state:['play','pause','stop'].indexOf(mpdState.state)
		});
	});
}
Pebble.addEventListener('showConfiguration',function(e){
	Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed',function(e){
	console.log('changed config, i guess');
	if(e && !e.response){
		return;
	}
	// flatten the settings for 
	var settingsStorage = {},
		settings = JSON.parse(e.response);
	Object.keys(settings).forEach(function(key) {
		if(typeof settings[key] === 'object' && settings[key]){
			settingsStorage[key] = settings[key].value;
		}else{
			settingsStorage[key] = settings[key];
		}
	});
	localStorage.setItem('clay-settings', JSON.stringify(settingsStorage));
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
		case 255: // getinfo
			getInfo();
			break;
	}
});
