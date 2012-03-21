#define _XOPEN_SOURCE 500

#include "logging.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Global variables */
FILE* logfile;

/* Function definitions */
void write_log_connect(int num_connections)
{
    /* Duplicate some of the logging code to avoid
     * using snprintf(), which triggers a warning
     * on FreeBSD 9 when compiling with
     * -pedantic -std=c99 -Wall -Wextra
     */
    if (logfile != NULL)
    {
        char time_string[32];
        time_t t = time(NULL);
        char* str_time = ctime(&t);
        strncpy(time_string, str_time, sizeof(time_string));
        size_t stime = strlen(time_string);
        time_string[stime-1] = '\0'; /* remove '\n' from time string */
        fprintf(logfile, "[%s] Connections: %d.\n", time_string, num_connections);
        fflush(logfile);
    }
}

void write_log(const char* message)
{
    /* write a log message that is prefixed with the current date & time */
    if (logfile != NULL)
    {
        char time_string[32];
        time_t t = time(NULL);
        char* str_time = ctime(&t); /* obtain time as string */
        strncpy(time_string, str_time, sizeof(time_string));
        size_t stime = strlen(time_string);
        time_string[stime-1] = '\0'; /* remove '\n' from time string */
        fprintf(logfile, "[%s] %s\n", time_string, message);
        fflush(logfile);
    }
}

