#define _POSIX_C_SOURCE 200809L

#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char message[256];
} SharedData;

/* A timeout prevents a failed partner from causing an endless wait. */
static int wait_for_signal(sem_t *sem)
{
    struct timespec deadline;

    if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) {
        perror("clock_gettime");
        return 1;
    }

    deadline.tv_sec += 5;

    while (sem_timedwait(sem, &deadline) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("sem_timedwait");
        return 1;
    }

    return 0;
}

static int check_semaphore_removed(const char *name)
{
    sem_t *sem = sem_open(name, 0);

    if (sem != SEM_FAILED) {
        sem_close(sem);
        fprintf(stderr, "[Check] Semaphore still exists: %s\n", name);
        return 1;
    }

    if (errno != ENOENT) {
        perror("Checking semaphore removal");
        return 1;
    }

    printf("[Check] Semaphore removed: %s\n", name);
    return 0;
}

int run_ipc_demo(void)
{
    char shm_name[80];
    char request_name[80];
    char reply_name[80];

    int fd = -1;
    int shm_created = 0;
    int result = 1;
    int status = 0;

    pid_t child = -1;

    SharedData *data = MAP_FAILED;
    sem_t *request = SEM_FAILED;
    sem_t *reply = SEM_FAILED;

    long parent_pid = (long)getpid();

    snprintf(shm_name, sizeof(shm_name),
             "/campus_shm_%ld", parent_pid);
    snprintf(request_name, sizeof(request_name),
             "/campus_request_%ld", parent_pid);
    snprintf(reply_name, sizeof(reply_name),
             "/campus_reply_%ld", parent_pid);

    fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);

    if (fd == -1) {
        perror("shm_open");
        goto cleanup;
    }

    shm_created = 1;

    if (ftruncate(fd, (off_t)sizeof(SharedData)) == -1) {
        perror("ftruncate");
        goto cleanup;
    }

    data = mmap(NULL, sizeof(SharedData),
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }

    request = sem_open(request_name, O_CREAT | O_EXCL, 0600, 0);

    if (request == SEM_FAILED) {
        perror("sem_open request");
        goto cleanup;
    }

    reply = sem_open(reply_name, O_CREAT | O_EXCL, 0600, 0);

    if (reply == SEM_FAILED) {
        perror("sem_open reply");
        goto cleanup;
    }

    printf("[IPC] Created shared memory: %s\n", shm_name);
    printf("[IPC] Created request and reply semaphores\n");

    fflush(NULL);
    child = fork();

    if (child == -1) {
        perror("fork");
        goto cleanup;
    }

    if (child == 0) {
        int child_result = 1;

        printf("[Child %ld] Waiting for request semaphore\n",
               (long)getpid());
        fflush(stdout);

        if (wait_for_signal(request) == 0) {
            printf("[Child] Read shared memory: %s\n",
                   data->message);

            snprintf(data->message, sizeof(data->message),
                     "Hello parent! Child received your message.");

            printf("[Child] Reply written; posting reply semaphore\n");

            if (sem_post(reply) == -1) {
                perror("sem_post reply");
            } else {
                child_result = 0;
            }
        }

        /* Each process releases its own handles and mapping. */
        if (munmap(data, sizeof(SharedData)) == -1) {
            perror("Child munmap");
            child_result = 1;
        }

        if (close(fd) == -1) {
            perror("Child close");
            child_result = 1;
        }

        if (sem_close(request) == -1) {
            perror("Child sem_close request");
            child_result = 1;
        }

        if (sem_close(reply) == -1) {
            perror("Child sem_close reply");
            child_result = 1;
        }

        fflush(NULL);
        _exit(child_result);
    }

    snprintf(data->message, sizeof(data->message),
             "Hello child! This is CampusCompute.");

    printf("[Parent %ld] Request written; posting request semaphore\n",
           parent_pid);

    if (sem_post(request) == -1) {
        perror("sem_post request");
        goto cleanup;
    }

    printf("[Parent] Waiting for reply semaphore\n");
    fflush(stdout);

    if (wait_for_signal(reply) != 0) {
        goto cleanup;
    }

    printf("[Parent] Read shared memory: %s\n", data->message);

    /*
     * Wait without blocking indefinitely, even if the child encounters
     * an unexpected problem after posting its reply.
     */
    for (int attempt = 0; attempt < 50; attempt++) {
        pid_t waited = waitpid(child, &status, WNOHANG);

        if (waited == child) {
            child = -1;

            if (WIFEXITED(status)) {
                printf("[Parent] Child collected; exit status=%d\n",
                       WEXITSTATUS(status));

                if (WEXITSTATUS(status) == 0) {
                    result = 0;
                }
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "[Parent] Child ended by signal %d\n",
                        WTERMSIG(status));
            }

            goto cleanup;
        }

        if (waited == -1 && errno != EINTR) {
            perror("waitpid");
            goto cleanup;
        }

        struct timespec pause = {0, 100000000L};
        nanosleep(&pause, NULL);
    }

    fprintf(stderr, "[Parent] Child completion timed out\n");

cleanup:
    /* On failure, stop and collect only the child created by this run. */
    if (child > 0) {
        if (kill(child, SIGKILL) == -1 && errno != ESRCH) {
            perror("kill child");
        }

        while (waitpid(child, &status, 0) == -1) {
            if (errno != EINTR) {
                perror("cleanup waitpid");
                break;
            }
        }
    }

    if (data != MAP_FAILED &&
        munmap(data, sizeof(SharedData)) == -1) {
        perror("Parent munmap");
        result = 1;
    }

    if (fd != -1 && close(fd) == -1) {
        perror("Parent close");
        result = 1;
    }

    /*
     * A valid handle means this invocation successfully created
     * that semaphore using O_EXCL.
     */
    if (request != SEM_FAILED) {
        if (sem_close(request) == -1) {
            perror("Parent sem_close request");
            result = 1;
        }

        if (sem_unlink(request_name) == -1) {
            perror("sem_unlink request");
            result = 1;
        }

        if (check_semaphore_removed(request_name) != 0) {
            result = 1;
        }
    }

    if (reply != SEM_FAILED) {
        if (sem_close(reply) == -1) {
            perror("Parent sem_close reply");
            result = 1;
        }

        if (sem_unlink(reply_name) == -1) {
            perror("sem_unlink reply");
            result = 1;
        }

        if (check_semaphore_removed(reply_name) != 0) {
            result = 1;
        }
    }

    if (shm_created) {
        if (shm_unlink(shm_name) == -1) {
            perror("shm_unlink");
            result = 1;
        }

        int check_fd = shm_open(shm_name, O_RDWR, 0600);

        if (check_fd != -1) {
            fprintf(stderr, "[Check] Shared memory still exists\n");
            close(check_fd);
            result = 1;
        } else if (errno == ENOENT) {
            printf("[Check] Shared memory removed: %s\n", shm_name);
        } else {
            perror("Checking shared-memory removal");
            result = 1;
        }
    }

    if (result == 0) {
        printf("[IPC] Exchange and cleanup completed successfully\n");
    }

    return result;
}
