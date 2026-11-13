#ifndef WORKER_H
#define WORKER_H

/* Create a child and verify successful completion. */
int run_worker_normal(void);

/* Detect a controlled simulated child failure. */
int run_worker_failure(void);

/* Apply and verify a resource limit in a child process. */
int run_resource_limit_demo(void);

#endif
