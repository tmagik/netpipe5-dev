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

static PyObject *
netpipe_run_nrepeat(PyObject *self, PyObject *pyargs)
{
	uint32_t microseconds, t0, nrepeat, size, j;
	double time;
	ArgStruct *args = self->args;

	int integCheck = 0;  /* KILL ME */

	if (!PyArg_ParseTuple(pyargs, "i, i", &size, &nrepeat))
		return NULL;

	fprintf("size: %d, nrepeats: %d\n", size, nrepeat);
	
	/* buffer(s) should be allocated and preposted (if desired).
	 * we just call SendData/RecvData, and return timing info
	 */
	
	Sync(&args);    /* Sync to prevent timing artifacts and
			   race condition in armci module */

	
	for (j = 0; j < nrepeat; j++)
	{
		/* This should be a function pointer ??*/
		if (integCheck){
			/* take nanosecond timestamp..  (ns_timestamp) */
			SetIntegrityData(&args);
			/* ns_timestamp */
		}
			
		if (args.tr){
			SendData(&args); /* this is what matters */
			/* ns_timestamp */
			RecvData(&args); /* Wait for it to come back */
		else{
			RecvData(&args); /* this is what matters */
			/* ns_timestamp */
			SendData(&args); /* Wait for it to come back */
		}
		/* ns_timestamp */
		
		if (integCheck){
			VerifyIntegrity(&args);
			/* ns_timestamp */
		}
		
		if(!args.cache){
			AdvanceRecvPtr(&args, len_buf_align);
		  	AdvanceSendPtr(&args, len_buf_align);
		}
	}

	/* t is the 1-directional trasmission time */

	microseconds = When() - t0
	time = microseconds / nrepeat;


	return Py_BuildValue("(i, i, i, d)", 
			size, nrepeat, microseconds, time);
}


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

/* This is generic across all modules */
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
    
    if(!p->cache) /* Allocate second buffer if limiting cache */
      if((p->s_buff=(char *)malloc(bufflen+soffset))==(char *)NULL)
      {
          fprintf(stderr,"couldn't allocate memory for send buffer\n");
          exit(-1);
      }
}

void FreeBuff(char *buff1, char *buff2)
{
  if(buff1 != NULL)

   free(buff1);


  if(buff2 != NULL)

   free(buff2);
}

#endif
