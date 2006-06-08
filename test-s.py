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

n = NPtcp.NPtcp('localhost')
print n
print "--.. n.run_iters(1,1)"
n.run_iters(1,1)
