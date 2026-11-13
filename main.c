#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "job.h"
#include "scheduler.h"
#include "banker.h"
#include "logger.h"
#include "ipc.h"
#include "worker.h"

#define LOG_FILE "activity.log"

/* Display available commands. */
static void show_help(void)
{
    printf("\n========== CAMPUSCOMPUTE ==========\n");
    printf("Intelligent Job Scheduling and Resource Management\n\n");

    printf("Available Commands:\n");
    printf("  help                         Show this menu\n");
    printf("  fcfs <job_file>              FCFS scheduling\n");
    printf("  rr <job_file> <quantum>      Round Robin scheduling\n");
    printf("  priority <job_file>          Priority scheduling\n");
    printf("  compare <job_file> <quantum> Compare schedulers\n");
    printf("  banker <file>                Run Banker's algorithm\n");
    printf("  logger                       Run logger demonstration\n");
    printf("  ipc                          Run IPC demonstration\n");
    printf("  worker normal                Run normal child worker\n");
    printf("  worker fail                  Simulate worker failure\n");
    printf("  limit                        Run resource limit demo\n");
    printf("  demo                         Run integrated demonstration\n");

    printf("\n===================================\n");
}

/* Convert a positive command-line integer safely. */
static int parse_positive_int(const char *text, int *value)
{
    char *end;
    long number;

    if (text == NULL || value == NULL || *text == '\0') {
        return 0;
    }

    errno = 0;
    number = strtol(text, &end, 10);

    if (errno == ERANGE || *end != '\0' ||
        number <= 0 || number > INT_MAX) {
        return 0;
    }

    *value = (int)number;
    return 1;
}

/* Load jobs from a file. */
static int load_job_data(const char *path,
                         Job jobs[], int *count)
{
    if (load_jobs(path, jobs, MAX_JOBS, count) != 0) {
        return 0;
    }

    return 1;
}

/* Run a scheduling algorithm with logger support. */
static int run_scheduling_command(
    const char *algorithm,
    const char *path,
    int quantum)
{
    Job jobs[MAX_JOBS];
    int count = 0;
    int result = 1;
    int logger_started = 0;

    if (!load_job_data(path, jobs, &count)) {
        return 1;
    }

    if (logger_start(LOG_FILE) != 0) {
        fprintf(stderr, "Error: Could not start logger.\n");
        return 1;
    }

    logger_started = 1;

    logger_event("CPU scheduling operation started.");

    if (strcmp(algorithm, "fcfs") == 0) {
        result = run_fcfs(jobs, count);
    } else if (strcmp(algorithm, "rr") == 0) {
        result = run_rr(jobs, count, quantum);
    } else if (strcmp(algorithm, "priority") == 0) {
        result = run_priority(jobs, count);
    } else if (strcmp(algorithm, "compare") == 0) {
        result = compare_schedulers(jobs, count, quantum);
    }

    logger_event("CPU scheduling operation completed.");

    if (logger_started && logger_stop() != 0) {
        fprintf(stderr, "Warning: Logger shutdown failed.\n");
        result = 1;
    }

    return result;
}

/* Run Banker's algorithm with logger support. */
static int run_banker_command(const char *path)
{
    int result;

    if (logger_start(LOG_FILE) != 0) {
        fprintf(stderr, "Error: Could not start logger.\n");
        return 1;
    }

    logger_event("Banker's algorithm started.");

    result = run_banker_file(path);

    logger_event("Banker's algorithm completed.");

    if (logger_stop() != 0) {
        fprintf(stderr, "Warning: Logger shutdown failed.\n");
        return 1;
    }

    return result;
}

/* Run the complete integrated demonstration. */
static int run_demo(void)
{
    Job jobs[MAX_JOBS];
    int count = 0;
    int result = 0;

    printf("\n========== CAMPUSCOMPUTE DEMO ==========\n");

    if (!load_job_data("tests/jobs.txt", jobs, &count)) {
        return 1;
    }

    if (logger_start(LOG_FILE) != 0) {
        fprintf(stderr, "Error: Could not start logger.\n");
        return 1;
    }

    logger_event("Integrated demonstration started.");

    printf("\n--- CPU SCHEDULING ---\n");

    if (compare_schedulers(jobs, count, 2) != 0) {
        result = 1;
    }

    printf("\n--- BANKER'S SAFE STATE ---\n");

    if (run_banker_file("tests/banker_safe.txt") != 0) {
        result = 1;
    }

    printf("\n--- BANKER'S UNSAFE STATE ---\n");

    if (run_banker_file("tests/banker_unsafe.txt") != 0) {
        result = 1;
    }

    logger_event("Scheduling and Banker demonstrations completed.");

    if (logger_stop() != 0) {
        fprintf(stderr, "Error: Logger shutdown failed.\n");
        return 1;
    }

    printf("\n--- LOGGER DEMONSTRATION ---\n");

    if (run_logger_demo() != 0) {
        result = 1;
    }

    printf("\n--- IPC DEMONSTRATION ---\n");

    if (run_ipc_demo() != 0) {
        result = 1;
    }

    printf("\n--- NORMAL WORKER ---\n");

    if (run_worker_normal() != 0) {
        result = 1;
    }

    printf("\n--- SIMULATED WORKER FAILURE ---\n");

    if (run_worker_failure() != 0) {
        result = 1;
    }

    printf("\n--- RESOURCE LIMIT DEMONSTRATION ---\n");

    if (run_resource_limit_demo() != 0) {
        result = 1;
    }

    if (result == 0) {
        printf("\nCampusCompute demonstration completed successfully.\n");
    } else {
        printf("\nCampusCompute demonstration completed with errors.\n");
    }

    return result;
}

/* Main command-line interface. */
int main(int argc, char *argv[])
{
    int quantum;

    if (argc < 2) {
        show_help();
        return 1;
    }

    if (strcmp(argv[1], "help") == 0) {
        if (argc != 2) {
            fprintf(stderr, "Usage: campuscompute help\n");
            return 1;
        }

        show_help();
        return 0;
    }

    if (strcmp(argv[1], "fcfs") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: campuscompute fcfs <job_file>\n");
            return 1;
        }

        return run_scheduling_command("fcfs", argv[2], 0);
    }

    if (strcmp(argv[1], "rr") == 0) {
        if (argc != 4 ||
            !parse_positive_int(argv[3], &quantum)) {
            fprintf(stderr,
                    "Usage: campuscompute rr <job_file> <quantum>\n");
            return 1;
        }

        return run_scheduling_command("rr", argv[2], quantum);
    }

    if (strcmp(argv[1], "priority") == 0) {
        if (argc != 3) {
            fprintf(stderr,
                    "Usage: campuscompute priority <job_file>\n");
            return 1;
        }

        return run_scheduling_command("priority", argv[2], 0);
    }

    if (strcmp(argv[1], "compare") == 0) {
        if (argc != 4 ||
            !parse_positive_int(argv[3], &quantum)) {
            fprintf(stderr,
                    "Usage: campuscompute compare <job_file> <quantum>\n");
            return 1;
        }

        return run_scheduling_command("compare", argv[2], quantum);
    }

    if (strcmp(argv[1], "banker") == 0) {
        if (argc != 3) {
            fprintf(stderr,
                    "Usage: campuscompute banker <file>\n");
            return 1;
        }

        return run_banker_command(argv[2]);
    }

    if (strcmp(argv[1], "logger") == 0 && argc == 2) {
        return run_logger_demo();
    }

    if (strcmp(argv[1], "ipc") == 0 && argc == 2) {
        return run_ipc_demo();
    }

    if (strcmp(argv[1], "worker") == 0 && argc == 3) {
        if (strcmp(argv[2], "normal") == 0) {
            return run_worker_normal();
        }

        if (strcmp(argv[2], "fail") == 0) {
            return run_worker_failure();
        }

        fprintf(stderr, "Usage: campuscompute worker normal|fail\n");
        return 1;
    }

    if (strcmp(argv[1], "limit") == 0 && argc == 2) {
        return run_resource_limit_demo();
    }

    if (strcmp(argv[1], "demo") == 0 && argc == 2) {
        return run_demo();
    }

    fprintf(stderr, "Error: Unknown command or invalid arguments.\n");
    show_help();

    return 1;
}
