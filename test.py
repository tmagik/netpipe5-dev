#!/usr/bin/python

import os
os.system("python setup.py build")


#the way this should be is:
# import Netpipe
# modules = Netpipe.modules()
# .. returns list of modules
# hostlist = ['localhost', 'localhost']
# n = Netpipe.tcp(hostlist)  # instantiate TCP module
# n.run_iters(1,1)

import NPtcp

n = NPtcp.NPtcp()

n.run_iters(1,1)
