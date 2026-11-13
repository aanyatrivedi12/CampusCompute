#define _POSIX_C_SOURCE 200809L

#include "worker.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Collect a child and check its actual exit status. */
static int collect_worker(pid_t child, int expected_status)
{
    int status;
    pid_t waited;

    do {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);

        printf("[Parent] Collected child %ld; exit status=%d\n",
               (long)child, exit_status);

        if (exit_status == expected_status) {
            printf("[Parent] Expected worker result verified\n");
            return 0;
        }

        fprintf(stderr, "[Parent] Unexpected exit status\n");
        return 1;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[Parent] Child terminated by signal %d\n",
                WTERMSIG(status));
    } else {
        fprintf(stderr, "[Parent] Unexpected child status\n");
    }

    return 1;
}

int run_worker_normal(void)
{
    fflush(NULL);

    pid_t child = fork();

    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        int sum = 0;

        for (int i = 1; i <= 100; i++) {
            sum += i;
        }

        printf("[Child %ld] Normal worker started\n",
               (long)getpid());
        printf("[Child] Sum of integers 1 to 100 = %d\n", sum);
        printf("[Child] Work completed; exiting with status 0\n");

        fflush(NULL);
        _exit(sum == 5050 ? 0 : 1);
    }

    printf("[Parent %ld] Created normal worker %ld\n",
           (long)getpid(), (long)child);

    return collect_worker(child, 0);
}

int run_worker_failure(void)
{
    fflush(NULL);

    pid_t child = fork();

    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        printf("[Child %ld] CONTROLLED SIMULATED FAILURE\n",
               (long)getpid());
        printf("[Child] Deliberately exiting with status 7\n");

        fflush(NULL);
        _exit(7);
    }

    printf("[Parent %ld] Created simulated-failure worker %ld\n",
           (long)getpid(), (long)child);

    int result = collect_worker(child, 7);

    if (result == 0) {
        printf("[Parent] Simulated failure detected correctly\n");
    }

    return result;
}

/* Runs only inside the resource-limit child. */
static int demonstrate_file_limit(int fd)
{
    struct rlimit limit;
    struct sigaction action;
    struct stat file_info;

    /*
     * Ignore SIGXFSZ so write() reports EFBIG instead of
     * terminating the child before it can explain the result.
     */
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGXFSZ, &action, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    if (getrlimit(RLIMIT_FSIZE, &limit) == -1) {
        perror("getrlimit");
        return 1;
    }

    if (limit.rlim_max != RLIM_INFINITY &&
        limit.rlim_max < (rlim_t)1024) {
        fprintf(stderr,
                "[Child] Existing hard limit is below 1024 bytes\n");
        return 1;
    }

    /* Keep the existing hard limit; lower only this child's soft limit. */
    limit.rlim_cur = 1024;

    if (setrlimit(RLIMIT_FSIZE, &limit) == -1) {
        perror("setrlimit");
        return 1;
    }

    printf("[Child %ld] RLIMIT_FSIZE soft limit: 1024 bytes\n",
           (long)getpid());
    printf("[Child] Attempting to write 2048 bytes\n");

    char buffer[512];
    memset(buffer, 'A', sizeof(buffer));

    size_t total = 0;
    int blocked = 0;

    while (total < 2048) {
        ssize_t written = write(fd, buffer, sizeof(buffer));

        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EFBIG) {
                printf("[Child] Further write blocked: %s\n",
                       strerror(errno));
                blocked = 1;
            } else {
                perror("write");
            }

            break;
        }

        if (written == 0) {
            fprintf(stderr, "[Child] Write made no progress\n");
            break;
        }

        total += (size_t)written;
    }

    if (fstat(fd, &file_info) == -1) {
        perror("fstat");
        return 1;
    }

    printf("[Child] Bytes written: %zu\n", total);
    printf("[Child] Actual file size: %lld bytes\n",
           (long long)file_info.st_size);

    if (blocked && total == 1024 && file_info.st_size == 1024) {
        printf("[Child] File-size limit effect verified\n");
        return 0;
    }

    fprintf(stderr, "[Child] Expected limit effect was not verified\n");
    return 1;
}

int run_resource_limit_demo(void)
{
    char path[] = "/tmp/campus_limit_XXXXXX";

    int fd = mkstemp(path);

    if (fd == -1) {
        perror("mkstemp");
        return 1;
    }

    printf("[Parent] Created temporary file: %s\n", path);
    fflush(NULL);

    pid_t child = fork();

    if (child == -1) {
        perror("fork");

        if (close(fd) == -1) {
            perror("close");
        }

        if (unlink(path) == -1) {
            perror("unlink");
        }

        return 1;
    }

    if (child == 0) {
        int result = demonstrate_file_limit(fd);

        if (close(fd) == -1) {
            perror("Child close");
            result = 1;
        }

        fflush(NULL);
        _exit(result);
    }

    int result = collect_worker(child, 0);

    if (close(fd) == -1) {
        perror("Parent close");
        result = 1;
    }

    if (unlink(path) == -1) {
        perror("unlink");
        result = 1;
    } else {
        printf("[Parent] Temporary file removed\n");
    }

    printf("[Parent] Resource limit applied only to the child\n");

    return result;
}
