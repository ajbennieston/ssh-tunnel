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

int main(int argc, char** argv)
{
    char* proxy_host;
    char* proxy_port;
    char* ssh_host;
    char* ssh_port;

    process_arguments(argc, argv, &proxy_host, &proxy_port, &tunneld_port, &ssh_host, &ssh_port);
    /* Set tunneld_host to point to proxy_host */
    tunneld_host = proxy_host;
    size_t phost_len = strlen(proxy_host);
    size_t pport_len = strlen(proxy_port);
    size_t phost_port_len = phost_len + pport_len + 2;
    char* proxy_host_port = malloc(phost_port_len*sizeof(char));

    /* build string "proxyhost:proxyport" */
    strncpy(proxy_host_port, proxy_host, phost_len);
    proxy_host_port[phost_len] = ':';
    strncpy(proxy_host_port+phost_len+1, proxy_port, pport_len);
    proxy_host_port[phost_len+pport_len+1] = '\0';

    /* Register a signal handler */
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
        int status = execlp("nc", "nc", "-X", "5", "-x", proxy_host_port, argv[1], argv[2], NULL);
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
    }
    return EXIT_SUCCESS;
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



