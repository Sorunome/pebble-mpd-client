
#!/usr/bin/python3
## -*- coding: utf-8 -*-

import server,json,traceback,os,copy,time
from mpd import MPDClient

def makeUnicode(s):
	try:
		return s.decode('utf-8')
	except:
		if s!='':
			return s
	return ''

with open(os.path.dirname(os.path.realpath(__file__))+'/config.json') as f:
	config = json.load(f)

class WebSocketHandler(server.WebSocketHandler):
	def setup_extra(self):
		print('New client!')
		
		self.ident = False
		if config['passwd'] == '':
			self.ident = True
		self.host = ''
		self.passwd = ''
		self.port = 0
		
		self.mpd = False
		self.skip_update = False
		
		self.mpdState = {
			'volume':0,
			'repeat':False,
			'random':False,
			'single':False,
			'consume':False,
			'playlist':0,
			'playlistlength':0,
			'xfade':0,
			'state':'loading',
			'song':0,
			'songid':0,
			'time':0,
			'pos':0,
			'artist':'',
			'title':'',
			'album':'',
			'file':''
		}
		return
	def close(self):
		try:
			self.socket.close()
		except:
			pass
		if self.mpd:
			self.mpd.close()
			self.mpd.disconnect()
	
	def mpd_connect(self):
		try:
			if self.mpd:
				self.mpd.close()
				self.mpd.disconnect()
			self.mpd = MPDClient()
			self.mpd.connect(self.host,self.port)
			if self.passwd != '':
				self.mpd.password(self.passwd)
			
			print('MPD version: ',self.mpd.mpd_version)
			self.mpd_update()
		except:
			traceback.print_exc()
	
	def mpd_isSameState(self,state):
		for k,v in state.items():
			if k == 'pos':
				if self.mpdState['state'] == 'play' and abs(v+1-self.mpdState['pos']) > 1:
					return False
			elif v != self.mpdState[k]:
				return False
		return True
	def mpd_update(self):
		if self.mpd:
			try:
				self.mpd.command_list_ok_begin()
				self.mpd.ping()
				self.mpd.status()
				self.mpd.currentsong()
				
				oldstate = copy.deepcopy(self.mpdState)
				
				self.mpdState['artist'] = ''
				self.mpdState['title'] = ''
				self.mpdState['album'] = ''
				
				ignorePos = False
				for c in self.mpd.command_list_end():
					if c:
						for k,v in c.items():
							k = k.lower()
							if k in self.mpdState:
								if k == 'time':
									if ':' in v:
										parts = v.split(':')
										self.mpdState['pos'] = int(parts[0])
										v = parts[1]
										ignorePos = True
								if (k == 'pos' and not ignorePos) or k != 'pos':
									var = self.mpdState[k]
									if type(var) is int:
										self.mpdState[k] = int(v)
									elif type(var) is bool:
										self.mpdState[k] = bool(int(v))
									else:
										self.mpdState[k] = v
				if not self.mpd_isSameState(oldstate):
					print('Sending out new state!')
					title = self.mpdState['title']
					artist = self.mpdState['artist']
					if not title:
						title = self.mpdState['file'].split('/')[-1].split('.')[0]
					if self.mpdState['album'] and len(title) < 28:
						title += ' — '+self.mpdState['album']
					if not artist:
						artist = 'Unkown'
					print('Title:',title)
					print('Artist:',artist)
					
					self.send_message(json.dumps({
						'action': 'state',
						'state': {
							'state': ['play','pause','stop'].index(self.mpdState['state']),
							'artist':artist[0:20],
							'title':title[0:30],
							'time':self.mpdState['time'],
							'pos':self.mpdState['pos'],
							'volume':self.mpdState['volume']
						}
					}))
			except:
				traceback.print_exc()
	def on_message(self,m):
		if 'passwd' in m:
			p = dict(m)
			p['passwd'] = '--snip--'
			print(p)
		else:
			print(m)
		try:
			if m['action'] == 'ident':
				if self.ident:
					return True
				if m['passwd'] == config['passwd']:
					self.ident = True
				else:
					self.send_message(json.dumps({
						'action': 'invalid_pwd'
					}))
					return False
			elif self.ident:
				if m['action'] == 'mpd_server':
					self.host = m['host']
					self.port = m['port']
					self.passwd = m['passwd']
					self.mpd_connect()
				elif m['action'] == 'commands':
					self.skip_update = True
					try:
						self.mpd.command_list_ok_begin()
					except:
						self.mpd.command_list_end()
						self.mpd.command_list_ok_begin()
						pass
					for l in m['commands']:
						p = l.lower().split(' ')
						if p[0] == 'pause':
							self.mpd.pause(int(p[1]))
						elif p[0] == 'play':
							self.mpd.play()
						elif p[0] == 'stop':
							self.mpd.stop()
						elif p[0] == 'previous':
							self.mpd.previous()
						elif p[0] == 'next':
							self.mpd.next()
						elif p[0] == 'setvol':
							self.mpd.setvol(int(p[1]))
					try:
						self.mpd.command_list_end()
					except:
						traceback.print_exc()
						pass
					
					self.mpd_update()
					self.skip_update = False
		except:
			traceback.print_exc()
			pass
		return True

if __name__ == '__main__':
	s = server.Server(config['host'],6601,WebSocketHandler)
	s.start()
	try:
		while True:
			time.sleep(1)
			for c in s.inputHandlers:
				if not c.skip_update:
					c.mpd_update()
	except KeyboardInterrupt:
		s.stop()
		s.join()
