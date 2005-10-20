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
/*       ibv.c             ---- Infiniband module for OpenIB verbs           */
/*****************************************************************************/

#define USE_VOLATILE_RPTR /* needed for polling on last byte of recv buffer */
#include    "netpipe.h"
#include    <stdio.h>
#include    <getopt.h>
#include    <pthread.h>

/* Debugging output macro */

FILE* logfile;

#if 0
#define LOGPRINTF(_format, _aa...) fprintf(logfile, "%s: " _format, __func__ , ##_aa); fflush(logfile)
#else
#define LOGPRINTF(_format, _aa...)
#endif

/* Header files needed for Infiniband */

#include    <infiniband/verbs.h>

/* Global vars */

static struct ibv_device      *hca;
static struct ibv_context     *ctx;
static struct ibv_port_attr    hca_port;
static int                     port_num;
static uint16_t                lid;
static uint16_t                d_lid;
static struct ibv_pd          *pd_hndl;
static int                     num_cqe;
static int                     act_num_cqe;
static struct ibv_cq          *s_cq_hndl;
static struct ibv_cq          *r_cq_hndl;
static struct ibv_mr          *s_mr_hndl;
static struct ibv_mr          *r_mr_hndl;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp          *qp_hndl;
static uint32_t                d_qp_num;
static struct ibv_qp_attr      qp_attr;
static struct ibv_wc           wc;
static int                     max_wq=50000;
static void*                   remote_address;
static uint32_t                remote_key;
static volatile int            receive_complete;
static pthread_t               thread;

/* Function definitions */

void Init(ArgStruct *p, int* pargc, char*** pargv)
{
   /* Set defaults
    */
   p->prot.ib_mtu = IBV_MTU_1024;        /* 1024 Byte MTU                    */
   p->prot.commtype = NP_COMM_RDMAWRITE; /* Use RDMA write communications    */
   p->prot.comptype = NP_COMP_LOCALPOLL; /* Use local polling for completion */
   p->tr = 0;                            /* I am not the transmitter         */
   p->rcv = 1;                           /* I am the receiver                */      
}

void Setup(ArgStruct *p)
{

 int one = 1;
 int sockfd;
 struct sockaddr_in *lsin1, *lsin2;      /* ptr to sockaddr_in in ArgStruct */
 char *host;
 struct hostent *addr;
 struct protoent *proto;
 int send_size, recv_size, sizeofint = sizeof(int);
 struct sigaction sigact1;
 char logfilename[80];

 /* Sanity check */
 if( p->prot.commtype == NP_COMM_RDMAWRITE && 
     p->prot.comptype != NP_COMP_LOCALPOLL ) {
   fprintf(stderr, "Error, RDMA Write may only be used with local polling.\n");
   fprintf(stderr, "Try using RDMA Write With Immediate Data with vapi polling\n");
   fprintf(stderr, "or event completion\n");
   exit(-1);
 }
 
 if( p->prot.commtype != NP_COMM_RDMAWRITE && 
     p->prot.comptype == NP_COMP_LOCALPOLL ) {
   fprintf(stderr, "Error, local polling may only be used with RDMA Write.\n");
   fprintf(stderr, "Try using vapi polling or event completion\n");
   exit(-1);
 }

 /* Open log file */
 sprintf(logfilename, ".iblog%d", 1 - p->tr);
 logfile = fopen(logfilename, "w");

 host = p->host;                           /* copy ptr to hostname */ 

 lsin1 = &(p->prot.sin1);
 lsin2 = &(p->prot.sin2);

 bzero((char *) lsin1, sizeof(*lsin1));
 bzero((char *) lsin2, sizeof(*lsin2));

 if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
   printf("NetPIPE: can't open stream socket! errno=%d\n", errno);
   exit(-4);
 }

 if(!(proto = getprotobyname("tcp"))){
   printf("NetPIPE: protocol 'tcp' unknown!\n");
   exit(555);
 }

 if (p->tr){                                  /* if client i.e., Sender */


   if (atoi(host) > 0) {                   /* Numerical IP address */
     lsin1->sin_family = AF_INET;
     lsin1->sin_addr.s_addr = inet_addr(host);

   } else {
      
     if ((addr = gethostbyname(host)) == NULL){
       printf("NetPIPE: invalid hostname '%s'\n", host);
       exit(-5);
     }

     lsin1->sin_family = addr->h_addrtype;
     bcopy(addr->h_addr, (char*) &(lsin1->sin_addr.s_addr), addr->h_length);
   }

   lsin1->sin_port = htons(p->port);

 } else {                                 /* we are the receiver (server) */
   
   bzero((char *) lsin1, sizeof(*lsin1));
   lsin1->sin_family      = AF_INET;
   lsin1->sin_addr.s_addr = htonl(INADDR_ANY);
   lsin1->sin_port        = htons(p->port);
   
   if (bind(sockfd, (struct sockaddr *) lsin1, sizeof(*lsin1)) < 0){
     printf("NetPIPE: server: bind on local address failed! errno=%d", errno);
     exit(-6);
   }

 }

 if(p->tr)
   p->commfd = sockfd;
 else
   p->servicefd = sockfd;

 

 /* Establish tcp connections */

 establish(p);

 /* Initialize Mellanox Infiniband */

 if(initIB(p) == -1) {
   CleanUp(p);
   exit(-1);
 }
}   

void event_handler(struct ibv_cq *cq);

void *EventThread(void *unused)
{
  struct ibv_cq *cq;
  void *ev_ctx;

  while (1) {
    if (ibv_get_cq_event(0, &cq, &ev_ctx)) {
      fprintf(stderr, "Failed to get CQ event\n");
      return NULL;
    }
    event_handler(cq);
  }
}

int initIB(ArgStruct *p)
{
  struct dlist *dev_list;
  int ret;

  dev_list = ibv_get_devices();
  dlist_start(dev_list);
  hca = dlist_next(dev_list);
  if (!hca) {
    fprintf(stderr, "Couldn't find any InfiniBand devices\n");
    return -1;
  } else {
    LOGPRINTF("Found Infiniband HCA %s\n", ibv_get_device_name(hca));
  }

  ctx = ibv_open_device(hca);
  if (!ctx) {
    fprintf(stderr, "Couldn't create InfiniBand context\n");
    return -1;
  } else {
    LOGPRINTF("Found Infiniband HCA %s\n", ibv_get_device_name(hca));
  }

  /* Get HCA properties */

  port_num=1;
  ret = ibv_query_port(ctx, port_num, &hca_port);
  if(ret) {
    fprintf(stderr, "Error querying Infiniband HCA\n");
    return -1;
  } else {
    LOGPRINTF("Queried Infiniband HCA\n");
  }
  lid = hca_port.lid;
  LOGPRINTF("  lid = %d\n", lid);


  /* Allocate Protection Domain */

  pd_hndl = ibv_alloc_pd(ctx);
  if(!pd_hndl) {
    fprintf(stderr, "Error allocating PD\n");
    return -1;
  } else {
    LOGPRINTF("Allocated Protection Domain\n");
  }


  /* Create send completion queue */
  
  num_cqe = 30000; /* Requested number of completion q elements */
  s_cq_hndl = ibv_create_cq(ctx, num_cqe, NULL, NULL, 0);
  if(!s_cq_hndl) {
    fprintf(stderr, "Error creating send CQ\n");
    return -1;
  } else {
    act_num_cqe = s_cq_hndl->cqe;
    LOGPRINTF("Created Send Completion Queue with %d elements\n", act_num_cqe);
  }


  /* Create recv completion queue */
  
  num_cqe = 20000; /* Requested number of completion q elements */
  r_cq_hndl = ibv_create_cq(ctx, num_cqe, NULL, NULL, 0);
  if(!r_cq_hndl) {
    fprintf(stderr, "Error creating send CQ\n");
    return -1;
  } else {
    act_num_cqe = r_cq_hndl->cqe;
    LOGPRINTF("Created Recv Completion Queue with %d elements\n", act_num_cqe);
  }


  /* Placeholder for MR */


  /* Create Queue Pair */

  qp_init_attr.cap.max_recv_wr    = max_wq; /* Max outstanding WR on RQ      */
  qp_init_attr.cap.max_send_wr    = max_wq; /* Max outstanding WR on SQ      */
  qp_init_attr.cap.max_recv_sge   = 1; /* Max scatter/gather entries on RQ */
  qp_init_attr.cap.max_send_sge   = 1; /* Max scatter/gather entries on SQ */
  qp_init_attr.recv_cq            = r_cq_hndl; /* CQ handle for RQ         */
  qp_init_attr.send_cq            = s_cq_hndl; /* CQ handle for SQ         */
  qp_init_attr.sq_sig_all         = 0; /* Signalling type */
  qp_init_attr.qp_type            = IBV_QPT_RC; /* Transmission type         */
  
  qp_hndl = ibv_create_qp(pd_hndl, &qp_init_attr);
  if(!qp_hndl) {
    fprintf(stderr, "Error creating Queue Pair\n");
    return -1;
  } else {
    LOGPRINTF("Created Queue Pair\n");
  }


  /* Exchange lid and qp_num with other node */
  
  if( write(p->commfd, &lid, sizeof(lid) ) != sizeof(lid) ) {
    fprintf(stderr, "Failed to send lid over socket\n");
    return -1;
  }
  if( write(p->commfd, &qp_hndl->qp_num, sizeof(qp_hndl->qp_num) ) != sizeof(qp_hndl->qp_num) ) {
    fprintf(stderr, "Failed to send qpnum over socket\n");
    return -1;
  }
  if( read(p->commfd, &d_lid, sizeof(d_lid) ) != sizeof(d_lid) ) {
    fprintf(stderr, "Failed to read lid from socket\n");
    return -1;
  }
  if( read(p->commfd, &d_qp_num, sizeof(d_qp_num) ) != sizeof(d_qp_num) ) {
    fprintf(stderr, "Failed to read qpnum from socket\n");
    return -1;
  }
  
  LOGPRINTF("Local: lid=%d qp_num=%d Remote: lid=%d qp_num=%d\n",
         lid, qp_hndl->qp_num, d_lid, d_qp_num);


  /* Bring up Queue Pair */
  
  /******* INIT state ******/

  qp_attr.qp_state = IBV_QPS_INIT;
  qp_attr.pkey_index = 0;
  qp_attr.port_num = port_num;
  qp_attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

  ret = ibv_modify_qp(qp_hndl, &qp_attr,
		      IBV_QP_STATE              |
		      IBV_QP_PKEY_INDEX         |
		      IBV_QP_PORT               |
		      IBV_QP_ACCESS_FLAGS);
  if(ret) {
    fprintf(stderr, "Error modifying QP to INIT\n");
    return -1;
  }

  LOGPRINTF("Modified QP to INIT\n");

  /******* RTR (Ready-To-Receive) state *******/

  qp_attr.qp_state = IBV_QPS_RTR;
  qp_attr.max_dest_rd_atomic = 1;
  qp_attr.dest_qp_num = d_qp_num;
  qp_attr.ah_attr.sl = 0;
  qp_attr.ah_attr.is_global = 0;
  qp_attr.ah_attr.dlid = d_lid;
  qp_attr.ah_attr.static_rate = 0;
  qp_attr.ah_attr.src_path_bits = 0;
  qp_attr.ah_attr.port_num = port_num;
  qp_attr.path_mtu = p->prot.ib_mtu;
  qp_attr.rq_psn = 0;
  qp_attr.pkey_index = 0;
  qp_attr.min_rnr_timer = 5;
  
  ret = ibv_modify_qp(qp_hndl, &qp_attr,
		      IBV_QP_STATE              |
		      IBV_QP_AV                 |
		      IBV_QP_PATH_MTU           |
		      IBV_QP_DEST_QPN           |
		      IBV_QP_RQ_PSN             |
		      IBV_QP_MAX_DEST_RD_ATOMIC |
		      IBV_QP_MIN_RNR_TIMER);

  if(ret) {
    fprintf(stderr, "Error modifying QP to RTR\n");
    return -1;
  }

  LOGPRINTF("Modified QP to RTR\n");

  /* Sync before going to RTS state */
  Sync(p);

  /******* RTS (Ready-to-Send) state *******/

  qp_attr.qp_state = IBV_QPS_RTS;
  qp_attr.sq_psn = 0;
  qp_attr.timeout = 31;
  qp_attr.retry_cnt = 1;
  qp_attr.rnr_retry = 1;
  qp_attr.max_rd_atomic = 1;

  ret = ibv_modify_qp(qp_hndl, &qp_attr,
		      IBV_QP_STATE              |
		      IBV_QP_TIMEOUT            |
		      IBV_QP_RETRY_CNT          |
		      IBV_QP_RNR_RETRY          |
		      IBV_QP_SQ_PSN             |
		      IBV_QP_MAX_QP_RD_ATOMIC);

  if(ret) {
    fprintf(stderr, "Error modifying QP to RTS\n");
    return -1;
  }
  
  LOGPRINTF("Modified QP to RTS\n");

  /* If using event completion, request the initial notification */
  if( p->prot.comptype == NP_COMP_EVENT ) {
    if (pthread_create(&thread, NULL, EventThread, NULL)) {
      fprintf(stderr, "Couldn't start event thread\n");
      return -1;
    }
    ibv_req_notify_cq(r_cq_hndl, 0);
  }
 
  return 0;
}

int finalizeIB(ArgStruct *p)
{
  int ret;

  LOGPRINTF("Finalizing IB stuff\n");

  if(qp_hndl) {
    LOGPRINTF("Destroying QP\n");
    ret = ibv_destroy_qp(qp_hndl);
    if(ret) {
      fprintf(stderr, "Error destroying Queue Pair\n");
    }
  }

  if(r_cq_hndl) {
    LOGPRINTF("Destroying Recv CQ\n");
    ret = ibv_destroy_cq(r_cq_hndl);
    if(ret) {
      fprintf(stderr, "Error destroying recv CQ\n");
    }
  }

  if(s_cq_hndl) {
    LOGPRINTF("Destroying Send CQ\n");
    ret = ibv_destroy_cq(s_cq_hndl);
    if(ret) {
      fprintf(stderr, "Error destroying send CQ\n");
    }
  }

  /* Check memory registrations just in case user bailed out */
  if(s_mr_hndl) {
    LOGPRINTF("Deregistering send buffer\n");
    ret = ibv_dereg_mr(s_mr_hndl);
    if(ret) {
      fprintf(stderr, "Error deregistering send mr\n");
    }
  }

  if(r_mr_hndl) {
    LOGPRINTF("Deregistering recv buffer\n");
    ret = ibv_dereg_mr(r_mr_hndl);
    if(ret) {
      fprintf(stderr, "Error deregistering recv mr\n");
    }
  }

  if(pd_hndl) {
    LOGPRINTF("Deallocating PD\n");
    ret = ibv_dealloc_pd(pd_hndl);
    if(ret) {
      fprintf(stderr, "Error deallocating PD\n");
    }
  }

  /* Application code should not close HCA, just release handle */

  if(ctx) {
    LOGPRINTF("Releasing HCA\n");
    ret = ibv_close_device(ctx);
    if(ret) {
      fprintf(stderr, "Error releasing HCA\n");
    }
  }

  return 0;
}

void event_handler(struct ibv_cq *cq)
{
  int ret;
 
  while(1) {
     
    ret = ibv_poll_cq(cq, 1, &wc);

     if(ret == 0) {
        LOGPRINTF("Empty completion queue, requesting next notification\n");
        ibv_req_notify_cq(r_cq_hndl, 0);
        return;
     } else if(ret < 0) {
        fprintf(stderr, "Error in event_handler, polling cq\n");
        exit(-1);
     } else if(wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Error in event_handler, on returned work completion "
		"status: %d\n", wc.status);
        exit(-1);
     }
     
     LOGPRINTF("Retrieved work completion\n");

     /* For ping-pong mode at least, this check shouldn't be needed for
      * normal operation, but it will help catch any bugs with multiple
      * sends coming through when we're only expecting one.
      */
     if(receive_complete == 1) {

        while(receive_complete != 0) sched_yield();

     }

     receive_complete = 1;

  }
  
}

static int
readFully(int fd, void *obuf, int len)
{
  int bytesLeft = len;
  char *buf = (char *) obuf;
  int bytesRead = 0;

  while (bytesLeft > 0 &&
        (bytesRead = read(fd, (void *) buf, bytesLeft)) > 0)
    {
      bytesLeft -= bytesRead;
      buf += bytesRead;
    }
  if (bytesRead <= 0)
    return bytesRead;
  return len;
}

void Sync(ArgStruct *p)
{
    char s[] = "SyncMe";
    char response[7];

    if (write(p->commfd, s, strlen(s)) < 0 ||
        readFully(p->commfd, response, strlen(s)) < 0)
      {
        perror("NetPIPE: error writing or reading synchronization string");
        exit(3);
      }
    if (strncmp(s, response, strlen(s)))
      {
        fprintf(stderr, "NetPIPE: Synchronization string incorrect!\n");
        exit(3);
      }
}

void PrepareToReceive(ArgStruct *p)
{
  int                ret;       /* Return code */
  struct ibv_recv_wr rr;        /* Receive request */
  struct ibv_recv_wr *bad_wr;
  struct ibv_sge     sg_entry;  /* Scatter/Gather list - holds buff addr */

  /* We don't need to post a receive if doing RDMA write with local polling */

  if( p->prot.commtype == NP_COMM_RDMAWRITE &&
      p->prot.comptype == NP_COMP_LOCALPOLL )
     return;
  
  rr.num_sge = 1;
  rr.sg_list = &sg_entry;
  rr.next = NULL;

  sg_entry.lkey = r_mr_hndl->lkey;
  sg_entry.length = p->bufflen;
  sg_entry.addr = (uintptr_t)p->r_ptr;

  ret = ibv_post_recv(qp_hndl, &rr, &bad_wr);
  if(ret) {
    fprintf(stderr, "Error posting recv request\n");
    CleanUp(p);
    exit(-1);
  } else {
    LOGPRINTF("Posted recv request\n");
  }

  /* Set receive flag to zero and request event completion 
   * notification for this receive so the event handler will 
   * be triggered when the receive completes.
   */
  if( p->prot.comptype == NP_COMP_EVENT ) {
    receive_complete = 0;
  }
}

void SendData(ArgStruct *p)
{
  int                ret;       /* Return code */
  struct ibv_send_wr sr;        /* Send request */
  struct ibv_send_wr *bad_wr;
  struct ibv_sge     sg_entry;  /* Scatter/Gather list - holds buff addr */

  /* Fill in send request struct */

  if(p->prot.commtype == NP_COMM_SENDRECV) {
     sr.opcode = IBV_WR_SEND;
     LOGPRINTF("Doing regular send\n");
  } else if(p->prot.commtype == NP_COMM_SENDRECV_WITH_IMM) {
     sr.opcode = IBV_WR_SEND_WITH_IMM;
     LOGPRINTF("Doing regular send with imm\n");
  } else if(p->prot.commtype == NP_COMM_RDMAWRITE) {
     sr.opcode = IBV_WR_RDMA_WRITE;
     sr.wr.rdma.remote_addr = (uintptr_t)(remote_address + (p->s_ptr - p->s_buff));
     sr.wr.rdma.rkey = remote_key;
     LOGPRINTF("Doing RDMA write (raddr=%p)\n", sr.wr.rdma.remote_addr);
  } else if(p->prot.commtype == NP_COMM_RDMAWRITE_WITH_IMM) {
     sr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
     sr.wr.rdma.remote_addr = (uintptr_t)(remote_address + (p->s_ptr - p->s_buff));
     sr.wr.rdma.rkey = remote_key;
     LOGPRINTF("Doing RDMA write with imm (raddr=%p)\n", sr.wr.rdma.remote_addr);
  } else {
     fprintf(stderr, "Error, invalid communication type in SendData\n");
     exit(-1);
  }
  
  sr.send_flags = 0; /* This needed due to a bug in Mellanox HW rel a-0 */

  sr.num_sge = 1;
  sr.sg_list = &sg_entry;
  sr.next = NULL;

  sg_entry.lkey = s_mr_hndl->lkey; /* Local memory region key */
  sg_entry.length = p->bufflen;
  sg_entry.addr = (uintptr_t)p->s_ptr;

  ret = ibv_post_send(qp_hndl, &sr, &bad_wr);
  if(ret) {
    fprintf(stderr, "Error posting send request\n");
  } else {
    LOGPRINTF("Posted send request\n");
  }

}

void RecvData(ArgStruct *p)
{
  int ret;

  /* Busy wait for incoming data */

  LOGPRINTF("Receiving at buffer address %p\n", p->r_ptr);

  /*
   * Unsignaled receives are not supported, so we must always poll the
   * CQ, except when using RDMA writes.
   */
  if( p->prot.commtype == NP_COMM_RDMAWRITE ) {
       
    /* Poll for receive completion locally on the receive data */

    LOGPRINTF("Waiting for last byte of data to arrive\n");
     
    while(p->r_ptr[p->bufflen-1] != 'a' + (p->cache ? 1 - p->tr : 1) ) 
    {
       /* BUSY WAIT -- this should be fine since we 
        * declared r_ptr with volatile qualifier */ 
    }

    /* Reset last byte */
    p->r_ptr[p->bufflen-1] = 'a' + (p->cache ? p->tr : 0);

    LOGPRINTF("Received all of data\n");

  } else if( p->prot.comptype != NP_COMP_EVENT ) {
     
     /* Poll for receive completion using VAPI poll function */

     LOGPRINTF("Polling completion queue for VAPI work completion\n");
     
     ret = 0;
     while(ret == 0)
        ret = ibv_poll_cq(r_cq_hndl, 1, &wc);

     if(ret < 0) {
        fprintf(stderr, "Error in RecvData, polling for completion\n");
        exit(-1);
     }

     if(wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "Error in status of returned completion: %d\n",
                wc.status);
        exit(-1);
     }

     LOGPRINTF("Retrieved successful completion\n");
     
  } else if( p->prot.comptype == NP_COMP_EVENT ) {

     /* Instead of polling directly on data or VAPI completion queue,
      * let the VAPI event completion handler set a flag when the receive
      * completes, and poll on that instead. Could try using semaphore here
      * as well to eliminate busy polling
      */

     LOGPRINTF("Polling receive flag\n");
     
     while( receive_complete == 0 )
     {
        /* BUSY WAIT */
     }

     /* If in prepost-burst mode, we won't be calling PrepareToReceive
      * between ping-pongs, so we need to reset the receive_complete
      * flag here.
      */
     if( p->preburst ) receive_complete = 0;

     LOGPRINTF("Receive completed\n");
  }
}

/* Reset is used after a trial to empty the work request queues so we
   have enough room for the next trial to run */
void Reset(ArgStruct *p)
{

  int                ret;       /* Return code */
  struct ibv_send_wr sr;        /* Send request */
  struct ibv_send_wr *bad_sr;
  struct ibv_recv_wr rr;        /* Recv request */
  struct ibv_recv_wr *bad_rr;

  /* If comptype is event, then we'll use event handler to detect receive,
   * so initialize receive_complete flag
   */
  if(p->prot.comptype == NP_COMP_EVENT) receive_complete = 0;

  /* Prepost receive */
  rr.num_sge = 0;
  rr.next = NULL;

  LOGPRINTF("Posting recv request in Reset\n");
  ret = ibv_post_recv(qp_hndl, &rr, &bad_rr);
  if(ret) {
    fprintf(stderr, "  Error posting recv request\n");
    CleanUp(p);
    exit(-1);
  }

  /* Make sure both nodes have preposted receives */
  Sync(p);

  /* Post Send */
  sr.opcode = IBV_WR_SEND;
  sr.send_flags = IBV_SEND_SIGNALED;
  sr.num_sge = 0;
  sr.next = NULL;

  LOGPRINTF("Posting send request \n");
  ret = ibv_post_send(qp_hndl, &sr, &bad_sr);
  if(ret) {
    fprintf(stderr, "  Error posting send request in Reset\n");
    exit(-1);
  }
  if(wc.status != IBV_WC_SUCCESS) {
     fprintf(stderr, "  Error in completion status: %d\n",
             wc.status);
     exit(-1);
  }

  LOGPRINTF("Polling for completion of send request\n");
  ret = 0;
  while(ret == 0)
    ret = ibv_poll_cq(s_cq_hndl, 1, &wc);

  if(ret < 0) {
    fprintf(stderr, "Error polling CQ for send in Reset\n");
    exit(-1);
  }
  if(wc.status != IBV_WC_SUCCESS) {
     fprintf(stderr, "  Error in completion status: %d\n",
             wc.status);
     exit(-1);
  }          
  
  LOGPRINTF("Status of send completion: %d\n", wc.status);

  if(p->prot.comptype == NP_COMP_EVENT) { 
     /* If using event completion, the event handler will set receive_complete
      * when it gets the completion event.
      */
     LOGPRINTF("Waiting for receive_complete flag\n");
     while(receive_complete == 0) { /* BUSY WAIT */ }
  } else {
     LOGPRINTF("Polling for completion of recv request\n");
     ret = 0;
     while(ret == 0)
       ret = ibv_poll_cq(r_cq_hndl, 1, &wc);
     
     if(ret < 0) {
       fprintf(stderr, "Error polling CQ for recv in Reset");
       exit(-1);
     }
     if(wc.status != IBV_WC_SUCCESS) {
        fprintf(stderr, "  Error in completion status: %d\n",
                wc.status);
        exit(-1);
     }

     LOGPRINTF("Status of recv completion: %d\n", wc.status);
  }
  LOGPRINTF("Done with reset\n");
}

void SendTime(ArgStruct *p, double *t)
{
    uint32_t ltime, ntime;

    /*
      Multiply the number of seconds by 1e6 to get time in microseconds
      and convert value to an unsigned 32-bit integer.
      */
    ltime = (uint32_t)(*t * 1.e6);

    /* Send time in network order */
    ntime = htonl(ltime);
    if (write(p->commfd, (char *)&ntime, sizeof(uint32_t)) < 0)
      {
        printf("NetPIPE: write failed in SendTime: errno=%d\n", errno);
        exit(301);
      }
}

void RecvTime(ArgStruct *p, double *t)
{
    uint32_t ltime, ntime;
    int bytesRead;

    bytesRead = readFully(p->commfd, (void *)&ntime, sizeof(uint32_t));
    if (bytesRead < 0)
      {
        printf("NetPIPE: read failed in RecvTime: errno=%d\n", errno);
        exit(302);
      }
    else if (bytesRead != sizeof(uint32_t))
      {
        fprintf(stderr, "NetPIPE: partial read in RecvTime of %d bytes\n",
                bytesRead);
        exit(303);
      }
    ltime = ntohl(ntime);

    /* Result is ltime (in microseconds) divided by 1.0e6 to get seconds */
    *t = (double)ltime / 1.0e6;
}

void SendRepeat(ArgStruct *p, int rpt)
{
  uint32_t lrpt, nrpt;

  lrpt = rpt;
  /* Send repeat count as a long in network order */
  nrpt = htonl(lrpt);
  if (write(p->commfd, (void *) &nrpt, sizeof(uint32_t)) < 0)
    {
      printf("NetPIPE: write failed in SendRepeat: errno=%d\n", errno);
      exit(304);
    }
}

void RecvRepeat(ArgStruct *p, int *rpt)
{
  uint32_t lrpt, nrpt;
  int bytesRead;

  bytesRead = readFully(p->commfd, (void *)&nrpt, sizeof(uint32_t));
  if (bytesRead < 0)
    {
      printf("NetPIPE: read failed in RecvRepeat: errno=%d\n", errno);
      exit(305);
    }
  else if (bytesRead != sizeof(uint32_t))
    {
      fprintf(stderr, "NetPIPE: partial read in RecvRepeat of %d bytes\n",
              bytesRead);
      exit(306);
    }
  lrpt = ntohl(nrpt);

  *rpt = lrpt;
}

void establish(ArgStruct *p)
{
 unsigned int clen;
 int one = 1;
 struct protoent;

 clen = sizeof(p->prot.sin2);
 if(p->tr){
   if(connect(p->commfd, (struct sockaddr *) &(p->prot.sin1),
              sizeof(p->prot.sin1)) < 0){
     printf("Client: Cannot Connect! errno=%d\n",errno);
     exit(-10);
   }
  }
  else {
    /* SERVER */
    listen(p->servicefd, 5);
    p->commfd = accept(p->servicefd, (struct sockaddr *) &(p->prot.sin2),
                       &clen);

    if(p->commfd < 0){
      printf("Server: Accept Failed! errno=%d\n",errno);
      exit(-12);
    }
  }
}

void CleanUp(ArgStruct *p)
{
   char *quit="QUIT";
   if (p->tr)
   {
      write(p->commfd,quit, 5);
      read(p->commfd, quit, 5);
      close(p->commfd);
   }
   else
   {
      read(p->commfd,quit, 5);
      write(p->commfd,quit,5);
      close(p->commfd);
      close(p->servicefd);
   }

   finalizeIB(p);
}


void AfterAlignmentInit(ArgStruct *p)
{
  int bytesRead;

  /* Exchange buffer pointers and remote infiniband keys if doing rdma. Do
   * the exchange in this function because this will happen after any
   * memory alignment is done, which is important for getting the 
   * correct remote address.
  */
  if( p->prot.commtype == NP_COMM_RDMAWRITE || 
      p->prot.commtype == NP_COMM_RDMAWRITE_WITH_IMM ) {
     
     /* Send my receive buffer address
      */
     if(write(p->commfd, (void *)&p->r_buff, sizeof(void*)) < 0) {
        perror("NetPIPE: write of buffer address failed in AfterAlignmentInit");
        exit(-1);
     }
     
     LOGPRINTF("Sent buffer address: %p\n", p->r_buff);
     
     /* Send my remote key for accessing
      * my remote buffer via IB RDMA
      */
     if(write(p->commfd, (void *)&r_mr_hndl->rkey, sizeof(uint32_t)) < 0) {
        perror("NetPIPE: write of remote key failed in AfterAlignmentInit");
        exit(-1);
     }
  
     LOGPRINTF("Sent remote key: %d\n", r_mr_hndl->rkey);
     
     /* Read the sent data
      */
     bytesRead = readFully(p->commfd, (void *)&remote_address, sizeof(void*));
     if (bytesRead < 0) {
        perror("NetPIPE: read of buffer address failed in AfterAlignmentInit");
        exit(-1);
     } else if (bytesRead != sizeof(void*)) {
        perror("NetPIPE: partial read of buffer address in AfterAlignmentInit");
        exit(-1);
     }
     
     LOGPRINTF("Received remote address from other node: %p\n", remote_address);
     
     bytesRead = readFully(p->commfd, (void *)&remote_key, sizeof(uint32_t));
     if (bytesRead < 0) {
        perror("NetPIPE: read of remote key failed in AfterAlignmentInit");
        exit(-1);
     } else if (bytesRead != sizeof(uint32_t)) {
        perror("NetPIPE: partial read of remote key in AfterAlignmentInit");
        exit(-1);
     }
     
     LOGPRINTF("Received remote key from other node: %d\n", remote_key);

  }
}


void MyMalloc(ArgStruct *p, int bufflen, int soffset, int roffset)
{
  /* Allocate buffers */

  p->r_buff = malloc(bufflen+MAX(soffset,roffset));
  if(p->r_buff == NULL) {
    fprintf(stderr, "Error malloc'ing buffer\n");
    exit(-1);
  }

  if(p->cache) {

    /* Infiniband spec says we can register same memory region
     * more than once, so just copy buffer address. We will register
     * the same buffer twice with Infiniband.
     */
    p->s_buff = p->r_buff;

  } else {

    p->s_buff = malloc(bufflen+soffset);
    if(p->s_buff == NULL) {
      fprintf(stderr, "Error malloc'ing buffer\n");
      exit(-1);
    }

  }

  /* Register buffers with Infiniband */

  r_mr_hndl = ibv_reg_mr(pd_hndl, p->r_buff, bufflen + MAX(soffset, roffset),
			 IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  if(!r_mr_hndl)
        {
    fprintf(stderr, "Error registering recv buffer\n");
    exit(-1);
        }
        else
        {
         LOGPRINTF("Registered Recv Buffer\n");
        }

  s_mr_hndl = ibv_reg_mr(pd_hndl, p->s_buff, bufflen+soffset, IBV_ACCESS_LOCAL_WRITE);
  if(!s_mr_hndl) {
    fprintf(stderr, "Error registering send buffer\n");
    exit(-1);
  } else {
    LOGPRINTF("Registered Send Buffer\n");
  }

}
void FreeBuff(char *buff1, char *buff2)
{
  int ret;

  if(s_mr_hndl) {
    LOGPRINTF("Deregistering send buffer\n");
    ret = ibv_dereg_mr(s_mr_hndl);
    if(ret) {
      fprintf(stderr, "Error deregistering send mr\n");
    } else {
      s_mr_hndl = NULL;
    }
  }

  if(r_mr_hndl) {
    LOGPRINTF("Deregistering recv buffer\n");
    ret = ibv_dereg_mr(r_mr_hndl);
    if(ret) {
      fprintf(stderr, "Error deregistering recv mr\n");
    } else {
      r_mr_hndl = NULL;
    }
  }

  if(buff1 != NULL)
    free(buff1);

  if(buff2 != NULL)
    free(buff2);
}

