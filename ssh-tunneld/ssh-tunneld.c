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

int tunneld_main(char* ssh_hostname, char* ssh_port,
                 char* proxy_port, char* tunneld_port,
                 int accept_remote);

void sig_handler(int signum);

int test_connection(char* proxy_port);

int main(int argc, char** argv)
{
    char* remote_hostname;
    char* remote_port;
    char* tunneld_port;
    char* proxy_port;
    char* log_filename;
    int nofork;
    int accept_remote;
    process_options(argc, argv, &nofork, &log_filename, &remote_hostname, &remote_port, &proxy_port, &tunneld_port, &accept_remote);

    /* Become a daemon, then run tunneld_main() */

    /* 1. Fork */
    pid_t process_id = 0;
    if (! nofork)
        process_id = fork();
    if(process_id == 0)
    {
        /* in child process */
        umask(0); /* set umask to something sensible */

        /* open a logfile */
        if (nofork)
        {
            logfile = stderr;
        }
        else if (log_filename != NULL)
        {
            logfile = fopen(log_filename, "a");
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

        /* become session leader */
        if (! nofork)
        {
            pid_t session_id = setsid();
            if (session_id < 0)
            {
                write_log("Could not create session. Exiting.");
                exit(EXIT_FAILURE);
            }
        }   

        /* change the working directory */
        if(chdir("/") < 0)
        {
            write_log("Could not change directory to /. Exiting.");
            exit(EXIT_FAILURE);
        }

        /* Close standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        if (! nofork)
            close(STDERR_FILENO);

        /* Set up signal handler */
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
        tunneld_main(remote_hostname, remote_port, proxy_port, tunneld_port, accept_remote);

    }
    else if (process_id > 0)
    {
        /* in parent process; exit */
        _exit(EXIT_SUCCESS);
    }
    else
    {
        /* something went wrong */
        perror("fork");
        exit(EXIT_FAILURE);
    }
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

int tunneld_main(char* ssh_hostname, char* ssh_port,
                 char* proxy_port, char* tunneld_port,
                 int accept_remote)
{
    int socket_fd, new_fd; /* listen on socket_fd, accept new connections -> new_fd */
    struct addrinfo *result, *rp; /* Structures to hold addresses from getaddrinfo() */
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
    if (accept_remote)
    {
        hints.ai_flags = AI_PASSIVE;
    }
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    int yes = 1;

    /* Use the hints to find address(es) to bind to */
    int gai_result = 0;
    if (accept_remote)
    {
        gai_result = getaddrinfo(NULL, tunneld_port, &hints, &result);
    }
    else
    {
        gai_result = getaddrinfo("127.0.0.1", tunneld_port, &hints, &result);
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
                ssh_tunnel_process = start_ssh_tunnel(ssh_hostname, ssh_port, proxy_port);
                /* Sleep for 1 second then test
                 * connection; repeat until success
                 */
                do {
                    sleep(1);
                } while ( test_connection(proxy_port) );
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
    struct addrinfo *result, *rp;
    int socket_fd;
    int gai_return_value;

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
            break; /* Successfully connected */

        close(socket_fd);
    }

    freeaddrinfo(result); /* no longer need the address structures */

    if (rp == NULL)
    {
        /* no attempt to connect succeeded */
        /* return 1 */
        return 1;
    }
    else
    {
        /* We successfully connected */
        /* Disconnect and return 0 */
        close(socket_fd);
        return 0;
    }
}
