#!/usr/bin/env python

#the way this should be is:
# import Netpipe
# modules = Netpipe.modules()
# .. returns list of modules
# hostlist = ['localhost', 'localhost']
# n = Netpipe.tcp(hostlist)  # instantiate TCP module
# n.run_iters(1,1)

import NPtcp

n = NPtcp.NPtcp()
print n
print "--.. n.run_iters(1,1)"
r = n.run_iters(1000000,1000)
print "r: ", r
