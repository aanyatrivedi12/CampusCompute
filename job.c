#include "job.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

/*
 * Parses a line containing integers.
 * Blank lines and comment lines beginning with # are ignored.
 */
static int parse_line(char *line, int values[], int max_values,
                      int *value_count)
{
    char *ptr = line;
    int count = 0;

    while (*ptr == ' ' || *ptr == '\t' ||
           *ptr == '\n' || *ptr == '\r') {
        ptr++;
    }

    if (*ptr == '\0' || *ptr == '#') {
        return 0;
    }

    while (*ptr != '\0' && *ptr != '#') {
        char *end;
        long number;

        while (*ptr == ' ' || *ptr == '\t' ||
               *ptr == '\n' || *ptr == '\r') {
            ptr++;
        }

        if (*ptr == '\0' || *ptr == '#') {
            break;
        }

        errno = 0;
        number = strtol(ptr, &end, 10);

        if (ptr == end || errno == ERANGE ||
            number < INT_MIN || number > INT_MAX) {
            return -1;
        }

        if (count >= max_values) {
            return -1;
        }

        values[count++] = (int)number;
        ptr = end;

        if (*ptr != '\0' && *ptr != '#' &&
            *ptr != ' ' && *ptr != '\t' &&
            *ptr != '\n' && *ptr != '\r') {
            return -1;
        }
    }

    *value_count = count;
    return 1;
}

/*
 * Loads and validates jobs from a file.
 */
int load_jobs(const char *path, Job jobs[], int capacity, int *count)
{
    FILE *file;
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;

    int expected = 0;
    int loaded = 0;
    int line_number = 0;
    int values[4];
    int value_count;
    int result;
    int count_found = 0;

    if (path == NULL || jobs == NULL || count == NULL ||
        capacity <= 0 || capacity > MAX_JOBS) {
        fprintf(stderr, "Error: Invalid arguments to load_jobs.\n");
        return 1;
    }

    *count = 0;

    file = fopen(path, "r");

    if (file == NULL) {
        perror("Error opening job file");
        return 1;
    }

    while ((line_length = getline(&line, &line_capacity, file)) != -1) {
        (void)line_length;
        line_number++;

        value_count = 0;

        result = parse_line(line, values, 4, &value_count);

        if (result == 0) {
            continue;
        }

        if (result == -1) {
            fprintf(stderr, "Error: Invalid data on line %d.\n",
                    line_number);
            goto failure;
        }

        if (!count_found) {
            if (value_count != 1 || values[0] <= 0 ||
                values[0] > capacity) {
                fprintf(stderr,
                        "Error: Invalid job count on line %d.\n",
                        line_number);
                goto failure;
            }

            expected = values[0];
            count_found = 1;
            continue;
        }

        if (value_count != 4) {
            fprintf(stderr,
                    "Error: Expected 4 values on line %d.\n",
                    line_number);
            goto failure;
        }

        if (values[0] <= 0 || values[1] < 0 ||
            values[2] <= 0 || values[3] < 0) {
            fprintf(stderr,
                    "Error: Invalid job values on line %d.\n",
                    line_number);
            goto failure;
        }

        for (int i = 0; i < loaded; i++) {
            if (jobs[i].id == values[0]) {
                fprintf(stderr,
                        "Error: Duplicate job ID %d.\n",
                        values[0]);
                goto failure;
            }
        }

        jobs[loaded].id = values[0];
        jobs[loaded].arrival = values[1];
        jobs[loaded].burst = values[2];
        jobs[loaded].priority = values[3];
        jobs[loaded].status = NEW;

        loaded++;
    }

    if (ferror(file)) {
        fprintf(stderr, "Error: Failed to read job file.\n");
        goto failure;
    }

    if (!count_found || loaded != expected) {
        fprintf(stderr,
                "Error: Expected %d jobs but loaded %d.\n",
                expected, loaded);
        goto failure;
    }

    free(line);
    fclose(file);

    *count = loaded;

    printf("Successfully loaded %d jobs.\n", loaded);

    return 0;

failure:
    free(line);
    fclose(file);
    *count = 0;
    return 1;
}
