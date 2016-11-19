#!/usr/bin/python3
## -*- coding: utf-8 -*-

# Server framework (c) Sorunome
# published under GPL 3 or later

import threading,socket,select,traceback,json,re,time,struct
from base64 import b64encode
from hashlib import sha1

def makeUnicode(s):
	try:
		return s.decode('utf-8')
	except:
		if s!='':
			return s
	return ''

class ServerHandler:
	def __init__(self,s,address):
		self.socket = s
		self.client_address = address
	def setup(self):
		return True
	def recieve(self):
		data = self.socket.recv(1024)
		return True
	def close(self):
		try:
			self.socket.close()
		except:
			pass
	def isHandler(self,s):
		return s == self.socket
	def getSocket(self):
		return self.socket

class Server(threading.Thread):
	host = ''
	port = 0
	backlog = 5
	stopnow = False
	def __init__(self,host,port,handler,errhandler = False):
		threading.Thread.__init__(self)
		self.host = host
		self.port = port
		self.handler = handler
		self.errhandler = errhandler
	def getHandler(self,client,address):
		return self.handler(client,address)
	def getInputHandler(self,s):
		for i in self.inputHandlers:
			if i.isHandler(s):
				return i
		return False
	def getSocket(self):
		if self.port != 0:
			return socket.socket(socket.AF_INET,socket.SOCK_STREAM)
		return socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
	def run(self):
		if self.port == 0:
			import os
			try:
				os.unlink(self.host)
			except OSError:
				if os.path.exists(self.host):
					raise
		server = self.getSocket()
		
		if self.port != 0:
			try:
				server.bind((self.host,self.port))
			except socket.error:
				if hasattr(self.errhandler,'__call__'):
					self.errhandler()
					return
		else:
			server.bind(self.host)
			os.chmod(self.host,0o777)
		server.listen(self.backlog)
		server.settimeout(5)
		input = [server]
		self.inputHandlers = []
		while not self.stopnow:
			inputready,outputready,exceptready = select.select(input,[],[],5)
			for s in inputready:
				if s == server:
					# handle incoming socket connections
					client, address = server.accept()
					client.settimeout(0.1)
					handler = self.getHandler(client,address)
					if handler.setup():
						self.inputHandlers.append(handler)
						input.append(client)
					else:
						handler.close()
				else:
					# handle other socket connections
					i = self.getInputHandler(s)
					if i:
						try:
							if not i.recieve():
								try:
									i.close()
								except:
									pass
								try:
									s.close()
								except:
									pass
								input.remove(s)
								self.inputHandlers.remove(i)
						except socket.timeout:
							pass
						except Exception as err:
							print(err)
							traceback.print_exc()
							try:
								i.close()
							except:
								pass
							try:
								s.close()
							except:
								pass
							input.remove(s)
							self.inputHandlers.remove(i)
							break
					else:
						s.close()
						input.remove(s)
		for s in input:
			try:
				s.close()
			except:
				pass
		for i in self.inputHandlers:
			try:
				i.close()
			except:
				pass
		if self.port == 0:
			try:
				os.unlink(self.host)
			except OSError:
				if os.path.exists(self.host):
					raise
	def stop(self):
		self.stopnow = True

class SSLServer(Server):
	def __init__(self,host,port,handler,errhandler = False,certfile = '',keyfile = ''):
		Server.__init__(self,host,port,handler,errhandler)
		self.certfile = certfile
		self.keyfile = keyfile
	def getSocket(self):
		import ssl
		s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
		return ssl.wrap_socket(s, keyfile=self.keyfile, certfile=self.certfile, cert_reqs=ssl.CERT_NONE)

class WebSocketHandler(ServerHandler):
	magic = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
	def log(self,s):
		print(s)
	def setup_extra(self):
		return
	def setup(self):
		self.upgraded_to_sockets = False
		self.handshake_done = False
		self.setup_extra()
		self.log('connection established, new web-client')
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
			self.log('nothing to recieve')
			return False
		
		if b1 & 0x0F == 0x8:
			self.log('Client asked to close connection')
			return False
		if not b1 & 0x80:
			self.log('Client must always be masked')
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
		self.log('more headers...')
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
		self.log('Handshaking...')
		
		key = re.search('\n[sS]ec-[wW]eb[sS]ocket-[kK]ey[\s]*:[\s]*([^\r\n]*)\r?\n?',data)
		if key:
			key = key.group(1).strip()
		else:
			self.log('Missing Key!')
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
		return False
