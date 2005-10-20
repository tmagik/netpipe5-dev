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
/*     * pvm.c              ---- PVM calls source                            */
/*****************************************************************************/
#include    "netpipe.h"
#include    <pvm3.h>

extern int mytid;

int Init(ArgStruct *p, int* pargc, char*** pargv)
{

}

int Setup(ArgStruct *p)
{
    int parent_tid;
    int nprocs;

    p->prot.tid = mytid;
    parent_tid = pvm_parent();
/*    pvm_joingroup("netpipe"); */
    {
        char s[255];
        gethostname(s,253);
        printf("%d: %s\n",p->prot.tid,s); fflush(stdout);
    }

    if (parent_tid == PvmNoParent)
    {
/*        nprocs = pvm_spawn("NPpvm", (char**)0, 0, "", 1, &p->prot.nbor);*/
printf("spawn to host %s\n", p->host ); fflush(stdout);
        nprocs = pvm_spawn("NPpvm", (char**)0, 1, p->host, 1, &p->prot.nbor);
        if (nprocs != 1) 
        {
            printf("Error spawning the receiver. Error code is: %d\n", nprocs);
            exit(0);  
        }
    }
    else
        p->prot.nbor = parent_tid;

    if (parent_tid == PvmNoParent)
        p->tr = 1;
    else
        p->tr = 0;
}   

void Sync(ArgStruct *p)
{
/*          pvm_barrier("netpipe", 2); */
}


void PrepareToReceive(ArgStruct *p)
{
        /*
          The PVM interface doesn't have a method to pre-post
          a buffer for reception of data.
        */ 
}

void SendData(ArgStruct *p)
{
        pvm_pkbyte( p->buff, p->bufflen, 1 );

        pvm_send( p->prot.nbor, 1 );
}

void RecvData(ArgStruct *p)
{
        pvm_recv( -1, -1 );

        pvm_upkbyte( p->buff, p->bufflen, 1 );
}


void SendTime(ArgStruct *p, double *t)
{
        pvm_pkdouble(t, 1, 1);

        pvm_send(p->prot.nbor, 2);
}

void RecvTime(ArgStruct *p, double *t)
{
        pvm_recv(p->prot.nbor, 2);

        pvm_upkdouble(t, 1, 1);
}

void SendRepeat(ArgStruct *p, int nrepeat)
{
        pvm_pkint( &nrepeat, 1, 1 );

        pvm_send( p->prot.nbor, 1 );
}

void RecvRepeat(ArgStruct *p, int *nrepeat)
{
        pvm_recv( -1, -1 );

        pvm_upkint( nrepeat, 1, 1 );
}


int  CleanUp(ArgStruct *p)
{
/*        pvm_lvgroup("netpipe"); */
        pvm_exit();
}

void Reset(ArgStruct *p)
{

}
