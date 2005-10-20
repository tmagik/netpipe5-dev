#include  "netpipe.h"

extern double *pTime;
extern int    *pNrepeat;

int Setup(ArgStruct *p)
{
   int npes;

   if((npes=gpnumpes())!=2) {

      printf("Error Message: npes = %d - You must run on 2 nodes\n", gpnumpes());
      exit(1);
   }

   p->prot.flag=(int *) gpshmalloc(sizeof(int));
   pTime = (double *) gpshmalloc(sizeof(double));
   pNrepeat = (int *) gpshmalloc(sizeof(int));

   if((p->prot.ipe=gpmype()) == 0) {
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
   gpshmem_barrier_all();
}

void PrepareToReceive(ArgStruct *p) { }

void SendData(ArgStruct *p)
{
   if(p->bufflen%4==0)
      gpshmem_put32(p->buff,p->buff,p->bufflen/4,p->prot.nbor);
   else
      gpshmem_putmem(p->buff,p->buff,p->bufflen,p->prot.nbor);
}

void RecvData(ArgStruct *p)
{
   int i=0;

   while( p->buff[p->bufflen-1] != 'b'+p->prot.ipe ) {

      if( ++i%10000000==0 ) printf(""); 

   }

   p->buff[p->bufflen-1] = 'b' + p->prot.nbor; 
}

void SendTime(ArgStruct *p, double *t)
{
   *pTime=*t;

   gpshmem_putmem(pTime,pTime,sizeof(double),p->prot.nbor);
   gpshmem_putmem(p->prot.flag,p->prot.flag,sizeof(int),p->prot.nbor);
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
printf("%d in SendRepeat()\n",p->prot.ipe); fflush(stdout);
printf("%d: %d %p\n",p->prot.ipe, *p->prot.flag, p->prot.flag); fflush(stdout);
/*
*/

   *pNrepeat= rpt;

   gpshmem_putmem(pNrepeat,pNrepeat,sizeof(int),p->prot.nbor);

   gpshmem_putmem(p->prot.flag,p->prot.flag,sizeof(int),p->prot.nbor);

printf("%d leaving SendRepeat()\n",p->prot.ipe); fflush(stdout);
/*
*/
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
   int i=0;

printf("%d in RecvRepeat()\n",p->prot.ipe); fflush(stdout);
printf("%d: %d %p\n",p->prot.ipe, *p->prot.flag, p->prot.flag); fflush(stdout);
sleep(1);
/*
*/
   while( *p->prot.flag != p->prot.ipe ) {

      if( ++i%2 == 3 ) printf("%d", *p->prot.flag);  /* invalidate cache */
      if( i%10000 == 9999 ) {
printf("%d: %d %p\n",p->prot.ipe, *p->prot.flag, p->prot.flag); fflush(stdout);
         sleep(1);
      }
/*
*/
   }
   *rpt=*pNrepeat;
   *p->prot.flag=p->prot.nbor;
printf("\n%d: %d %p\n",p->prot.ipe, *p->prot.flag, p->prot.flag); fflush(stdout);
printf("%d leaving RecvRepeat()\n",p->prot.ipe); fflush(stdout);
/*
*/
}

int  CleanUp(ArgStruct *p)
{
   gpshmem_finalize();
   return 0;    /* Damn SGI compilers want this */
}

void FreeBuff(char *buff1, char* buff2)
{
   gpshfree(buff1);
   gpshfree(buff2);
}

int MyMalloc(ArgStruct *p, int bufflen)
{
   if((p->buff=(char *)gpshmalloc(bufflen))==(char *)NULL)
   {
      fprintf(stderr,"couldn't allocate memory\n");
      return -1;
   }
   p->buff[bufflen-1]='b'+p->tr;
   if((p->buff1=(char *)gpshmalloc(bufflen))==(char *)NULL)
   {
      fprintf(stderr,"Couldn't allocate memory\n");
      return -1;
   }
   return 0;
}
