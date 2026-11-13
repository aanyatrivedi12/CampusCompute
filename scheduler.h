#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "job.h"

/* First Come First Serve scheduling */
int run_fcfs(const Job jobs[], int count);

/* Round Robin scheduling */
int run_rr(const Job jobs[], int count, int quantum);

/* Non-preemptive Priority scheduling */
int run_priority(const Job jobs[], int count);

/* Compare all three scheduling algorithms */
int compare_schedulers(const Job jobs[], int count, int quantum);

#endif
