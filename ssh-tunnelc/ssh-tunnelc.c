/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

/* Project headers */
#include "control.h"
#include "options.h"

void sig_handler(int signum);

char* build_host_port(const char* host, const char* port);
void register_signal_handlers();

int main(int argc, char** argv)
{
    struct program_options options;

    process_arguments(argc, argv, &options);
    /* Set tunneld_host to point to proxy_host */
    tunneld_host = options.proxy_host;
    tunneld_port = options.tunnel_port;
    char* proxy_host_port = build_host_port(options.proxy_host,
            options.proxy_port);
    if (proxy_host_port == NULL)
    {
        /* malloc() failed */
        fprintf(stderr, "Unable to allocate memory. Exiting.\n");
        return EXIT_FAILURE;
    }
    
    /* Deal with SIGTERM, SIGCHLD, SIGHUP and SIGINT */
    register_signal_handlers();
    
    /* Send a message to ssh-tunneld telling it we
     * want to open an ssh connection through the tunnel
     * While we do this, block SIGINT and SIGTERM so
     * the tunneld has a chance of keeping track of state.
     */
    sigset_t sigmask;
    if ((sigemptyset(&sigmask) == -1)
            || (sigaddset(&sigmask, SIGTERM) == -1)
            || (sigaddset(&sigmask, SIGINT) == -1)
            || sigprocmask(SIG_BLOCK, &sigmask, NULL))
    {
        perror("Failed to block SIGINT and SIGTERM");
    }
    connection_start();
    if (sigprocmask(SIG_UNBLOCK, &sigmask, NULL) == -1)
    {
        perror("Failed to unblock SIGINT and SIGTERM");
    }

    /* fork and execute:
     * nc -X 5 -x proxyhost:proxyport host port
     */
    pid_t id = fork();
    if (id < 0)
    {
        /* something went wrong... */
        fprintf(stderr, "fork() failed.\n");
        exit(EXIT_FAILURE);
    }
    else if (id == 0)
    {
        /* in child process */
        int status = execlp("nc", "nc", "-X", "5", "-x", proxy_host_port,
                options.remote_host, options.remote_port, (char *) NULL);
        /* if the exec succeeded, we should never get here */
        if (status == -1)
        {
            fprintf(stderr, "Failed to execute nc.\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        /* in parent process; id is the process id
         * of the child. Wait for it to finish.
         */
        wait(NULL);

        if (proxy_host_port != NULL)
            free(proxy_host_port);
    }
    return EXIT_SUCCESS;
}

char* build_host_port(const char* host, const char* port)
{
    /* Obtain sufficient memory */
    size_t host_len = strlen(host);
    size_t port_len = strlen(port);
    size_t result_len = host_len + port_len + 2; /* colon and null char */
    char* result = malloc(result_len * sizeof(char));
    if (result == NULL)
    {
        /* malloc() failed */
        return NULL;
    }

    /* Build the string "host:port" and ensure that it is null-terminated */
    strncpy(result, host, host_len);
    result[host_len] = ':';
    strncpy(result + host_len + 1, port, port_len);
    result[host_len + port_len + 1] = '\0';
    return result;
}

void register_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handler;
    sigaddset(&(sa.sa_mask), SIGTERM);
    sigaddset(&(sa.sa_mask), SIGCHLD);
    sigaddset(&(sa.sa_mask), SIGHUP);
    sigaddset(&(sa.sa_mask), SIGINT);
    sa.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
    if (sigaction(SIGTERM, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
    if (sigaction(SIGHUP, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
}

void sig_handler(int signum)
{
    switch (signum)
    {
        case SIGCHLD:
        case SIGTERM:
        case SIGINT:
        case SIGHUP:
            connection_stop();
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
    }
}


