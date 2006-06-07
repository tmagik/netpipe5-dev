# This is a test file to build/run the function in C-code

from distutils.core import setup, Extension

nptcp = Extension(	'NPtcp',
			sources = ['src/netpipe.c','src/tcp.c'],
#			include_dirs = [''],
			define_macros = [('TCP',None)],
			extra_compile_args = ['-ggdb'],
			)

setup (name = 'NetPIPE',
	version = '4.9',
	description = 'Network Protocol Independent Performance Evaluator',
	ext_modules = [nptcp])
	

	


#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -O3 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.4 -c netpipe.c -o build/temp.linux-i686-2.4/netpipe.o


