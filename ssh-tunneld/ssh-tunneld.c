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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

#include "logging.h"
#include "options.h"
#include "ssh-control.h"

int tunneld_main(struct program_options* options);

void sig_handler(int signum);

int test_connection(char* proxy_port);

void daemonize(int nofork);

int main(int argc, char** argv)
{
    /* Keep the program options together */
    struct program_options options;
    memset(&options, sizeof(struct program_options), 0);

    process_options(argc, argv, &options);

    /* open a logfile */
    if (options.nofork)
    {
        logfile = stderr;
    }
    else if (options.log_filename != NULL)
    {
        logfile = fopen(options.log_filename, "a");
        if (logfile == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // No log file
        logfile = NULL;
    }

    /* Become a daemon */
    daemonize(options.nofork);

    /* Set up signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handler;
    sigaddset(&(sa.sa_mask), SIGTERM);
    if (sigaction(SIGTERM, &sa, NULL) != 0)
    {
        write_log("Could not set signal handler for SIGTERM. Exiting.");
        exit(EXIT_FAILURE);
    }

    /* Run tunneld_main() */
    tunneld_main(&options);

    return 0;
}

void sig_handler(int signum)
{
    switch(signum)
    {
        case SIGTERM:
            write_log("Received SIGTERM. Stopping.");
            kill(0, SIGTERM); /* Send child processes the same signal */
            exit(EXIT_SUCCESS);
        default:
            break;
    }
}

int tunneld_main(struct program_options* options)
{
    int socket_fd = 0; /* listen on socket_fd... */
    int new_fd = 0; /*  ... accept new connections -> new_fd */
    struct addrinfo *result = 0; /* Structure to hold addresses from getaddrinfo() */
    struct addrinfo *rp = 0; /* Pointer for our convenience */
    struct addrinfo hints; /* hints to getaddrinfo() */
    
    char buf[1]; /* future-proof; if we have bigger messages we can expand this here */
    unsigned int n_connected = 0; /* number of clients using the SSH tunnel */
    pid_t ssh_tunnel_process = 0; /* process ID for "ssh -D ..." */

    /* Hint that we want to bind to any interface...
     * Would be better to bind to local interface only (by default)
     */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (options->accept_remote)
    {
        hints.ai_flags = AI_PASSIVE;
    }
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    const int yes = 1;

    /* Use the hints to find address(es) to bind to */
    int gai_result = 0;
    if (options->accept_remote)
    {
        gai_result = getaddrinfo(NULL, options->tunnel_port, &hints, &result);
    }
    else
    {
        gai_result = getaddrinfo("127.0.0.1", options->tunnel_port, &hints, &result);
    }
    if(gai_result != 0)
    {
        write_log("Error looking up address. Exiting.");
        exit(EXIT_FAILURE);
    }

    /* Try to bind to an interface on requested port*/
    for(rp = result; rp != NULL; rp = rp->ai_next)
    {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue; /* try next address */

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            write_log("Error in setsockopt. Exiting.");
            exit(EXIT_FAILURE);
        }

        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* successfully bound */

        close(socket_fd);
    }
    if (rp == NULL)
    {
        /* no address was successfully bound */
        write_log("Error binding to port. Exiting.");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result); /* don't need this any more */

    /* listen on the port we just bound
     * Keep a maximum of 10 pending connections in the "backlog"
     */
    if (listen(socket_fd, 10) == -1)
    {
        write_log("Error listening on port. Exiting.");
        exit(EXIT_FAILURE);
    }

    write_log("tunneld: Started.");
    /* now accept connections and deal with them one by one */
    while(1)
    {
        /*
         * No need to use select()... accept() blocks until a client connects, and
         * the processing time is typically short. In any case, if another client
         * connects while we're in the sleep() period waiting for an ssh tunnel,
         * we want to wait anyway, rather than creating a second tunnel!
         */
        new_fd = accept(socket_fd, NULL, NULL); /* don't care about client address */
        if (new_fd == -1)
        {
            write_log("Error while accepting connection. Continuing.");
            continue;
        }

        /* Read a message from the client to see what it wants us to do */
        if(recv(new_fd, buf, 1, 0) != 1)
        {
            write_log("Received unexpected data. Closing connection.");
            close(new_fd);
            continue;
        }

        if (buf[0] == 'C') /* client wants to connect through tunnel */
        {
            if (n_connected == 0)
            {
                /* no tunnel exists; start it */
                ssh_tunnel_process = start_ssh_tunnel(options->remote_host, options->remote_port, options->proxy_port);
                /* Sleep for 1 second then test
                 * connection; repeat until success
                 */
                do {
                    sleep(1);
                } while ( test_connection(options->proxy_port) );
            }
            n_connected += 1;
            write_log_connect(n_connected);

            /* tell the client it can proceed */
            char message = 'C';
            send(new_fd, &message, sizeof(message), 0);
        }
        else if (buf[0] == 'D') /* client telling us it is done with tunnel */
        {
            n_connected -= 1;
            write_log_connect(n_connected);
            if (n_connected <= 0)
            {
                /* nothing using the tunnel any more; stop it. */
                n_connected = 0;
                stop_ssh_tunnel(ssh_tunnel_process);
                ssh_tunnel_process = 0;
            }
            /* tell the client we acted on their message */
            char message = 'D';
            send(new_fd, &message, sizeof(message), 0);
        }
        close(new_fd); /* close client socket */
    }

    return 0;
}

int test_connection(char* proxy_port)
{
    const char* hostname = "127.0.0.1";

    struct addrinfo hints;
    struct addrinfo *result = 0;
    struct addrinfo *rp = 0;
    int socket_fd = 0;
    int gai_return_value = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    if ((gai_return_value = getaddrinfo(hostname, proxy_port, &hints, &result)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_return_value));
        exit(EXIT_FAILURE);
    }

    /*
     * Try each address returned by getaddrinfo
     * in turn
     */
    for(rp = result; rp != NULL; rp = rp->ai_next)
    {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue;

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            /* Successfully connected */
            freeaddrinfo(result);
            close(socket_fd);
            return 0;
        }

        close(socket_fd);
    }

    /* If we got here, we didn't manage to connect successfully */
    freeaddrinfo(result); /* no longer need the address structures */
    return 1;
}

void daemonize(int nofork)
{
    pid_t process_id = 0;
    if (! nofork)
        process_id = fork();

    if (process_id > 0)
    {
        /* In parent process; exit */
        _exit(EXIT_SUCCESS);
    }
    else if (process_id < 0)
    {
        /* Something went wrong */
        perror("fork");
        exit(EXIT_FAILURE);
    }

    /* If we get here, we're in the child process,
     * or no fork was requested. Now do some other
     * things to play nicely with others.
     */

    /* Become the session leader if we forked */
    if (! nofork)
    {
        pid_t session_id = setsid();
        if (session_id < 0)
        {
            write_log("Could not create session. Exiting.");
            exit(EXIT_FAILURE);
        }
    }

    umask(077); /* set umask to sensible default */

    /* Change the working directory */
    if (chdir("/") < 0)
    {
        write_log("Could not change directory to /. Exiting.");
        exit(EXIT_FAILURE);
    }

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    if (! nofork)
        close(STDERR_FILENO);
}
