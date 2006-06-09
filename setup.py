# This is a test file to build/run the function in C-code

import distutils.core

#distutils.core.DEBUG = 1

nptcp = distutils.core.Extension(	
			'NPtcp',
			sources = ['src/netpipe.c','src/tcp.c'],
			depends = ['src/netpipe.h'],
#			include_dirs = [''],
			define_macros = [('TCP',None)],
#			extra_compile_args = ['-O0', '-ggdb'],
			)

distutils.core.setup (
	name = 'NetPIPE',
	version = '4.9',
	description = 'Network Protocol Independent Performance Evaluator',
	ext_modules = [nptcp])
	

	


#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -O3 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.4 -c netpipe.c -o build/temp.linux-i686-2.4/netpipe.o


