#ifndef JOB_H
#define JOB_H

#define MAX_JOBS 100

/*
 * Represents the current lifecycle state of a job.
 */
typedef enum {
    NEW,
    READY,
    RUNNING,
    COMPLETED
} JobStatus;

/*
 * Stores all information about a CPU scheduling job.
 */
typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
    JobStatus status;
} Job;

/*
 * Loads jobs from a file.
 * Returns 0 on success and non-zero on failure.
 */
int load_jobs(const char *path, Job jobs[], int capacity, int *count);

#endif
