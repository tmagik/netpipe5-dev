/* Netpipe module for mpi-2 one-sided communications by Adam Oline */
#include "netpipe.h"
#include <mpi.h>

MPI_Win win;
char* buf_orig = NULL;

int Init(ArgStruct *p, int* pargc, char*** pargv)
{
  p->prot.use_get = 0;  /* Default to put   */
  p->prot.no_fence = 0; /* Default to fence */

  MPI_Init(pargc, pargv);
}

int Setup(ArgStruct *p)
{
  int nproc;

  MPI_Comm_rank(MPI_COMM_WORLD, &p->prot.iproc);

  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (nproc % 2 != 0)
    {
      printf("Need <multiple of two> processes\n");
      exit(-2);
    }

  {
    char s[255], *ptr;
    gethostname(s,253);
    if( s[0] != '.' ) {                 /* just print the base name */
      ptr = strchr( s, '.');
      if( ptr != NULL ) *ptr = '\0';
    }
    printf("%d: %s\n",p->prot.iproc,s);
    fflush(stdout);
  }

  /* TODO: Finish changing netpipe such that it can run with > 2 procs */
  /* 0 <--> (nproc - 1)
   * 1 <--> (nproc - 2)
   * ...
   */
 
  p->prot.nbor = (nproc - 1) - p->prot.iproc;

  if (p->prot.iproc % 2 == 0) /* Even procs transmit */
    p->tr = 1;
  else
    p->tr = 0;

  return 0;
}

void Sync(ArgStruct *p)
{
  MPI_Win_fence(0, win);
}

void PrepareToReceive(ArgStruct *p)
{

}

void SendData(ArgStruct *p)
{
  int buf_offset=0;
  
  if(buf_orig != NULL) /* Only true if -c was not specified on cmd-line */
    buf_offset = p->buff - buf_orig; /* offset to the next memory block */

  if( p->prot.use_get )
    MPI_Get(p->buff, p->bufflen, MPI_BYTE, p->prot.nbor, buf_offset, 
            p->bufflen, MPI_BYTE, win);
  else
    MPI_Put(p->buff, p->bufflen, MPI_BYTE, p->prot.nbor, buf_offset, 
            p->bufflen, MPI_BYTE, win);

  if (p->prot.no_fence == 0)
    MPI_Win_fence(0, win);

}

void RecvData(ArgStruct *p)
{
  /* If user specified 'no fence' option on cmd line, then we try to bypass
     the fence call by waiting for the last byte to arrive.  The MPI-2
     standard does not require any data to be written locally until a
     synchronization call (such as fence) occurs, however, so this may
     hang, depending on the MPI-2 implementation.  Currently works with
     MP_Lite */
     
  if( p->prot.no_fence ) {
    
    while(p->buff[p->bufflen-1] != 'b'+p->prot.iproc)
      sched_yield();
    
    p->buff[p->bufflen-1] = 'b'+p->prot.nbor;

  } else {

    MPI_Win_fence(0, win);

  }

}

void SendTime(ArgStruct *p, double *t)
{
  MPI_Send(t, 1, MPI_DOUBLE, p->prot.nbor, 2, MPI_COMM_WORLD);
}

void RecvTime(ArgStruct *p, double *t)
{
  MPI_Status status;
  
  MPI_Recv(t, 1, MPI_DOUBLE, p->prot.nbor, 2, MPI_COMM_WORLD, &status);
}

void SendRepeat(ArgStruct *p, int rpt)
{
  MPI_Send(&rpt, 1, MPI_INT, p->prot.nbor, 2, MPI_COMM_WORLD);
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
  MPI_Status status;
  
  MPI_Recv(rpt, 1, MPI_INT, p->prot.nbor, 2, MPI_COMM_WORLD, &status);
}

int CleanUp(ArgStruct *p)
{
  MPI_Finalize();
  return 0;
}

void FreeBuff(char *buff1, char *buff2)
{
  MPI_Win_fence(0, win);

  MPI_Win_free(&win);

  free(buff1);
  free(buff2);
}

int MyMalloc(ArgStruct *p, int bufflen)
{
  if((p->buff=(char *)malloc(bufflen))==(char *)NULL)
    {
      fprintf(stderr,"Couldn't allocate memory\n");
      return -1;
    }
  p->buff[bufflen-1] = 'b' + p->prot.nbor; /* Used if we are not using Fence 
                                              during timing runs */

  if((p->buff1=(char *)malloc(bufflen))==(char *)NULL)
    {
      fprintf(stderr,"Couldn't allocate memory\n");
      return -1;
    }

  /* After mallocs, we need to create MPI Windows */
  MPI_Win_create(p->buff, bufflen, 1, NULL, MPI_COMM_WORLD, &win);

  return 0;
}

void Reset(ArgStruct *p)
{

}
