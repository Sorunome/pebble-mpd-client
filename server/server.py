#!/usr/bin/python3
## -*- coding: utf-8 -*-

# Server framework (c) Sorunome
# published under GPL 3 or later


import threading,socket,select,traceback

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
