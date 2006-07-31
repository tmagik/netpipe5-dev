#!/usr/bin/env python

#the way this should be is:
# import Netpipe
# modules = Netpipe.modules()
# .. returns list of modules
# hostlist = ['localhost', 'localhost']
# n = Netpipe.tcp(hostlist)  # instantiate TCP module
# n.run_iters(1,1)

import NPtcp

size = 8000000
iters = 10

n = NPtcp.NPtcp()
print n
for i in range(1,5):
	print "--.. n.run_iters(%d,%d)" % (size, iters)
	r = n.run_iters(size,iters)
	print "r: ", r
	bps = size*2/r[3]
	print "receiver: bits per sec: %f * 10e6 -- %f * 1024*1024" % (bps/1000000, bps/(1024*1024))
