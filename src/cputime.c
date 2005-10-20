#include "stdio.h"
#include "netpipe.h"

#ifdef HAVE_GETRUSAGE

/* Global vars for CPUTime functions */

struct rusage prev_rusage, curr_rusage; /* Resource usage                  */
double utime, stime;	                /* User & system time used         */
double best_utime, best_stime;          /* Total user & system time used   */
double ut1, ut2, st1, st2;              /* User & system ctrs for variance */
double ut_var, st_var;	                /* Variance in user & system time  */

void CPUTime_Init()
{
  ut1 = ut2 = st1 = st2 = 0.0;
  best_utime = best_stime = LONGTIME;
}

void CPUTime_Start()
{
  int rc;
  rc=getrusage(RUSAGE_SELF, &prev_rusage);
  if(rc==-1) {
    fprintf(stderr, "error RUSAGE_SELF\n");
    exit(-1);
  }

}

void CPUTime_Finish(int trials)
{
  getrusage(RUSAGE_SELF, &curr_rusage);
 
  utime = ((curr_rusage.ru_utime.tv_sec -
            prev_rusage.ru_utime.tv_sec) + (double)
           (curr_rusage.ru_utime.tv_usec -
            prev_rusage.ru_utime.tv_usec) * 1.0E-6) / trials;

  stime = ((curr_rusage.ru_stime.tv_sec -
            prev_rusage.ru_stime.tv_sec) + (double)
           (curr_rusage.ru_stime.tv_usec -
            prev_rusage.ru_stime.tv_usec) * 1.0E-6) / trials;

  ut2 += utime * utime;
  st2 += stime * stime;
  ut1 += utime;
  st1 += stime;

  if ((utime + stime) < (best_utime + best_stime)) {
    best_utime = utime;
    best_stime = stime;
  }
}

void CPUTime_Variance()
{
  ut_var = ut2/TRIALS - (ut1/TRIALS) * (ut1/TRIALS);
  st_var = st2/TRIALS - (st1/TRIALS) * (st1/TRIALS);
}

void CPUTime_Output(void* output)
{
  double user_total      = ut1 / (double) TRIALS;
  double system_total    = st1 / (double) TRIALS;

  fprintf((FILE*)output, " %lf %lf %lf %lf %lf", 
          user_total,
          system_total,
          user_total + system_total,
          ut_var,
          st_var);
}

#else  /* HAVE_GETRUSAGE */

/* If we don't have getrusage(), then just define empty functions so the
   compiler does not complain. */

void CPUTime_Init()               {}
void CPUTime_Start()              {}
void CPUTime_Finish(int trials)   {}
void CPUTime_Variance()           {}
void CPUTime_Output(void* output) {}

#endif /* HAVE_GETRUSAGE */


