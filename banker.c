#include "banker.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_PROCESSES 100
#define MAX_RESOURCES 10

/*
 * Read a non-negative integer safely.
 */
static int read_number(FILE *file, int *value)
{
    long long number;

    if (fscanf(file, "%lld", &number) != 1 ||
        number < 0 || number > INT_MAX) {
        return 0;
    }

    *value = (int)number;
    return 1;
}

/*
 * Check whether the input contains only whitespace
 * after all expected values have been read.
 */
static int check_end_of_file(FILE *file)
{
    int ch;

    while ((ch = fgetc(file)) != EOF) {
        if (ch != ' ' && ch != '\t' &&
            ch != '\n' && ch != '\r') {
            return 0;
        }
    }

    return !ferror(file);
}

/*
 * Banker's safety algorithm.
 */
static int check_safety(
    int processes,
    int resources,
    int available[],
    int allocation[][MAX_RESOURCES],
    int maximum[][MAX_RESOURCES])
{
    int need[MAX_PROCESSES][MAX_RESOURCES];
    int work[MAX_RESOURCES];
    int finished[MAX_PROCESSES] = {0};
    int safe_sequence[MAX_PROCESSES];

    int completed = 0;

    /* Calculate Need = Maximum - Allocation. */
    for (int i = 0; i < processes; i++) {
        for (int j = 0; j < resources; j++) {
            if (maximum[i][j] < allocation[i][j]) {
                fprintf(stderr,
                        "Error: Maximum is less than allocation "
                        "for P%d, resource %d.\n", i, j);
                return -1;
            }

            need[i][j] =
                maximum[i][j] - allocation[i][j];
        }
    }

    /* Copy Available into Work. */
    for (int j = 0; j < resources; j++) {
        work[j] = available[j];
    }

    /*
     * Find processes whose remaining needs
     * can be satisfied by the available resources.
     */
    while (completed < processes) {
        int found = 0;

        for (int i = 0; i < processes; i++) {
            if (finished[i]) {
                continue;
            }

            int can_execute = 1;

            for (int j = 0; j < resources; j++) {
                if (need[i][j] > work[j]) {
                    can_execute = 0;
                    break;
                }
            }

            if (can_execute) {
                /*
                 * Process finishes and releases
                 * its allocated resources.
                 */
                for (int j = 0; j < resources; j++) {
                    work[j] += allocation[i][j];
                }

                finished[i] = 1;
                safe_sequence[completed] = i;
                completed++;
                found = 1;
            }
        }

        if (!found) {
            break;
        }
    }

    if (completed == processes) {
        printf("\nSystem is in a SAFE state.\n");
        printf("Safe Sequence: ");

        for (int i = 0; i < processes; i++) {
            printf("P%d", safe_sequence[i]);

            if (i < processes - 1) {
                printf(" -> ");
            }
        }

        printf("\n");
        return 1;
    }

    printf("\nSystem is in an UNSAFE state.\n");
    printf("No safe sequence exists for the given state.\n");

    return 0;
}

/*
 * Read the resource allocation data from a file
 * and execute Banker's algorithm.
 */
int run_banker_file(const char *path)
{
    FILE *file;

    int processes;
    int resources;

    int available[MAX_RESOURCES];
    int allocation[MAX_PROCESSES][MAX_RESOURCES] = {{0}};
    int maximum[MAX_PROCESSES][MAX_RESOURCES] = {{0}};

    if (path == NULL) {
        fprintf(stderr, "Error: Invalid file path.\n");
        return 1;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        perror("Error opening Banker input file");
        return 1;
    }

    /* Read process and resource counts. */
    if (!read_number(file, &processes) ||
        !read_number(file, &resources) ||
        processes <= 0 || processes > MAX_PROCESSES ||
        resources <= 0 || resources > MAX_RESOURCES) {

        fprintf(stderr, "Error: Invalid process/resource count.\n");
        fclose(file);
        return 1;
    }

    /* Read Available vector. */
    for (int j = 0; j < resources; j++) {
        if (!read_number(file, &available[j])) {
            fprintf(stderr, "Error reading Available vector.\n");
            fclose(file);
            return 1;
        }
    }

    /* Read Allocation matrix. */
    for (int i = 0; i < processes; i++) {
        for (int j = 0; j < resources; j++) {
            if (!read_number(file, &allocation[i][j])) {
                fprintf(stderr, "Error reading Allocation matrix.\n");
                fclose(file);
                return 1;
            }
        }
    }

    /* Read Maximum matrix. */
    for (int i = 0; i < processes; i++) {
        for (int j = 0; j < resources; j++) {
            if (!read_number(file, &maximum[i][j])) {
                fprintf(stderr, "Error reading Maximum matrix.\n");
                fclose(file);
                return 1;
            }
        }
    }

    if (!check_end_of_file(file)) {
        fprintf(stderr, "Error: Unexpected extra data in file.\n");
        fclose(file);
        return 1;
    }

    fclose(file);

    printf("\n========== BANKER'S ALGORITHM ==========\n");
    printf("Processes: %d\n", processes);
    printf("Resources: %d\n", resources);

    printf("\nAvailable: ");
    for (int j = 0; j < resources; j++) {
        printf("%d ", available[j]);
    }

    printf("\n");

    int result = check_safety(
        processes,
        resources,
        available,
        allocation,
        maximum
    );

    return result == -1 ? 1 : 0;
}
