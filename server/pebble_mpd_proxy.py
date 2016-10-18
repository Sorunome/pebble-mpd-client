
#!/usr/bin/python3
## -*- coding: utf-8 -*-

import server,time,re,struct,json,traceback,os,socket,copy
from base64 import b64encode
from hashlib import sha1
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

class WebSocketHandler(server.ServerHandler):
	magic = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
	def setup(self):
		print('New client!')
		self.upgraded_to_sockets = False
		self.handshake_done = False
		
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
		return True
	def recieve(self):
		if not self.handshake_done:
			return self.handshake()
		elif not self.upgraded_to_sockets:
			return self.more_headers()
		else:
			return self.read_next_message()
	def read_next_message(self):
		try:
			b1,b2 = self.socket.recv(2)
		except:
			print('nothing to recieve')
			return False
		
		if b1 & 0x0F == 0x8:
			print('Client asked to close connection')
			return False
		if not b1 & 0x80:
			print('Client must always be masked')
			return False
		
		self.firstRun = False
		length = b2 & 127
		if length == 126:
			length = struct.unpack(">H", self.socket.recv(2))[0]
		elif length == 127:
			length = struct.unpack(">Q", self.socket.recv(8))[0]
		masks = self.socket.recv(4)
		decoded = b""
		for char in self.socket.recv(length):
			decoded += bytes([char ^ masks[len(decoded) % 4]])
		try:
			return self.on_message(json.loads(makeUnicode(decoded)))
		except Exception as inst:
			self.log_error(traceback.format_exc())
			return True
	def send_message(self, message):
		try:
			header = bytearray()
			message = makeUnicode(message)
			
			length = len(message)
			self.socket.send(bytes([129]))
			if length <= 125:
				self.socket.send(bytes([length]))
			elif length >= 126 and length <= 65535:
				self.socket.send(bytes([126]))
				self.socket.send(struct.pack(">H",length))
			else:
				self.socket.send(bytes([127]))
				self.socket.send(struct.pack(">Q",length))
			self.socket.send(bytes(message,'utf-8'))
		except IOError as e:
			self.log_error(traceback.format_exc())
			if e.errno == errno.EPIPE:
				return self.close()
		return True
	def more_headers(self):
		data = ''
		buf = ''
		print('more headers...')
		while True:
			buf = makeUnicode(self.socket.recv(1))
			data += buf
			if ('\r\n\r\n' in data) or ('\n\n' in data) or not buf:
				break
		
		self.upgraded_to_sockets = ('\r\n\r\n' in data) or ('\n\n' in data)
		return True
	def handshake(self):
		data = ''
		buf = ''
		while True:
			buf = makeUnicode(self.socket.recv(1024))
			data += buf
			if len(buf) < 1024:
				break
		print('Handshaking...')
		
		key = re.search('\n[sS]ec-[wW]eb[sS]ocket-[kK]ey[\s]*:[\s]*([^\r\n]*)\r?\n?',data)
		if key:
			key = key.group(1).strip()
		else:
			print('Missing Key!')
			return False
		digest = b64encode(sha1((key + self.magic).encode('latin_1')).digest()).strip().decode('latin_1')
		response = 'HTTP/1.1 101 Switching Protocols\r\n'
		response += 'Upgrade: websocket\r\n'
		response += 'Connection: Upgrade\r\n'
		protocol = re.search('\n[sS]ec-[wW]eb[sS]ocket-[pP]rotocol[\s]*:[\s]*([^\r\n]*)\r?\n?',data)
		if protocol:
			response += 'Sec-WebSocket-Protocol: %s\r\n' % protocol.group(1).strip()
		response += 'Sec-WebSocket-Accept: %s\r\n\r\n' % digest
		
		self.upgraded_to_sockets = ('\r\n\r\n' in data) or ('\n\n' in data)
		self.handshake_done = self.socket.send(bytes(response,'latin_1'))
		return True
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
							'pos':self.mpdState['pos']
						}
					}))
			except:
				traceback.print_exc()
	def on_message(self,m):
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
					self.mpd._write_line('command_list_ok_begin')
					for l in m['commands']:
						self.mpd._write_line(l)
					self.mpd._write_line('command_list_end')
					
					# read the lines
					while True:
						line = self.mpd._rfile.readline().rstrip("\n")
						if not line:
							break
						if line == 'OK':
							break
					self.mpd._command_list = None
					
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
