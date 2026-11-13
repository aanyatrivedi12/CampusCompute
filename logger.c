#include "logger.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define QUEUE_SIZE 4
#define MESSAGE_SIZE 256
#define PRODUCER_COUNT 2
#define EVENTS_PER_PRODUCER 4

static char queue[QUEUE_SIZE][MESSAGE_SIZE];
static int head;
static int tail;
static int count;

static int active;
static int stopping;
static int write_failed;
static unsigned long produced;
static unsigned long consumed;

static FILE *log_file;
static pthread_t consumer_thread;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

static void *consume_events(void *unused)
{
    (void)unused;

    for (;;) {
        char message[MESSAGE_SIZE];

        pthread_mutex_lock(&mutex);

        while (count == 0 && !stopping) {
            printf("[Consumer] Queue empty: waiting\n");
            pthread_cond_wait(&not_empty, &mutex);
        }

        if (count == 0 && stopping) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        strcpy(message, queue[head]);
        head = (head + 1) % QUEUE_SIZE;
        count--;
        consumed++;

        printf("[Consumer] Removed: %s | queued=%d\n",
               message, count);

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        /* Only this consumer writes to the log file. */
        if (fprintf(log_file, "%s\n", message) < 0) {
            write_failed = 1;
        }
    }

    printf("[Consumer] Pending events drained; thread ending\n");
    return NULL;
}

/* Start/stop are called by the main thread, outside producer activity. */
int logger_start(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        fprintf(stderr, "Logger: a valid file path is required\n");
        return 1;
    }

    pthread_mutex_lock(&mutex);

    if (active) {
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "Logger: already running\n");
        return 1;
    }

    log_file = fopen(path, "a");
    if (log_file == NULL) {
        pthread_mutex_unlock(&mutex);
        perror("Logger: fopen");
        return 1;
    }

    head = 0;
    tail = 0;
    count = 0;
    stopping = 0;
    write_failed = 0;
    produced = 0;
    consumed = 0;

    int error = pthread_create(&consumer_thread, NULL,
                               consume_events, NULL);
    if (error != 0) {
        fclose(log_file);
        log_file = NULL;
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "Logger: pthread_create: %s\n",
                strerror(error));
        return 1;
    }

    active = 1;
    printf("[Logger] Started; capacity=%d; file=%s\n",
           QUEUE_SIZE, path);

    pthread_mutex_unlock(&mutex);
    return 0;
}

int logger_event(const char *message)
{
    pthread_mutex_lock(&mutex);

    if (!active) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    if (message == NULL || strlen(message) >= MESSAGE_SIZE) {
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "Logger: message is missing or too long\n");
        return 1;
    }

    while (count == QUEUE_SIZE && !stopping) {
        printf("[Producer] Queue full: waiting\n");
        pthread_cond_wait(&not_full, &mutex);
    }

    if (stopping) {
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "Logger: shutdown in progress\n");
        return 1;
    }

    strcpy(queue[tail], message);
    tail = (tail + 1) % QUEUE_SIZE;
    count++;
    produced++;

    printf("[Producer] Queued: %s | queued=%d\n", message, count);

    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mutex);

    return 0;
}

int logger_stop(void)
{
    pthread_mutex_lock(&mutex);

    if (!active) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    stopping = 1;
    pthread_cond_broadcast(&not_empty);
    pthread_cond_broadcast(&not_full);
    pthread_mutex_unlock(&mutex);

    int error = pthread_join(consumer_thread, NULL);
    if (error != 0) {
        fprintf(stderr, "Logger: pthread_join: %s\n",
                strerror(error));
        return 1;
    }

    /* Joining the consumer makes its write status safe to read. */
    int result = write_failed;

    if (fclose(log_file) != 0) {
        perror("Logger: fclose");
        result = 1;
    }

    pthread_mutex_lock(&mutex);

    log_file = NULL;
    active = 0;

    printf("[Logger] Produced=%lu; consumed=%lu; remaining=%d\n",
           produced, consumed, count);

    if (produced != consumed || count != 0) {
        result = 1;
    }

    printf("[Logger] Consumer joined; log file closed\n");
    pthread_mutex_unlock(&mutex);

    if (result != 0) {
        fprintf(stderr, "Logger: logging did not finish successfully\n");
    }

    return result;
}

typedef struct {
    int id;
    int failed;
} ProducerInfo;

static void *produce_events(void *argument)
{
    ProducerInfo *info = argument;

    for (int i = 1; i <= EVENTS_PER_PRODUCER; i++) {
        char message[MESSAGE_SIZE];

        snprintf(message, sizeof(message),
                 "Producer %d: event %d", info->id, i);

        if (logger_event(message) != 0) {
            info->failed = 1;
            break;
        }
    }

    printf("[Producer %d] Thread ending\n", info->id);
    return NULL;
}

int run_logger_demo(void)
{
    pthread_t producers[PRODUCER_COUNT];
    ProducerInfo info[PRODUCER_COUNT];
    int started = 0;
    int result = 0;

    if (logger_start("activity.log") != 0) {
        return 1;
    }

    for (int i = 0; i < PRODUCER_COUNT; i++) {
        info[i].id = i + 1;
        info[i].failed = 0;

        int error = pthread_create(&producers[i], NULL,
                                   produce_events, &info[i]);
        if (error != 0) {
            fprintf(stderr, "Producer creation: %s\n",
                    strerror(error));
            result = 1;
            break;
        }

        started++;
    }

    for (int i = 0; i < started; i++) {
        int error = pthread_join(producers[i], NULL);

        if (error != 0) {
            fprintf(stderr, "Producer join: %s\n",
                    strerror(error));
            result = 1;
        } else if (info[i].failed) {
            result = 1;
        }
    }

    printf("[Demo] Joined %d producer threads\n", started);

    if (logger_stop() != 0) {
        result = 1;
    }

    return result;
}
