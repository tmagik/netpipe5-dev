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

import NPtcp
import threading
import Queue

hostname = 'localhost'

class TransmitThread( threading.Thread ):
	def run (self):
		netpipe = NPtcp.NPtcp(hostname)
		n = 0
		while True:
			args = trQueue.get()
			if args == None:
				print "Transmit thread exiting"
				break	# go bye-bye

			n = n + 1
			(size, iters) = args
			print "T: %3d: %7d bytes %6d times --> " % (n, size, iters)
			r = netpipe.run_iters(size, iters)
			print "T:   \----> %s" % r

class RecvThread( threading.Thread ):
	def run(self):
		netpipe = NPtcp.NPtcp()
		n = 0
		while True:
			args = rcvQueue.get()
			if args == None:
				print "Receive thread exiting"
				break	# go bye-bye

			n = n + 1
			(size, iters) = args
			print "R: %3d: %7d bytes %6d times --> " % (n, size, iters)
			r = netpipe.run_iters(size, iters)
			print "R:   \----> %s" % r


rcvQueue = Queue.Queue (0)
trQueue = Queue.Queue (0)

TransmitThread().start()
RecvThread().start()

iters = 1000
for size in [1, 2000, 10000]:
	rcvQueue.put( (size, iters) )
	trQueue.put( (size, iters) )

rcvQueue.put(None)
trQueue.put(None)

# Local variables:
#  c-indent-level: 4
#  c-basic-offset: 4
# End:
#
# vim: ts=4 sts=4 sw=4 noexpandtab

