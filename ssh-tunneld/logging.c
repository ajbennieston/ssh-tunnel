/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#define _XOPEN_SOURCE 500

#include "logging.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Global variables */
FILE* logfile;

char *terminated_strncpy(char *dest, const char *src, size_t n)
{
    /* Wrapper for strncpy() that ensures correct string termination */
    size_t source_length = strlen(src);
    strncpy(dest, src, n); /* null termination of dest will be enforced */
    if (source_length >= n)
    {
        /* Source string was too big, so we terminate at the end
         * of destination buffer
         */
        dest[n-1] = '\0';
    }
    else
    {
        /* Source string fits inside destination buffer, so we
         * can terminate immediately after it
         */
        dest[source_length] = '\0';
    }
    return dest;
}

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
        char time_string[64];
        time_t t = time(NULL);
        char* str_time = ctime(&t);
        terminated_strncpy(time_string, str_time, sizeof(time_string));
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
        char time_string[64];
        time_t t = time(NULL);
        char* str_time = ctime(&t); /* obtain time as string */
        terminated_strncpy(time_string, str_time, sizeof(time_string));
        size_t stime = strlen(time_string);
        time_string[stime-1] = '\0'; /* remove '\n' from time string */
        fprintf(logfile, "[%s] %s\n", time_string, message);
        fflush(logfile);
    }
}

