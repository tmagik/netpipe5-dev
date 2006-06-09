#!/usr/bin/env python

import os, sys
if os.system("python setup.py build"):
	sys.exit()

#the way this should be is:
# import Netpipe
# modules = Netpipe.modules()
# .. returns list of modules
# hostlist = ['localhost', 'localhost']
# n = Netpipe.tcp(hostlist)  # instantiate TCP module
# n.run_iters(1,1)

import socket
import pickle
import sys
import NPtcp
import time

hostname = 'localhost'
# 0: normal   1: don't call netpipe.run_iters 2: don't instantiate object
dummy = 0
port = 5000

pid = os.fork()
if not pid:
	sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
	sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
	sock.bind (('', port))
	sock.listen(5)
	channel, details = sock.accept()
	if dummy < 2:
		netpipe = NPtcp.NPtcp()
	# we are receiver
	n = 0
	while True:
		txt = channel.recv(1024)
		args = pickle.loads(txt)
		channel.send(txt) #send it back
		if args == None:
			print "Receive thread exiting"
			break	# go bye-bye

		n = n + 1
		(size, iters) = args
		print "R: %3d: %7d bytes %6d times --> " % (n, size, iters)
		if not dummy:
			r = netpipe.run_iters(size, iters)
			print "R:   \----> ", r

	channel.close()
	sys.exit(0)

else:

	sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
	sock.connect((hostname, port))
	#this is a HACK so other process executes before we do..
	time.sleep(0.5)
	if dummy < 2:
		netpipe = NPtcp.NPtcp(hostname)
	print "NPmodule loaded"
	n = 0
	iters = 100000
	for size in [1, 2000, 10000]:
		args = (size, iters)
		txt = pickle.dumps(args)
		sock.send( txt )
		args2 = pickle.loads(sock.recv(1024))
		if args != args2:
			print "error in sync, exiting!"
			sock.send( pickle.dumps (None))
			sys.exit(1);
		n = n + 1
		print "T: %3d: %7d bytes %6d times --> " % (n, size, iters)
		if not dummy: 
			r = netpipe.run_iters(size, iters)
			print "T:   \----> ", r

	sock.send( pickle.dumps (None))

	print "T: os.wait: ", os.wait()

# Local variables:
#  c-indent-level: 4
#  c-basic-offset: 4
# End:
#
# vim: ts=4 sts=4 sw=4 noexpandtab

