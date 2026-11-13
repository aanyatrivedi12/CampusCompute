#ifndef LOGGER_H
#define LOGGER_H

/* Start logging to the given file in append mode. */
int logger_start(const char *path);

/* Safely queue a copied message from a producer thread. */
int logger_event(const char *message);

/* Finish pending messages and stop the logger thread. */
int logger_stop(void);

/* Run the producer-consumer demonstration independently. */
int run_logger_demo(void);

#endif
