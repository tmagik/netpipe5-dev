#!/usr/bin/python
# This is a test file to build/run the function in C-code

import distutils.core
import os

#distutils.core.DEBUG = 1

modules = []

n = 'tcp'
modules.append( distutils.core.Extension(	
		'NP%s' % n,
		sources = ['src/netpipe.c','src/%s.c' % n],
		depends = ['src/netpipe.h'],
		define_macros = [('TCP',None), 
				('NPNAME', 'NP%s' % n)
			],
#		extra_compile_args = ['-O0', '-ggdb'],
		)
	)

os.environ['CC'] = 'mpicc.mpich2'
os.environ['LDSHARED'] = 'mpicc.mpich2 -shared'

for n in ['mpi']:
	modules.append( distutils.core.Extension(	
		'NP%s' % n,
		sources = ['src/netpipe.c','src/%s.c' % n],
		depends = ['src/netpipe.h'],
		define_macros = [('MPI',None), 
				('NPNAME', 'NP%s' % n)
			],
#		extra_compile_args = ['-O0', '-ggdb'],
		)
	)

for n in ['mpi_atoa']:
	modules.append( distutils.core.Extension(	
		'NP%s' % n,
		sources = ['src/netpipe.c','src/%s.c' % n],
		depends = ['src/netpipe.h'],
		define_macros = [('MPI',None), 
				('COLLECTIVES', None),
				('NPNAME', 'NP%s' % n)
			],
#		extra_compile_args = ['-O0', '-ggdb'],
		)
	)


distutils.core.setup (
	name = 'NetPIPE',
	version = '4.9',
	author = "Troy Benjegerdes",
	url = "http://bitspjoule.org",
	description = 'Network Protocol Independent Performance Evaluator',
	ext_modules = modules)
	

	


#gcc -pthread -fno-strict-aliasing -DNDEBUG -g -O3 -Wall -Wstrict-prototypes -fPIC -I/usr/include/python2.4 -c netpipe.c -o build/temp.linux-i686-2.4/netpipe.o

# gcc -pthread -shared build/temp.linux-x86_64-2.6/src/netpipe.o build/temp.linux-x86_64-2.6/src/mpi.o -L/usr/lib64 -lpython2.6 -o build/lib.linux-x86_64-2.6/NPmpi.so

# cray:
# cc -fPIC -DMPI -I/usr/include/python2.6 -c src/mpi.c -o build/temp.linux-x86_64-2.6/src/mpi.o
# cc -shared build/temp.linux-x86_64-2.6/src/netpipe.o build/temp.linux-x86_64-2.6/src/mpi.o -L/usr/lib64 -lpython2.6 -o build/lib.linux-x86_64-2.6/NPmpi.so

# cc -DMPI -fPIC -I/usr/include/python2.6 -shared src/netpipe.c src/mpi.c -L/usr/lib64 -lpython2.6 -o build/lib.linux-x86_64-2.6/NPmpi.so

# cc -DCOLLECTIVES -DMPI -fPIC -I/usr/include/python2.6 -shared src/netpipe.c src/mpi_atoa.c -L/usr/lib64 -lpython2.6 -o build/lib.linux-x86_64-2.6/NPmpi_atoa.so

