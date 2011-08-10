/* Netpipe for MPI Communication Collectives developed by Veerendra Allada */
/* modified by Troy Benjegerdes for Netpipe-5 */

#include	"netpipe.h"
#include	<mpi.h>

#define MAXPROCS 2048

void Init(Netpipe * self, int *pargc, char ***pargv)
{
	ArgStruct *p = &self->args;
	char s[255], *ptr;
	
	MPI_Init(pargc,pargv);	
	MPI_Comm_rank(MPI_COMM_WORLD,&p->prot.iproc);
	MPI_Comm_size(MPI_COMM_WORLD,&p->prot.nprocs);
	
	p->source_node=0;
	
	if(p->prot.iproc==0){
		p->tr=1;
		p->rcv=0;
	}else{
		p->tr=0;
		p->rcv=1;
	}
	
	if(p->prot.nprocs < 2)
	{
		printf("Need at least two processes (given only %d) \n", p->prot.nprocs);
		exit(-1);
	}
	if(p->prot.nprocs>MAXPROCS)
	{
		printf(" Exceeded the manimum number of proceeses \n");
		exit(-2);
	}
	gethostname(s,253);
	/* Get the Name alone from the Fully Qualified Domain Name */
	if(s[0]!='.'){
		ptr=strchr(s,'.');
		if(ptr!=NULL) *ptr='\0';
	}
	printf("%d: %s\n", p->prot.iproc,s);
	fflush(stdout);
	
	
}	

void Setup( ArgStruct *p)
{
#ifdef DEBUG
	volatile int stop = 1;
	int i = 0;
	while(stop){ 
		i += 1;
		usleep(10);
	}
#endif
}

void Sync(ArgStruct *p)
{
	MPI_Barrier(MPI_COMM_WORLD);
}

void SendData(ArgStruct *p)
{
	MPI_Alltoall(p->s_ptr,p->bufflen,MPI_BYTE,p->r_ptr,p->bufflen,MPI_BYTE,MPI_COMM_WORLD);
}

void RecvData(ArgStruct *p)
{
	MPI_Alltoall(p->s_ptr,p->bufflen,MPI_BYTE,p->r_ptr,p->bufflen,MPI_BYTE,MPI_COMM_WORLD);
}

void SendRepeat(ArgStruct *p, uint32_t nrepeat)
{
	MPI_Bcast(&nrepeat,1,MPI_INT,p->source_node,MPI_COMM_WORLD);
}

void RecvRepeat(ArgStruct *p, uint32_t *nrepeat)
{
	MPI_Bcast(nrepeat,1,MPI_INT,p->source_node,MPI_COMM_WORLD);
}

void Reset(ArgStruct *p)
{

}

void CleanUp(ArgStruct *p)
{
	MPI_Finalize();
}

void InitBufferData(ArgStruct *p, int nbytes, int soffset, int roffset)
{

    int nprocs,i ;
    int nbytes1;
    soffset=roffset=0;
    nprocs=p->prot.nprocs;
    
	nbytes1=nbytes*nprocs;
    memset(p->r_buff,'a',nbytes1+MAX(soffset,roffset));
    
    if(p->cache){
	    if(p->tr){
		    for(i=0;i<nprocs;i++){
			    p->r_buff[nbytes*(i+1)+MAX(soffset,roffset)-1]='b';
		    }
	    }
	    if(p->rcv){
		    p->r_buff[nbytes+MAX(soffset,roffset)-1]='b';
		    /*for(i=0;i<nprocs;i++){
			    p->r_buff[nbytes*(i+1)+MAX(soffset,roffset)-1]='b';
		    }*/
		    //p->r_buff[nbytes1+MAX(soffset,roffset)-1]='b';
	    }
    }else{
	    memset(p->s_buff,'b',nbytes+soffset);
    }
}

void MyMalloc(ArgStruct *p, int bufflen,int soffset, int roffset)
{

    int nprocs;
    int bufflen1;
    soffset=0;
    roffset=0;
    nprocs=p->prot.nprocs;

	bufflen1=bufflen*nprocs;
    if((p->r_buff=(char *)malloc(bufflen1+MAX(soffset,roffset)))==(char *)NULL){
	    printf(" Could not allocate memory for the recv buffer, Proc ID = %d\n", p->prot.iproc);
	    exit(-2);
    }
  
    if(!p->cache){
            if((p->s_buff=(char *)malloc(bufflen1+soffset))==(char *)NULL){
                printf(" Could not allocate memory for the recv buffer, Proc ID = %d\n",p->prot.iproc);
                exit(-2);
            }
    }
}

void FreeBuff( char *buff1, char*buff2)
{
    if(buff1!=NULL)
        free(buff1);
    if(buff2!=NULL)
        free(buff2);
}

void AfterAlignmentInit(ArgStruct *p)
{

}

void PrepareToReceive(ArgStruct *p)
{
}

void SendTime(ArgStruct *p, double *t)
{
}

void RecvTime(ArgStruct *p, double *t)
{
}


