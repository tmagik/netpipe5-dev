#!/usr/bin/env python

import os, sys
build = 1
if build:
	if os.system("python setup.py build"):
		sys.exit()
	import distutils.util
	platform = distutils.util.get_platform()
	version = '%d.%d' % (sys.version_info[0], sys.version_info[1])
	# probably unixism here
	sys.path.append ("%s/build/lib.%s-%s" % (os.getcwd(), platform, version))
	print sys.path


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
import timeit
import decimal

#duplicate NP3 constants from netpipe.h
STOPTM = 0.5
#ifdef FINAL
#TRIALS = 7
#RUNTM =  0.25
#else
TRIALS = 3
RUNTM = 0.10

hostname = 'localhost'
# 0: normal   1: don't call netpipe.run_iters 2: don't instantiate object
dummy = 0
port = 5000
'''Functions that are used in this demo'''
####################################################################################################################
'''increment function'''
def incFunc(n, inc):
	inc = inc/8
	if n <= 2:
		inc = 2
	elif 3 <= n <= 5:
		inc = 4
	elif 6 <= n <= 8:
		inc = 7
	elif 9 <= n <= 14:
		inc = 8
	else:
		if n > 2:
			if n%2: # n is odd
				inc = inc + inc
			else:  # n is even
				inc = inc
	return inc*8

'''size function'''
def sizeFunc(size, pert):
	size = size + pert
	return size

def pertFunc(n, inc, firstpert):
	''' We implement the following NP3 C code in a rather different way
	           /* Exponentially increase the block size.  */

       if (nq > 2) inc = ((nq % 2))? inc + inc: inc;
       
          /* This is a perturbation loop to test nearby values */

       for (pert = ((perturbation > 0) && (inc > perturbation+1)) ? -perturbation : 0;
            pert <= perturbation; 
            n++, pert += ((perturbation > 0) && (inc > perturbation+1)) ? perturbation : perturbation+1)
       {
        '''
	if n <= 2:
		pertList = [-8,0,8]
	elif 3 <= n <= 5:
		pertList = [-16,0,16]
	elif 6 <= n <= 8:
		pertList = [-8,0,24]
	elif 9 <= n <= 14:
		pertList = [-16,0,24]
	else:
		pertList = [firstpert,0,24]
	return pertList
	
def iterFunc(tlast, oldsize, newsize):
	'''duplicate netpipe.c code
		nrepeat = MAX((RUNTM / ((double)args.bufflen /
                                  (args.bufflen - inc + 1.0) * tlast)),TRIALS);
	'''
	iters = int(RUNTM*oldsize/(tlast*newsize))
	#iters = max(int((RUNTM*bw)/(size)), TRIALS)	
	return max(iters, TRIALS)

fileName = sys.argv[1]
f = file(fileName,'w')
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
		if not dummy:  
			r = netpipe.run_iters(size, iters)

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
	'''Variables that go in this loop'''
	n = inc = totalTime = megaCount = permSize = pertInc =  firstPert = pertCount = 0
	iters = 1000
	count = 1
	pertList = [-1,0,1]
	size = size_r = 8
	rate = 0.3*(1e6)
	iterationTime = 25/(1e6)

	'''main loop: using a counter as a temporary switch'''	
	while iterationTime < STOPTM:
		previousInc = inc
		if count%6==0:
			pertList[0] = -inc+pert[0]
		inc = incFunc(n,inc)
		permSize = permSize + inc
		pert = pertFunc(n, previousInc, pertList[0])
		pertList = pert
		for pertrun in range(3):
			oldsize = size
			size = permSize + pert[pertrun]
			iters =  iterFunc(iterationTime, oldsize, size)
			#three runs of same size packet and take the lowest time of the runs
			ttimes = []
			itimes = []
			sys.stdout.write("%3d: %7d bytes %6d times" % (n, int(size/8), iters))
			sys.stdout.flush()
			for i in range(TRIALS):
				args = (size, iters)
				txt = pickle.dumps(args)
				sock.send( txt )
				args2 = pickle.loads(sock.recv(1024))
				if args != args2:
					print "error in sync, exiting!"
					sock.send( pickle.dumps (None))
					sys.exit(1);
				r = netpipe.run_iters(size, iters)
		     		(size_r, iters_r, tTime, iTime) = r
				ttimes.append(tTime)
				itimes.append(iTime)
				sys.stdout.write("-")
				sys.stdout.flush()
			totalTime = min(ttimes)
			iterationTime = min(itimes)
			totalTime = min(ttimes) 
			# iterationTime is total round trip time, divide by 2 for one-way time
			onewayTime = iterationTime/2
			# multiply by 2 because we sent size * iterations * 2
			rate = ((size_r*iters_r*2)/(totalTime))
			output = "%12d %15.2lf %12.8lf %8d" % (int(size_r/8),rate/(1e6),onewayTime,iters_r)
			for i in range(TRIALS):
				output = output + " %12.9lf" % (ttimes[i])
			f.write(output + '\n')
			sys.stdout.write("> %8.2lf Mbps in %10.2lf usec\n" % ( rate/(1e6),onewayTime*(1e6) ))
			n = n + 1	
			if n >= 11:
				count = count + 1
	f.close()
	sock.send( pickle.dumps (None))

	print "T: os.wait: ", os.wait()

# Local variables:
#  c-indent-level: 4
#  c-basic-offset: 4
# End:
#
# vim: ts=4 sts=4 sw=4 noexpandtab

