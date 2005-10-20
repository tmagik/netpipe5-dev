#include  "netpipe.h"

extern double *pTime;
extern int    *pNrepeat;

int Setup(ArgStruct *p)
{
   int npes;
   if((npes=shmem_n_pes())!=2) {

      printf("Error Message: Run with npes set to 2\n");
      exit(1);
   }

   p->prot.flag=(int *) shmalloc(sizeof(int));
   pTime = (double *) shmalloc(sizeof(double));
   pNrepeat = (int *) shmalloc(sizeof(int));

   if((p->prot.ipe=_my_pe()) == 0) {
      p->tr=1;
      p->prot.nbor=1;
      *p->prot.flag=1;

   } else {

      p->tr=0;
      p->prot.nbor=0;
      *p->prot.flag=0;
   }
   return 0;
}

void Sync(ArgStruct *p)
{
   shmem_barrier_all();
}

void PrepareToReceive(ArgStruct *p) { }

void SendData(ArgStruct *p)
{
   if(p->bufflen%8==0)
      shmem_put64(p->buff,p->buff,p->bufflen/8,p->prot.nbor);
   else
      shmem_putmem(p->buff,p->buff,p->bufflen,p->prot.nbor);
}

void RecvData(ArgStruct *p)
{
   int i=0;

   while(p->buff[p->bufflen-1]!='b'+p->prot.ipe) {

      if(++i%10000000==0) printf(""); 

   }

   p->buff[p->bufflen-1]='b'+p->prot.nbor; 
}

void SendTime(ArgStruct *p, double *t)
{
   *pTime=*t;

   shmem_double_put(pTime,pTime,1,p->prot.nbor);
   shmem_int_put(p->prot.flag,p->prot.flag,1,p->prot.nbor);
}

void RecvTime(ArgStruct *p, double *t)
{
   int i=0;

   while(*p->prot.flag!=p->prot.ipe)
   {
      if(++i%10000000==0) printf("");
   }
   *t=*pTime; 
   *p->prot.flag=p->prot.nbor;
}

int Establish(ArgStruct *p)
{
        return 0;
}

void SendRepeat(ArgStruct *p, int rpt)
{
   *pNrepeat= rpt;
   shmem_int_put(pNrepeat,pNrepeat,1,p->prot.nbor);
   shmem_int_put(p->prot.flag,p->prot.flag,1,p->prot.nbor);
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
   int i=0;

   while(*p->prot.flag!=p->prot.ipe)
   {
      if(++i%10000000==0) printf("");
   }
   *rpt=*pNrepeat;
   *p->prot.flag=p->prot.nbor;
}

int  CleanUp(ArgStruct *p)
{
   return 0;    /* Damn SGI compilers want this */
}

void FreeBuff(char *buff1, char* buff2)
{
   shfree(buff1);
   shfree(buff2);
}

int MyMalloc(ArgStruct *p, int bufflen)
{
   if((p->buff=(char *)shmalloc(bufflen))==(char *)NULL)
   {
      fprintf(stderr,"couldn't allocate memory\n");
      return -1;
   }
   p->buff[bufflen-1]='b'+p->tr;
   if((p->buff1=(char *)shmalloc(bufflen))==(char *)NULL)
   {
      fprintf(stderr,"Couldn't allocate memory\n");
      return -1;
   }
   return 0;
}
