#!/usr/bin/env python

#the way this should be is:
# import Netpipe
# modules = Netpipe.modules()
# .. returns list of modules
# hostlist = ['localhost', 'localhost']
# n = Netpipe.tcp(hostlist)  # instantiate TCP module
# n.run_iters(1,1)

import NPtcp
import sys

size = 8000000
iters = 10

if len(sys.argv) != 2:
	print "useage: test-s.py [hostname of receiver]"
	sys.exit(1)

n = NPtcp.NPtcp(sys.argv[1])
print n
for i in range(1,5):
	print "--.. n.run_iters(%d,%d)" % (size, iters)
	r = n.run_iters(size,iters)
	print "r: ", r
	bps = size*2/r[3]
	print "sender: bits per sec: %f * 10e6 -- %f * 1024*1024" % (bps/1000000, bps/(1024*1024))
