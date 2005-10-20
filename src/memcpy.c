/*****************************************************************************/
/* "NetPIPE" -- Network Protocol Independent Performance Evaluator.          */
/* Copyright 1997, 1998 Iowa State University Research Foundation, Inc.      */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation.  You should have received a copy of the     */
/* GNU General Public License along with this program; if not, write to the  */
/* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.   */
/*                                                                           */
/*     * memcpy.c           ---- single process memory copy                  */
/*****************************************************************************/
#include    "netpipe.h"
#undef SPLIT_MEMCPY

void Init(ArgStruct *p, int* pargc, char*** pargv)
{
  /* Print out message about results */

  printf("\n");
  printf("  *** Note about memcpy module results ***  \n");
  printf("\n");
  printf("The memcpy module is sensitive to the L1 and L2 cache sizes,\n" \
         "the size of the cache-lines, and the compiler.  The following\n" \
         "may help to interpret the results:\n" \
         "\n" \
         "* With cache effects and no perturbations (NPmemcpy -p 0),\n" \
         "    the plot will show 2 peaks.  The first peak is where data is\n" \
         "    copied from L1 cache to L1, peaking around half the L1 cache\n" \
         "    size.  The second peak is where data is copied from the L2 cache\n" \
         "    to L2, peaking around half the L2 cache size.  The curve then\n" \
         "    will drop off as messages are copied from RAM through the caches\n" \
         "    and back to RAM.\n" \
         "\n" \
         "* Without cache effects and no perturbations (NPmemcpy -I -p 0).\n" \
         "    Data always starts in RAM, and is copied through the caches\n" \
         "    up in L1, L2, or RAM depending on the message size.\n"\
         "\n" \
         "* Compiler effects (NPmemcpy)\n" \
         "    The memcpy() function in even current versions of glibc is\n"\
         "    poorly optimized.  Performance is great when the message size\n" \
         "    is divisible by 4 bytes, but other sizes revert to a byte-by-byte\n" \
         "    copy that can be 4-5 times slower.  This produces sharp peaks\n" \
         "    in the curve that are not seen using other compilers.\n" \
         );
  printf("\n");

  p->tr = 1;
  p->rcv = 0;
}

void Setup(ArgStruct *p)
{
}   

void Sync(ArgStruct *p)
{
}

void PrepareToReceive(ArgStruct *p)
{
}

void SendData(ArgStruct *p)
{
    int nbytes = p->bufflen, nleft;
    char *src = p->s_ptr, *dest = p->r_ptr;

#ifndef SPLIT_MEMCPY

    memcpy(dest, src, nbytes);

#else

/* Alternately try splitting the memcpy to copy the body then the
 * remainder that is not divisible by 8 bytes.  glibc memcpy under
 * RedHat Linux is less efficient if the size is not divisible by 4 bytes.
 */

    nleft = nbytes%8;
    nbytes -= nleft;

    memcpy(dest, src, nbytes);

    if( nleft > 0 ) {

        src  += nbytes;
        dest += nbytes;

        memcpy(dest, src, nleft);

    }
#endif
}

void RecvData(ArgStruct *p)
{
    int nbytes = p->bufflen, nleft;
    char *src = p->s_ptr, *dest = p->r_ptr;

#ifndef SPLIT_MEMCPY

    memcpy(src, dest, nbytes);

#else

/* Alternately try splitting the memcpy to copy the body then the
 * remainder that is not divisible by 8 bytes.  glibc memcpy under
 * RedHat Linux is less efficient if the size is not divisible by 4 bytes.
 */

    nleft = nbytes%8;
    nbytes -= nleft;

    memcpy(src, dest, nbytes);

    if( nleft > 0 ) {

        src  += nbytes;
        dest += nbytes;

        memcpy(src, dest, nleft);

    }
#endif
}

void SendTime(ArgStruct *p, double *t)
{
}

void RecvTime(ArgStruct *p, double *t)
{
}

void SendRepeat(ArgStruct *p, int rpt)
{
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
}

void CleanUp(ArgStruct *p)
{
}

void FreeBuff(char *buff1, char *buff2)
{
  if(buff1 != NULL)
    free(buff1);

  if(buff2 != NULL)
    free(buff2);
}

void MyMalloc(ArgStruct *p, int bufflen)
{
    if((p->r_buff=(char *)malloc(bufflen))==(char *)NULL)
    {
        fprintf(stderr,"couldn't allocate memory for receive buffer\n");
        exit(-1);
    }

    if(!p->cache)
      if((p->s_buff=(char *)malloc(bufflen))==(char *)NULL)
      {
          fprintf(stderr,"Couldn't allocate memory for send buffer\n");
          exit(-1);
      }
}

void Reset(ArgStruct *p)
{

}

void InitBufferData(ArgStruct *p, int nbytes)
{
  memset(p->r_buff, 'a', nbytes);

  if(!p->cache)
    memset(p->s_buff, 'b', nbytes);
}

void AfterAlignmentInit(ArgStruct *p)
{

}
