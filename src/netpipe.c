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
/* Files needed for use:                                                     */
/*     * netpipe.c       ---- Driver source                                  */
/*     * netpipe.h       ---- General include file                           */
/*     * tcp.c           ---- TCP calls source                               */
/*     * tcp.h           ---- Include file for TCP calls and data structs    */
/*     * mpi.c           ---- MPI calls source                               */
/*     * pvm.c           ---- PVM calls source                               */
/*     * pvm.h           ---- Include file for PVM calls and data structs    */
/*     * tcgmsg.c        ---- TCGMSG calls source                            */
/*     * tcgmsg.h        ---- Include file for TCGMSG calls and data structs */
/*****************************************************************************/

#include "netpipe.h"
#if defined(ARMCI) || defined(MPI)
#include "mpi.h"
#endif

#if defined(MPLITE)
#include "mplite.h"
#endif

#ifdef TCGMSG
#include <sndrcv.h>
#endif

#ifdef PVM
    int         mytid;          /* the tid of the current process            */
#endif

extern char *optarg;

#if defined(MPI2)
extern MPI_Win win;
#endif

#if defined(MPI2) || defined(ARMCI)
extern char* buf_orig;
#endif

main(int argc, char **argv)
{
    FILE        *out;           /* Output data file                          */
    char        s[255];         /* Generic string                            */
    char        *memtmp,        /* store buffers to be sent out              */
                *memtmp_align;  /* after alignment                           */    
    char        *memtmp1,       /* receive data to this part of memory       */ 
                *memtmp1_align; /* after alignment                           */    
    int         *memcache;      /* used to flush cache                       */  
    int         b_usecache = 0; /* 1, more cache effect                      */
                                /* 0, little cache effect                    */

    int         len_buf_align,  /* meaningful when b_usecache is 0. buflen   */ 
                                /* rounded up to be divisible by 8           */
                num_buf_align,  /* meaningful when b_usecache is 0. number   */ 
                                /* of aligned buffers in memtmp              */
                num1_buf_align; /* meaningful when b_usecache is 0. number   */ 
                                /* of aligned buffers in memtmp1             */

    int         c,              /* option index                              */
                i, j, n, nq,    /* Loop indices                              */
                asyncReceive=0, /* Pre-post a receive buffer?                */
                bufoffset=0,    /* Align buffer to this                      */
                bufalign=16*1024,/* Boundary to align buffer to              */
                errFlag,        /* Error occurred in inner testing loop      */
                nrepeat,        /* Number of time to do the transmission     */
                len,            /* Number of bytes to be transmitted         */
                inc=0,          /* Increment value                           */
                trans=-1,       /* Transmitter flag. 1 if transmitting.      */
                detailflag=0,   /* Set to examine the signature curve detail */
                pert,           /* Perturbation value                        */
                start= 1,       /* Starting value for signature curve        */
                end=MAXINT,     /* Ending value for signature curve          */
                streamopt=0;    /* Streaming mode flag                       */
   
    ArgStruct   args;           /* Argumentsfor all the calls                */

    double      t, t0, t1, t2,  /* Time variables                            */
                tlast,          /* Time for the last transmission            */
                latency;        /* Network message latency                   */

    Data        bwdata[NSAMP];  /* Bandwidth curve data                      */

    short       port=DEFPORT;   /* Port number for connection                */
    int         onlyTwoComm=0;  /* If running on more than two nodes, only
                                   two nodes will actually communicate       */


#ifdef MPI
    MPI_Init(&argc, &argv);
#ifdef MPI2
    args.prot.use_get = 0;  /* Default to put   */
    args.prot.no_fence = 0; /* Default to fence */
#endif
#endif

#ifdef ARMCI
    MPI_Init(&argc, &argv);
    bufalign=0; /* changing buffer alignment throws off the list of pointers
                   to shared memory */
    fprintf(stderr, "Buffer alignment is off (Required for this module)\n");
#endif

#ifdef PVM
    mytid = pvm_mytid();
#endif

#ifdef TCGMSG
    PBEGIN_(argc, argv);
#endif

#ifdef LAPI
    int      rc;                 /* the return value of LAPI calls           */
    pRepeat = &nrepeat;
#endif

#ifdef GPSHMEM
    gpshmem_init(&argc, &argv);
#endif

    strcpy(s, "np.out");

#ifndef MPI
#ifndef ARMCI
#ifndef PVM
#ifndef TCGMSG
#ifndef LAPI
#ifndef SHMEM
    if(argc < 2) PrintUsage();
#endif
#endif
#endif
#endif
#endif
#endif

    args.source_node = 0;

#if defined(TCP)
    /* If possible, set the TCP buffers to 256 kB ( 512 kB Linux) */
    args.prot.sndbufsz = args.prot.rcvbufsz = 256000;
#endif

#if defined(TCGMSG)
    /* TCGMSG launches NPtcgmsg with a -master master_hostname
     * argument, so ignore all arguments and set them manually 
     * in netpipe.c instead.
     */

    printf("Command line options are disabled for the TCGMSG module.\n");
    printf("Edit modules/netpipe.c to set options manually if you want.\n");
#else

    /* Parse the arguments. See Usage for description */
    while ((c = getopt(argc, argv, "Pstzrgfh:p:o:A:O:l:u:i:b:a:c")) != -1)
    {
        switch(c)
        {
            case 'c': b_usecache = 1;
                      break;

            case 'o': strcpy(s,optarg);
                      break;

            case 't': trans = 1;
                      break;
            
            case 'r': trans = 0;
                      break;

            case 's': streamopt = 1;
                      printf("Streaming in one direction only\n");
                      fflush(stdout);
                      break;

            case 'l': /*detailflag = 1;*/
                      start = atoi(optarg);
                      if (start < 1)
                      {
                        fprintf(stderr,"Need a starting value >= 1\n");
                        exit(743);
                      }
                      break;

            case 'u': /*detailflag = 1;*/
                      end = atoi(optarg);
                      break;

            case 'i': detailflag = 1;
                      inc = atoi(optarg);
                      break;

            case 'b': /* -b # resets the buffer size, -b 0 keeps system defs */
#ifdef TCP
                      args.prot.sndbufsz = args.prot.rcvbufsz = atoi(optarg);
#endif
                      break;

            case 'A': bufalign = atoi(optarg);
                      break;

            case 'O': bufoffset = atoi(optarg);
                      break;

            case 'p': port = atoi(optarg);
                      break;
            
            case 'h': if (trans == 1)
                      {
                          args.host = (char *)malloc(strlen(optarg)+1);
                          strcpy(args.host, optarg);
                      }
                      else
                      {
                          fprintf(stderr, "Error: -t must be specified before -h\n");
                          exit(-11);
                      }
                      break;

            case 'z': args.source_node = -1;
                      break;

            case 'a': asyncReceive = 1;
                      printf("Preposting asynchronous receives\n");
                      fflush(stdout);
                      break;

#if defined(MPI2)
           case 'g': if(args.prot.no_fence == 1) {
                         fprintf(stderr, "-g and -f cannot be used together\n");
                         MPI_Finalize();
                         exit(-1);
                     }
                     args.prot.use_get = 1;
                     printf("Using MPI-2 Get instead of Put\n");
                     break;
            case 'f': if(args.prot.use_get == 1) {
                         fprintf(stderr, "-g and -f cannot be used together\n");
                         MPI_Finalize();
                         exit(-1);
                     }
                     args.prot.no_fence = 1;
                     bufalign = 0;
                     printf("Buffer alignment off (Required for no fence)\n");
                     break;
#endif      
            case 'T': onlyTwoComm = 1;
                      printf("Only two nodes will communicate\n");
                      fflush(stdout);
                      break;

            default: 
                     PrintUsage(); 
                     exit(-12);
       }
   }
#endif
   if (start > end)
   {
       fprintf(stderr, "Start MUST be LESS than end\n");
       exit(420132);
   }
   args.nbuff = TRIALS;
   args.tr = trans;
   args.port = port;

   Setup(&args);
   Establish(&args);

   if (args.tr)
   {
       if ((out = fopen(s, "w")) == NULL)
       {
           fprintf(stderr,"Can't open %s for output\n", s);
           exit(1);
       }
   }
   else
       out = stdout;
    /* 
    * Allocate memory  
    */
   if (!b_usecache)
   {
       if ( (memcache = (int *)malloc(MEMSIZE)) == NULL)
       {
           perror("malloc");
           exit(1);
       }
       mymemset(memcache, 0, MEMSIZE/sizeof(int)); 
#if defined(ARMCI)
       /* If transmitter, memtmp will correspond to memtmp1 on receiver
          This is necessary due to the way the linked-list pointer pairs
          are setup by armci_malloc in armci.c */
       if ( ( (args.tr ? memtmp : memtmp1) = (char *)armci_malloc(MEMSIZE)) 
            == NULL)
#else
       if ( (memtmp = (char *)malloc(MEMSIZE)) == NULL)
#endif
       {
           perror("malloc");
           exit(1);
       }
#if defined(ARMCI)
       /* Same as above, only reversed */
       if ( ( (args.tr ? memtmp1 : memtmp) = (char *)armci_malloc(MEMSIZE)) 
            == NULL)
#else
       if ( (memtmp1 =(char *)malloc(MEMSIZE)) == NULL)
#endif
       {
           perror("malloc");
           exit(1);
       }

       if (bufalign != 0)
       {
           memtmp_align  = memtmp + (bufalign - ( (long)memtmp % bufalign )
                           + bufoffset) % bufalign;
           memtmp1_align = memtmp1 + (bufalign - ( (long)memtmp1 % bufalign )
                           + bufoffset) % bufalign;
       }
       else
       {
         memtmp_align  = memtmp;
         memtmp1_align = memtmp1;
       }

#if defined(MPI2) || defined(ARMCI)
       /* These steps are required for MPI-2 module because usually the
          window is created in MyMalloc.  We use buf_orig to calculate
          the memory offset inside the MPI-2 and ARMCI modules. */
       buf_orig = memtmp_align;
#if defined(MPI2)
       MPI_Win_create(memtmp1_align,  MEMSIZE, 1, NULL, MPI_COMM_WORLD, &win);
#endif
#endif           
    
   }

   if (args.tr )
   {
       fprintf(stderr,"Now starting the main loop\n");
   }
   if (inc == 0)
   {
       /*Set a starting value for the message size increment. */
       inc = (start > 1) ? start / 2 : 1;
   } 
   tlast = 0;

   /* Main loop of benchmark */
   for (nq = n = 0, len = start, errFlag = 0; 
        n < NSAMP - 3 && tlast < STOPTM && len <= end && !errFlag; 
        len = len + inc, nq++ )
   {
       if (nq > 2 && !detailflag)
       {
           /*
             This has the effect of exponentially increasing the block
             size.  If detailflag is false, then the block size is
             linearly increased (the increment is not adjusted).
            */
           inc = ((nq % 2))? inc + inc: inc;
       }
       
       /* This is a perturbation loop to test nearby values */
       for (pert = (!detailflag && inc > PERT+1)? -PERT: 0;
            pert <= PERT; 
            n++, pert += (!detailflag && inc > PERT+1)? PERT: PERT+1)
       {
           /* Sync to prevent race condition in armci module */
           Sync(&args);

           /* Calculate how many times to repeat the experiment. */
           if (args.tr)
           {
               if (len == start)  /* The first try */
                   nrepeat = RUNTM/LATENCYMAX;
               else
                   nrepeat = MAX((RUNTM / ((double)args.bufflen /
                                  (args.bufflen - inc + 1.0) * tlast)), TRIALS);
               SendRepeat(&args, nrepeat);
           }
           else
           {
               RecvRepeat(&args, &nrepeat);
           }

           args.bufflen = len + pert;
           if (args.tr)
               fprintf(stderr,"%3d: %7d bytes %6d times --> ",
                       n,args.bufflen,nrepeat);
           if (b_usecache)
           {
               /* Allocate the buffer */
               if(MyMalloc(&args,args.bufflen+bufalign)<0) break;

               /*
                 Possibly align the data buffer: make memtmp and memtmp1
                 point to the original blocks (so they can be freed later),
                 then adjust args.buff and args.buff1 if the user requested it.
               */
               memtmp = args.buff;
               memtmp1 = args.buff1;
               if (bufalign != 0)
                 args.buff +=(bufalign - ( (long)args.buff % bufalign )
                              + bufoffset) % bufalign;
               if (bufalign != 0)
                 args.buff1 +=(bufalign - 
                               ((long)args.buff1 % bufalign) + bufoffset) % bufalign;
               
               /* 
                * The following assignment is only useful for testing shmem 
                * on cray T3E   - Xuehua Chen
                */
               args.buff[args.bufflen - 1] = 'b' + args.tr; 
            }
            else
            {
               /* 
                * buffer length rouded up to be devisible by bufalign
                */ 
               len_buf_align = args.bufflen + bufalign;
               if(bufalign != 0)
                 len_buf_align -= (int)args.bufflen % bufalign;
 
               /* 
                * number of buffers that have the same alignment in two
                * memory blockes seperately
                */ 
               num_buf_align  = ((long)memtmp  + MEMSIZE - (long)memtmp_align) 
                                / len_buf_align;
               num1_buf_align = ((long)memtmp1 + MEMSIZE - (long)memtmp1_align)
                                / len_buf_align;

               /* If we are using any of the following modules, we need to
                  initialize the last byte of each block before we flush
                  the cache, since the initialization would normally take
                  place in MyMalloc */
#if defined(MPI2) || defined(SHMEM) || defined(GPSHMEM) || defined(ARMCI)
               for (i = 0; i < num_buf_align; i++)
               *(memtmp_align  + i * len_buf_align 
                       + (args.bufflen -1)) = 'b' + args.tr;    

               for (i = 0; i < num1_buf_align; i++)
                    *(memtmp1_align + i * len_buf_align 
                    + (args.bufflen -1)) = 'b' + args.tr; 
#endif
                flushcache(memcache, MEMSIZE/sizeof(int));  
            }


/* NOTE: The buffer alignment does not work on the Paragon for some 
 *       reason.  Simply run NPparagon -A 0 to set the alignment to 0.
 *        - Dave Turner
 */


            /* Finally, we get to transmit or receive and time */

            if (args.tr)
            {
                /*
                   This is the transmitter: send the block TRIALS times, and
                   if we are not streaming, expect the receiver to return each
                   block.
                */

                bwdata[n].t = LONGTIME;
                t2 = t1 = 0;
                for (i = 0; i < TRIALS; i++)
                {
                    Sync(&args);

                    t0 = When();
                    for (j = 0; j < nrepeat; j++)
                    {
                        if (asyncReceive && !streamopt)
                        {
                            PrepareToReceive(&args);
                        }
                        if (!b_usecache)
                        {
                          args.buff = memtmp_align + 
                            ((i * nrepeat + j) % num_buf_align) * 
                            len_buf_align;
                        }

                        SendData(&args);

                        if (!streamopt)
                        {
                            if (!b_usecache)
                            {
                              args.buff = memtmp1_align + 
                                ((i *nrepeat + j) % num1_buf_align) * 
                                len_buf_align;

                            }

                            RecvData(&args);

                        }
                    }
                    t = (When() - t0)/((1 + !streamopt) * nrepeat);

/* NOTE: NetPIPE does each data point TRIALS times, bouncing the message
 * nrepeats times for each test, then reports the lowest of the TRIALS
 * times.  -Dave Turner
 */
                    if (!streamopt)
                    {
                        t2 += t*t;
                        t1 += t;
                        bwdata[n].t = MIN(bwdata[n].t, t);
                    }
                }
                if (!streamopt)
                    SendTime(&args, &bwdata[n].t);
                else
                    RecvTime(&args, &bwdata[n].t);

                if (!streamopt)
                    bwdata[n].variance = t2/TRIALS - t1/TRIALS * t1/TRIALS;

            }
            else
            {
                /*
                   This is the receiver: receive the block TRIALS times, and
                   if we are not streaming, send the block back to the
                   sender.
                */
                bwdata[n].t = LONGTIME;
                t2 = t1 = 0;
                for (i = 0; i < TRIALS; i++)
                {
                    if (asyncReceive)
                    {
                            PrepareToReceive(&args);
                    }

                    Sync(&args);

                    t0 = When();
                    for (j = 0; j < nrepeat; j++)
                    {
                        if (!b_usecache)
                        {
                          args.buff = memtmp1_align + 
                            ((i * nrepeat + j) % num1_buf_align) * 
                            len_buf_align; 
                        }

                        RecvData(&args);

                        if (asyncReceive && (j < nrepeat - 1))
                        {
                            PrepareToReceive(&args);
                        }
                        if (!streamopt)
                        {
                            if (!b_usecache)
                            {
                              args.buff = memtmp_align + 
                                ((i * nrepeat + j) % num_buf_align) * 
                                len_buf_align;

                            }

                            SendData(&args);

                        }
                    }
                    t = (When() - t0)/((1 + !streamopt) * nrepeat);

                    if (streamopt)
                    {
                        t2 += t*t;
                        t1 += t;
                        bwdata[n].t = MIN(bwdata[n].t, t);
                    }
                }
                if (!streamopt)
                    RecvTime(&args, &bwdata[n].t);
                else
                    SendTime(&args, &bwdata[n].t);

                if (streamopt)
                    bwdata[n].variance = t2/TRIALS - t1/TRIALS * t1/TRIALS;

            }

            tlast = bwdata[n].t;
            bwdata[n].bits = args.bufflen * CHARSIZE;
            bwdata[n].bps = bwdata[n].bits / (bwdata[n].t * 1024 * 1024);
            bwdata[n].repeat = nrepeat;
            
            if (args.tr)
            {
                fprintf(out,"%8d %lf %lf\n",
                      bwdata[n].bits / 8, bwdata[n].bps, bwdata[n].t);
                fflush(out);
            }
    
            if (b_usecache)
                FreeBuff(memtmp, memtmp1);
            
            if (args.tr )
              fprintf(stderr," %8.2lf Mbps in %10.2lf usec\n", 
                              bwdata[n].bps, tlast*1.0e6);

        } /* End of perturbation loop */

    } /* End of main loop  */
 

   if (!b_usecache) {
        FreeBuff(memtmp, memtmp1);
   }
    if (args.tr)
        fclose(out);
         
    CleanUp(&args);
}


/* Return the current time in seconds, using a double precision number.      */
double
When()
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
   static flag = 0;
   int    i; 

   flag = (flag + 1) % 2; 
   if ( flag == 0) 
       for (i = 0; i < n; i++)
           *(ptr + i) = *(ptr + i) + 1;
   else
       for (i = 0; i < n; i++) 
           *(ptr + i) = *(ptr + i) - 1; 
    
}
 
  
    
int PrintUsage()
{
    printf("\n NETPIPE USAGE \n\n");
    printf("A: specify buffers alignment e.g.: <-A 1024>\n");
    printf("a: asynchronous receive (a.k.a. preposted receive)\n");

#if defined(TCP)
    printf("b: specify TCP send/receive buffer sizes <set to 1 MB if allowed,"
           "use -b 0 to use system defaults>\n");
    printf("h: specify hostname <-h host>\n");
#endif

    printf("i: specify increment step size e.g. <-i 64>\n");
    printf("l: lower bound start value e.g. <-l 1>\n");
    printf("O: specify buffer offset e.g. <-O 127>\n");
    printf("o: specify output filename <-o fn>\n");

#if defined(TCP)
    printf("p: specify port e.g. <-p 5150>\n");
#endif

    printf("r: receiver\n");
    printf("s: stream option\n");
    printf("t: transmitter\n");
    printf("u: upper bound stop value e.g. <-u 1048576>\n");

#if defined(MPI)
    printf("z: receive messages using the 'anysource' flag (source = -1)\n");
#endif

#if defined(MPI2)
    printf("g: use get instead of put\n");
    printf("f: do not use fence during timing segment; may not work with\n");
    printf("   all MPI-2 implementations\n");
#endif

    printf("c: Allow cache effects <Default is to limit cache effects>\n");
    printf("\n");
    exit(-12);
    return (0);
}
