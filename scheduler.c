#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct {
    int id;
    long long start;
    long long end;
} Segment;

typedef struct {
    Segment *items;
    int count;
    int capacity;
} Timeline;

typedef struct {
    long long ct[MAX_JOBS];
    long long tat[MAX_JOBS];
    long long wt[MAX_JOBS];
} Results;

/* Validate job information before scheduling. */
static int validate_jobs(const Job jobs[], int count)
{
    if (jobs == NULL || count <= 0 || count > MAX_JOBS) {
        fprintf(stderr, "Error: Invalid number of jobs.\n");
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (jobs[i].id <= 0 || jobs[i].arrival < 0 ||
            jobs[i].burst <= 0 || jobs[i].priority < 0) {
            fprintf(stderr, "Error: Invalid job data.\n");
            return 0;
        }

        for (int j = i + 1; j < count; j++) {
            if (jobs[i].id == jobs[j].id) {
                fprintf(stderr, "Error: Duplicate job ID.\n");
                return 0;
            }
        }
    }

    return 1;
}

/* Add a segment to the dynamically allocated timeline. */
static int add_segment(Timeline *timeline, int id,
                       long long start, long long end)
{
    if (start == end) {
        return 1;
    }

    if (timeline->count == timeline->capacity) {
        int new_capacity = timeline->capacity == 0
                               ? 16
                               : timeline->capacity * 2;

        if (new_capacity < timeline->capacity ||
            (size_t)new_capacity > (size_t)-1 / sizeof(Segment)) {
            return 0;
        }

        Segment *temp = realloc(
            timeline->items,
            (size_t)new_capacity * sizeof(Segment)
        );

        if (temp == NULL) {
            return 0;
        }

        timeline->items = temp;
        timeline->capacity = new_capacity;
    }

    Segment *last = timeline->count > 0
                        ? &timeline->items[timeline->count - 1]
                        : NULL;

    if (last != NULL && last->id == id && last->end == start) {
        last->end = end;
        return 1;
    }

    timeline->items[timeline->count].id = id;
    timeline->items[timeline->count].start = start;
    timeline->items[timeline->count].end = end;
    timeline->count++;

    return 1;
}

/* Sort job indices by arrival time, then job ID. */
static void sort_arrival(const Job jobs[], int order[], int count)
{
    for (int i = 0; i < count; i++) {
        order[i] = i;
    }

    for (int i = 1; i < count; i++) {
        int key = order[i];
        int j = i - 1;

        while (j >= 0 &&
               (jobs[order[j]].arrival > jobs[key].arrival ||
                (jobs[order[j]].arrival == jobs[key].arrival &&
                 jobs[order[j]].id > jobs[key].id))) {
            order[j + 1] = order[j];
            j--;
        }

        order[j + 1] = key;
    }
}

/* Print Gantt chart and calculate scheduling statistics. */
static void print_results(const char *name, const Job jobs[],
                          int count, const Timeline *timeline,
                          Results *results)
{
    long long total_tat = 0;
    long long total_wt = 0;

    printf("\n========== %s ==========\n", name);
    printf("\nGantt Chart:\n");

    for (int i = 0; i < timeline->count; i++) {
        Segment s = timeline->items[i];

        if (s.id == -1) {
            printf("[IDLE %lld-%lld] ", s.start, s.end);
        } else {
            printf("[J%d %lld-%lld] ",
                   s.id, s.start, s.end);
        }
    }

    printf("\n\n");
    printf("%-8s %-12s %-12s %-12s\n",
           "Job", "CT", "TAT", "WT");

    for (int i = 0; i < count; i++) {
        results->tat[i] =
            results->ct[i] - jobs[i].arrival;

        results->wt[i] =
            results->tat[i] - jobs[i].burst;

        total_tat += results->tat[i];
        total_wt += results->wt[i];

        printf("J%-7d %-12lld %-12lld %-12lld\n",
               jobs[i].id,
               results->ct[i],
               results->tat[i],
               results->wt[i]);
    }

    printf("\nAverage Turnaround Time: %.2f\n",
           (double)total_tat / count);

    printf("Average Waiting Time: %.2f\n",
           (double)total_wt / count);
}

/* First Come First Serve scheduling. */
int run_fcfs(const Job jobs[], int count)
{
    if (!validate_jobs(jobs, count)) {
        return 1;
    }

    int order[MAX_JOBS];
    Results results = {0};
    Timeline timeline = {0};

    long long time = 0;

    sort_arrival(jobs, order, count);

    for (int i = 0; i < count; i++) {
        int index = order[i];

        if (time < jobs[index].arrival) {
            if (!add_segment(&timeline, -1,
                             time, jobs[index].arrival)) {
                goto failure;
            }

            time = jobs[index].arrival;
        }

        long long start = time;
        time += jobs[index].burst;

        if (!add_segment(&timeline, jobs[index].id,
                         start, time)) {
            goto failure;
        }

        results.ct[index] = time;
    }

    print_results("FCFS Scheduling", jobs, count,
                  &timeline, &results);

    free(timeline.items);
    return 0;

failure:
    fprintf(stderr, "Error: Unable to allocate timeline.\n");
    free(timeline.items);
    return 1;
}

/* Round Robin scheduling. */
int run_rr(const Job jobs[], int count, int quantum)
{
    if (!validate_jobs(jobs, count) || quantum <= 0) {
        fprintf(stderr, "Error: Quantum must be positive.\n");
        return 1;
    }

    int order[MAX_JOBS];
    int queue[MAX_JOBS + 1];
    int remaining[MAX_JOBS];

    int front = 0;
    int rear = 0;
    int next = 0;
    int completed = 0;

    Results results = {0};
    Timeline timeline = {0};

    long long time = 0;

    sort_arrival(jobs, order, count);

    for (int i = 0; i < count; i++) {
        remaining[i] = jobs[i].burst;
    }

    while (completed < count) {

        /* Add all jobs that have arrived. */
        while (next < count &&
               jobs[order[next]].arrival <= time) {
            queue[rear] = order[next];
            rear = (rear + 1) % (MAX_JOBS + 1);
            next++;
        }

        /* CPU remains idle if no job is ready. */
        if (front == rear) {
            long long old_time = time;
            time = jobs[order[next]].arrival;

            if (!add_segment(&timeline, -1, old_time, time)) {
                goto failure;
            }

            continue;
        }

        int index = queue[front];
        front = (front + 1) % (MAX_JOBS + 1);

        int execution = remaining[index] < quantum
                            ? remaining[index]
                            : quantum;

        long long start = time;
        time += execution;
        remaining[index] -= execution;

        if (!add_segment(&timeline, jobs[index].id,
                         start, time)) {
            goto failure;
        }

        if (remaining[index] == 0) {
            results.ct[index] = time;
            completed++;
        }

        /*
         * Enqueue newly arrived jobs before re-queuing
         * the unfinished current job.
         */
        while (next < count &&
               jobs[order[next]].arrival <= time) {
            queue[rear] = order[next];
            rear = (rear + 1) % (MAX_JOBS + 1);
            next++;
        }

        if (remaining[index] > 0) {
            queue[rear] = index;
            rear = (rear + 1) % (MAX_JOBS + 1);
        }
    }

    print_results("Round Robin Scheduling", jobs, count,
                  &timeline, &results);

    free(timeline.items);
    return 0;

failure:
    fprintf(stderr, "Error: Unable to allocate timeline.\n");
    free(timeline.items);
    return 1;
}

/* Non-preemptive Priority scheduling. */
int run_priority(const Job jobs[], int count)
{
    if (!validate_jobs(jobs, count)) {
        return 1;
    }

    int completed[MAX_JOBS] = {0};

    Results results = {0};
    Timeline timeline = {0};

    long long time = 0;

    for (int finished = 0; finished < count; finished++) {
        int selected = -1;

        for (int i = 0; i < count; i++) {
            if (completed[i] ||
                jobs[i].arrival > time) {
                continue;
            }

            if (selected == -1 ||
                jobs[i].priority < jobs[selected].priority ||
                (jobs[i].priority == jobs[selected].priority &&
                 jobs[i].arrival < jobs[selected].arrival) ||
                (jobs[i].priority == jobs[selected].priority &&
                 jobs[i].arrival == jobs[selected].arrival &&
                 jobs[i].id < jobs[selected].id)) {
                selected = i;
            }
        }

        if (selected == -1) {
            long long next_arrival = LLONG_MAX;

            for (int i = 0; i < count; i++) {
                if (!completed[i] &&
                    jobs[i].arrival < next_arrival) {
                    next_arrival = jobs[i].arrival;
                }
            }

            if (!add_segment(&timeline, -1,
                             time, next_arrival)) {
                goto failure;
            }

            time = next_arrival;
            finished--;
            continue;
        }

        long long start = time;
        time += jobs[selected].burst;

        if (!add_segment(&timeline, jobs[selected].id,
                         start, time)) {
            goto failure;
        }

        results.ct[selected] = time;
        completed[selected] = 1;
    }

    print_results("Priority Scheduling", jobs, count,
                  &timeline, &results);

    free(timeline.items);
    return 0;

failure:
    fprintf(stderr, "Error: Unable to allocate timeline.\n");
    free(timeline.items);
    return 1;
}

/* Compare all three scheduling algorithms. */
int compare_schedulers(const Job jobs[], int count, int quantum)
{
    if (!validate_jobs(jobs, count) || quantum <= 0) {
        fprintf(stderr, "Error: Invalid comparison arguments.\n");
        return 1;
    }

    printf("\n========== SCHEDULER COMPARISON ==========\n");

    if (run_fcfs(jobs, count) != 0) {
        return 1;
    }

    if (run_rr(jobs, count, quantum) != 0) {
        return 1;
    }

    if (run_priority(jobs, count) != 0) {
        return 1;
    }

    return 0;
}
