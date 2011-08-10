/*****************************************************************************/
/* "NetPIPE" -- Network Protocol Independent Performance Evaluator.          */
/* Copyright 1997, 1998, 2006 Iowa State University Research Foundation, Inc.*/
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation.  You should have received a copy of the     */
/* GNU General Public License along with this program; if not, write to the  */
/* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.   */
/*                                                                           */
/*****************************************************************************/

#include "netpipe.h"
#include <Python.h>

/* NOTE:
 * We need to hack together some sort of clean-up utility because we're dealing
 * with a language which has a garbage collecter, the interfaces must be
 * somehow closed prior to exiting!
 */

static PyObject *
netpipe_run_iters(Netpipe *self, PyObject *pyargs)
{
	uint32_t nrepeat, size, j, bytes;
	double time, t0, t1; /* do this using 64 bit ints or something */
	ArgStruct * args;

	args = &self->args;

	if (!PyArg_ParseTuple(pyargs, "ii", &size, &nrepeat)){
		if(args->rcv){
			RecvRepeat(args, &size);
			RecvRepeat(args, &nrepeat);
			if (size == 0){ /* signal to exit */
				return Py_BuildValue("None");
			}
		} else { /* bail out */
			return NULL;
		}
	}

	if (args->tr){
		SendRepeat(args, size);
		SendRepeat(args, nrepeat);
	}

	bytes = size / 8;
	if (size % 8) bytes = bytes + 1;
	
#if DEBUG
	fprintf(stderr, "size: %d, bytes: %d, nrepeats: %d\n", (int)size, (int)bytes, (int)nrepeat);
#endif	
	

	/* XXX TODO: clean up MyMalloc & other stuff for collectives nicely */
	//MyMalloc(ArgStruct *p, int bufflen, int ssoffset, int roffset
	args->bufflen = bytes;
	MyMalloc(args, args->bufflen, 0, 0);
	if(args->cache)	args->s_buff = args->r_buff;
	args->r_ptr = args->r_buff_orig = args->r_buff;
	args->s_ptr = args->s_buff_orig = args->s_buff;

	if (args->r_buff_orig == NULL || args->s_buff_orig == NULL){
		fprintf(stderr, "couldn't allocate memory\n");
		return PyErr_NoMemory();
	}
/*	fprintf(stderr, "address is %p\n", buffer);	 */
	
	//InitBufferData(args, args->bufflen, args->soffset, args->roffset);
	InitBufferData(args, args->bufflen, 0,0);

	AfterAlignmentInit(args);  /* MPI-2 needs this to create a window */

	/* Infiniband requires use of asynchronous communications, so we need
	 * the PrepareToReceive calls below
	 */
	if( self->asyncReceive )
		PrepareToReceive(args);

	Sync(args);    /* Sync to prevent timing artifacts and
			   race condition in armci module */

	t0 = When();
	for (j = 0; j < nrepeat; j++)
	{
#if 0
		/* This should be a function pointer ??*/
		if (self->integCheck){
			/* take nanosecond timestamp..  (ns_timestamp) */
			SetIntegrityData(args);
			/* ns_timestamp */
		}
#endif		
		if (args->tr){
			SendData(args); /* this is what matters */
			/* ns_timestamp */
			RecvData(args); /* Wait for it to come back */
		} 
		else{
			RecvData(args); /* Wait for data */
			/* ns_timestamp */ 
			SendData(args); /* bounce it back */
		}
		/* ns_timestamp */
#if 0		
		if (self->integCheck){
			VerifyIntegrity(args);
			/* ns_timestamp */
		}
		
		if(!args->cache){
			AdvanceRecvPtr(args, self->len_buf_align);
		  	AdvanceSendPtr(args, self->len_buf_align);
		}
#endif
	}

	/* t is the 1-directional transmission time */

	t1 = When() - t0;
	time = t1 / nrepeat;

	/* for now, free the buffer.. later be more intelligent */
	/* ... probably should use *_buff_orig */
	if(args->cache)
		FreeBuff(args->s_buff_orig, NULL);
	else {
		FreeBuff(args->s_buff_orig, args->r_buff_orig);
	}
	
	return Py_BuildValue("(i, i, d, d)", 
			size, nrepeat, t1, time);
}

/* Initialize a new netpipe object */
static PyObject *netpipe_object(PyObject *self, 
		PyObject *pyargs, PyObject *kw)
{
	/* need to parse for hostnames */
	Netpipe *newobj;
	char * temp = NULL;

	static char *kwlist[] = {"host", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(pyargs, kw, "|s", kwlist, &temp))
		return NULL;
	
	newobj = PyObject_New(Netpipe, &NetpipeType);
	if (newobj != NULL){
	
		memset(&newobj->args, 0, sizeof(Netpipe)-sizeof(dummy_pyobject_size));

		newobj->bufalign = 16*1024; /* 16k buffer alignment */
		
		strcpy(newobj->s, "np.out");

		if(temp){
			if(strlen(temp) > 254){
				fprintf(stderr, "XXXXX fail: host string too long\n");
			}
			strncpy(&newobj->args.host, temp, 255);
			fprintf(stderr, "transmit, connecting to %s\n",newobj->args.host);
                        newobj->args.tr = 1;			
		}

		/* grab argc/argv */
		PyObject *sys = PyImport_ImportModule("sys");
		PyObject *pyargv = PyObject_GetAttrString(sys, "argv");
		int argc = PyList_Size(pyargv);
#ifdef DEBUG
		fprintf(stderr, "init obj argc: %d\n", argc);
#endif	
		char **argv = malloc(sizeof(char *) * argc);
		int i;
		for (i = 0; i< argc; i++){
			PyObject *pyarg = PyList_GetItem(pyargv, i);
			argv[i] = PyString_AsString(pyarg);
#ifdef DEBUG
			fprintf(stderr, "argv[%d]: %s\n", i, argv[i]);			
#endif
		}

		/* only set things that are not 0 */
                //newobj->args.cache = 1; /* Default to use cache */

		Init(newobj, &argc, &argv);
		
		Setup(&(newobj->args));
		return (PyObject *)newobj;
	}
	return PyErr_NoMemory();
}

#define NPstring(name) _NPstring(name)
#define _NPstring(name) #name
static PyMethodDef TestMethods[] = {
	{NPstring(NPNAME),
		(PyCFunction)netpipe_object,
		METH_VARARGS | METH_KEYWORDS,
		"TEST!!"},
	{NULL, NULL, 0, NULL}
};

/* 
 * Module initialization, this will handle creating the Netpipe object, which
 * will be exported out to the main python script which is in charge of 
 * doing all of the runs/iterations.  This will also be the place-holder for
 * all of the Initialization functions and Startup functions required to get
 * the program into a state where we can call Send/Recv() functions.
 */

#define _initmacro(x) init ## x
#define initmacro(x) _initmacro(x)

PyMODINIT_FUNC
initmacro(NPNAME) (void)
{
	//PyObject * module;
	
	if(PyType_Ready(&NetpipeType) < 0)
		return;
	
	/* we must do init/startup first, before allowing the module to actually
	 * be instantiated.
	 */

	/* NOTE: Setup() needs an ArgStruct parameter, so we will need to parse
	 * everything before this point!
     */
	(void) Py_InitModule(NPstring(NPNAME), TestMethods);
}

static PyObject * Netpipe_get(Netpipe * self, void *closure)
{
	char * op = (char *)closure;
	PyObject * attr;

	/* um, this is dumb */
	if (strcmp(op, "streamopt") == 0) {
		attr = PyInt_FromLong(self->streamopt);
		return attr;
	}

	if (strcmp(op, "tr") == 0) {
		attr = PyInt_FromLong(self->args.tr);
		return attr;
	}

	/* if execution gets here, this is an error */
	PyErr_SetString(PyExc_RuntimeError, "request for unknown Netpipe attribute");
	return NULL;
}

static void Netpipe_dealloc(Netpipe *self)
{
	fprintf(stderr, "dealloc netpipe object %p", self);

	self->ob_type->tp_free((PyObject *)self);
}
    
static PyMethodDef Netpipe_methods[] = {
	{"run_iters",	(PyCFunction)netpipe_run_iters,
		METH_VARARGS,	 "Run message size m iters times"},
	{NULL} /* Sentinel */
};

static PyGetSetDef Netpipe_getsets[] = {
	{"streamopt", (getter)Netpipe_get, NULL, 
			"Streaming mode flag", "streamopt"},
	{"tr", (getter)Netpipe_get, NULL, 
			"We are transmitter", "tr"},
	{NULL} /* Sentinel */
};

PyTypeObject NetpipeType = {
	PyObject_HEAD_INIT(NULL)
	0,												/* ob_size				*/
	"Netpipe",										/* tp_name				*/
	sizeof(Netpipe),								/* tp_basicsize			*/
	0,												/* tp_itemsize 			*/
	(destructor)Netpipe_dealloc,					/* tp_dealloc 			*/
	0,												/* tp_print 			*/
	0,												/* tp_getattr			*/
	0,												/* tp_setattr			*/
	0,												/* tp_compare			*/
	0,												/* tp_repr				*/
	0,												/* tp_as_number			*/
	0,												/* tp_as_sequence		*/
	0,												/* tp_as_mapping		*/
	0,												/* tp_hash				*/
	0,												/* tp_call				*/
	0,												/* tp_str				*/
	0,												/* tp_getattro			*/
	0,												/* tp_setattro			*/
	0,												/* tp_as_buffer			*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,		/* tp_flags				*/
	"NetPIPE object.",								/* __doc__				*/
	0,												/* tp_traverse 			*/
	0,												/* tp_clear 			*/
	0,												/* tp_richcompare 		*/
	0,												/* tp_weaklistoffset	*/
	0,												/* tp_iter				*/
	0,												/* tp_iternext 			*/
	Netpipe_methods,								/* tp_methods 			*/
	0,												/* tp_members 			*/
	Netpipe_getsets,								/* tp_getset 			*/
};



/* Return the current time in seconds, using a double precision number.      */
double When()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return ((double) tp.tv_sec + (double) tp.tv_usec * 1e-6);
}

/* 
 * The mymemset() function fills the first n integers of the memory area 
 * pointed to by ptr with the constant integer c. 
 */
void mymemset(int *ptr, int c, int n)  
{
    int i;

    for (i = 0; i < n; i++) 
		*(ptr + i) = c;
}

/* Read the first n integers of the memmory area pointed to by ptr, to flush  
 * out the cache   
 */
void flushcache(int *ptr, int n)
{
   static int flag = 0;
   int    i; 

   flag = (flag + 1) % 2; 
   if ( flag == 0) 
       for (i = 0; i < n; i++)
           *(ptr + i) = *(ptr + i) + 1;
   else
       for (i = 0; i < n; i++) 
           *(ptr + i) = *(ptr + i) - 1; 
    
}

/* For integrity check, set each integer-sized block to the next consecutive
 * integer, starting with the value 0 in the first block, and so on.  Earlier
 * we made sure the memory allocated for the buffer is of size i*sizeof(int) +
 * 1 so there is an extra byte that can be used as a flag to detect the end
 * of a receive.
 */
void SetIntegrityData(ArgStruct *p)
{
  int i;
  int num_segments;

  num_segments = p->bufflen / sizeof(int);

  for(i=0; i<num_segments; i++) {

    *( (int*)p->s_ptr + i ) = i;

  }
}

void VerifyIntegrity(ArgStruct *p)
{
  int i;
  int num_segments;
  int integrityVerified = 1;

  num_segments = p->bufflen / sizeof(int);

  for(i=0; i<num_segments; i++) {

    if( *( (int*)p->r_ptr + i )  != i ) {

      integrityVerified = 0;
      break;

    }

  }


  if(!integrityVerified) {
    
    fprintf(stderr, "Integrity check failed: Expecting %d but received %d\n",
            i, *( (int*)p->r_ptr + i ) );

    /* Dump argstruct */
    /*
    fprintf(stderr, " args struct:\n");
    fprintf(stderr, "  r_buff_orig %p [%c%c%c...]\n", p->r_buff_orig, p->r_buff_orig[i], p->r_buff_orig[i+1], p->r_buff_orig[i+2]);
    fprintf(stderr, "  r_buff      %p [%c%c%c...]\n", p->r_buff,      p->r_buff[i],      p->r_buff[i+1],      p->r_buff[i+2]);
    fprintf(stderr, "  r_ptr       %p [%c%c%c...]\n", p->r_ptr,       p->r_ptr[i],       p->r_ptr[i+1],       p->r_ptr[i+2]);
    fprintf(stderr, "  s_buff_orig %p [%c%c%c...]\n", p->s_buff_orig, p->s_buff_orig[i], p->s_buff_orig[i+1], p->s_buff_orig[i+2]);
    fprintf(stderr, "  s_buff      %p [%c%c%c...]\n", p->s_buff,      p->s_buff[i],      p->s_buff[i+1],      p->s_buff[i+2]);
    fprintf(stderr, "  s_ptr       %p [%c%c%c...]\n", p->s_ptr,       p->s_ptr[i],       p->s_ptr[i+1],       p->s_ptr[i+2]);
    */
    exit(-1);

  }

}  
    
void* AlignBuffer(void* buff, int boundary)
{
  if(boundary == 0)
    return buff;
  else
    /* char* typecast required for cc on IRIX */
    return ((char*)buff) + (boundary - ((unsigned long)buff % boundary) );
}

void AdvanceSendPtr(ArgStruct* p, int blocksize)
{
  /* Move the send buffer pointer forward if there is room */

  if(p->s_ptr + blocksize < p->s_buff + MEMSIZE - blocksize)
    
    p->s_ptr += blocksize;

  else /* Otherwise wrap around to the beginning of the aligned buffer */

    p->s_ptr = p->s_buff;
}

void AdvanceRecvPtr(ArgStruct* p, int blocksize)
{
  /* Move the send buffer pointer forward if there is room */

  if(p->r_ptr + blocksize < p->r_buff + MEMSIZE - blocksize)
    
    p->r_ptr += blocksize;

  else /* Otherwise wrap around to the beginning of the aligned buffer */

    p->r_ptr = p->r_buff;
}

void SaveRecvPtr(ArgStruct* p)
{
  /* Typecast prevents warning about loss of volatile qualifier */

  p->r_ptr_saved = (void*)p->r_ptr; 
}

void ResetRecvPtr(ArgStruct* p)
{
  p->r_ptr = p->r_ptr_saved;
}

/* This is generic across all point to point modules */
#if !defined(COLLECTIVES)
void InitBufferData(ArgStruct *p, int nbytes, int soffset, int roffset)
{
  memset(p->r_buff, 'a', nbytes+MAX(soffset,roffset));

  /* If using cache mode, then we need to initialize the last byte
   * to the proper value since the transmitter and receiver are waiting
   * on different values to determine when the message has completely
   * arrive.
   */   
  if(p->cache)

    p->r_buff[(nbytes+MAX(soffset,roffset))-1] = 'a' + p->tr;

  /* If using no-cache mode, then we have distinct send and receive
   * buffers, so the send buffer starts out containing different values
   * from the receive buffer
   */
  else

    memset(p->s_buff, 'b', nbytes+soffset);
}

#if !defined(OPENIB) && !defined(INFINIBAND) && !defined(ARMCI) && !defined(LAPI) && !defined(GPSHMEM) && !defined(SHMEM) && !defined(GM) 

void MyMalloc(ArgStruct *p, int bufflen, int soffset, int roffset)
{
	if((p->r_buff=(char *)malloc(bufflen+MAX(soffset,roffset)))==(char *)NULL)
	{
		fprintf(stderr,"couldn't allocate memory for receive buffer\n");
		exit(-1);
	}
	/* if pcache==1, use cache, so this line happens only if flushing cache */

	if(!p->cache) { /* Allocate second buffer if limiting cache */ 
		if((p->s_buff=(char *)malloc(bufflen+soffset))==(char *)NULL) {
			fprintf(stderr,"couldn't allocate memory for send buffer\n");
			exit(-1);
		}
	}
#ifdef DEBUG
	fprintf(stderr, "MyMalloc: p->r_buff %p, p->s_buff %p\n", p->r_buff, p->s_buff);
#endif
}

void FreeBuff(char *buff1, char *buff2)
{
#ifdef DEBUG
	fprintf(stderr, "FreeBuff(buff1 %p, buff2 %p)\n", buff1, buff2);
#endif
	if(buff1 != NULL)
		free(buff1);
	if(buff2 != NULL)
		free(buff2);
}

#endif /* COLLECTIVES */
#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=4 sts=4 sw=4 noexpandtab
 */
