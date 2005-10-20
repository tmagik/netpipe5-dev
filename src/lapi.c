#include "netpipe.h"
extern lapi_handle_t  t_hndl;
extern lapi_cntr_t    l_cntr;
extern lapi_cntr_t    t_cntr;
extern lapi_cntr_t    c_cntr;
extern lapi_info_t    t_info;  /* LAPI info structure */
extern void           *global_addr[2];
extern void           *global_addr1[2];
extern void           *tgt_addr[2];
extern void           *rpt_addr[2];
extern void           *time_addr[2];
extern int            *pRepeat;
int Setup(ArgStruct *p)
{
        int tr, one=1;
        int           task_id,              /* My task id */
                      num_tasks;            /* Number of tasks in my job */
        char*         t_buf;         /* Buffer to manipulate */
        int           loop, rc, val, cur_val; 
        char          err_msg_buf[LAPI_MAX_ERR_STRING];
        bzero(&t_info, sizeof(lapi_info_t));

        t_info.err_hndlr = NULL;   /* Not registering error handler function */

        if ((rc = LAPI_Init(&t_hndl, &t_info)) != LAPI_SUCCESS) {
                LAPI_Msg_string(rc, err_msg_buf);
                printf("Error Message: %s, rc = %d\n", err_msg_buf, rc);
                exit (rc);
        }

        rc = LAPI_Qenv(t_hndl, TASK_ID, &task_id);     /* Get task number */
                                                /* within job */

        rc = LAPI_Qenv(t_hndl, NUM_TASKS, &num_tasks); /* Get number of */
                                                /* tasks in job */


        if (num_tasks != 2) {
                printf("Error Message: Run with MP_PROCS set to 2\n");
                exit(1);
        }

        /* Turn off parameter checking - default is on */
        rc = LAPI_Senv(t_hndl, ERROR_CHK, 0);

                /* Initialize counters to be zero at the start */
        rc = LAPI_Setcntr(t_hndl, &l_cntr, 0);

        rc = LAPI_Setcntr(t_hndl, &t_cntr, 0);

        rc = LAPI_Setcntr(t_hndl, &c_cntr, 0);

        rc = LAPI_Address_init(t_hndl,&t_cntr,tgt_addr);

        rc = LAPI_Address_init(t_hndl,pRepeat,rpt_addr); 
        /* Exchange buffer address and target counter address of every task */
	
        if (task_id ==0)
	{
                p->tr=1;
		p->prot.nbor=1;
	}
        else
	{
                p->tr=0;
		p->prot.nbor=0;
	}
        return 0;
}

void Sync(ArgStruct *p)
{
        LAPI_Gfence(t_hndl);
}

void PrepareToReceive(ArgStruct *p)
{
 /* Nothing to do */
}

void SendData(ArgStruct *p)
{
	int rc;
        rc = LAPI_Put(t_hndl,p->prot.nbor,p->bufflen*sizeof(char),
               global_addr[p->prot.nbor],(void *)p->buff,tgt_addr[p->prot.nbor],
               &l_cntr,&c_cntr);
        /* Wait for local Put completion */
        rc = LAPI_Waitcntr(t_hndl, &l_cntr, 1, NULL); 
/*        printf("In SendData, rc=%d\n",rc);*/
}

void RecvData(ArgStruct *p)
{
	int rc,val,cur_val;
        rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        while (val < 1) {
            rc = LAPI_Probe(t_hndl); /* Poll the adapter once */
            rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        }
        /* To clear the t_cntr value */
        rc = LAPI_Waitcntr(t_hndl, &t_cntr, 1, &cur_val); 
/*        printf("In RecvData, rc=%d\n",rc);*/
}

void SendTime(ArgStruct *p, double *t)
{
        int rc;
        rc = LAPI_Address_init(t_hndl,t,time_addr);
        rc = LAPI_Put(t_hndl,p->prot.nbor,sizeof(double),
               time_addr[p->prot.nbor],(void *)t,tgt_addr[p->prot.nbor],
               &l_cntr,&c_cntr);
        /* Wait for local Put completion */
        rc = LAPI_Waitcntr(t_hndl, &l_cntr, 1, NULL);
/*        printf("In SendData, rc=%d\n",rc);*/
}

void RecvTime(ArgStruct *p, double *t)
{
        int rc, val, cur_val;
        rc = LAPI_Address_init(t_hndl,t,time_addr);
        rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        while (val < 1) {
            rc = LAPI_Probe(t_hndl); /* Poll the adapter once */
            rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        }
        /* To clear the t_cntr value */
        rc = LAPI_Waitcntr(t_hndl, &t_cntr, 1, &cur_val);
}

int Establish(ArgStruct *p)
{
	return 0;
}

void SendRepeat(ArgStruct *p, int rpt)
{
        int rc;
/*        *pRepeat = rpt;
        rc = LAPI_Address_init(t_hndl,pRepeat,rpt_addr);*/
        rc = LAPI_Put(t_hndl,p->prot.nbor,sizeof(int), rpt_addr[p->prot.nbor],
                        (void *)pRepeat,tgt_addr[p->prot.nbor],&l_cntr,&c_cntr);
        /* Wait for local Put completion */
        rc = LAPI_Waitcntr(t_hndl, &l_cntr, 1, NULL);    
}

void RecvRepeat(ArgStruct *p, int *rpt) 
{
        int rc,val,cur_val;
/*        rc = LAPI_Address_init(t_hndl,pRepeat,rpt_addr);*/
        rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        while (val < 1) {
            rc = LAPI_Probe(t_hndl); /* Poll the adapter once */
            rc = LAPI_Getcntr(t_hndl, &t_cntr, &val);
        }
        /* To clear the t_cntr value */
/*        *rpt = *pRepeat; */
        rc = LAPI_Waitcntr(t_hndl, &t_cntr, 1, &cur_val);  
}

int  CleanUp(ArgStruct *p)
{
	int rc;
	rc = LAPI_Gfence(t_hndl); /* Global fence to sync before terminating job */
	rc = LAPI_Term(t_hndl);   
   	return 0;    /* Damn SGI compilers want this */
}        

void FreeBuff(char *buff1, char *buff2)
{
	free(buff1);
	free(buff2);
}

int MyMalloc(ArgStruct *p, int bufflen)
{
    int rc;
    if((p->buff=(char *)malloc(bufflen))==(char *)NULL)
    {
        fprintf(stderr,"couldn't allocate memory\n");
        return -1;
    }
    rc = LAPI_Address_init(t_hndl,p->buff,global_addr);
    if((p->buff1=(char *)malloc(bufflen))==(char *)NULL)
    {
        fprintf(stderr,"Couldn't allocate memory\n");
        return -1;
    }
    rc = LAPI_Address_init(t_hndl,p->buff1,global_addr);
    return 0;
}
