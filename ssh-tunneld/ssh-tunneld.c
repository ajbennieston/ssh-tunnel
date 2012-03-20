#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

/* I almost put the following code in:
 * 
 * FILE* logfile(FILE* init_file)
 * {
 *  static FILE* the_logfile;
 *  if (init_file)
 *  {
 *      the_logfile = init_file;
 *  }
 *  return the_logfile;
 * }
 *
 * but that's pretty ugly just to avoid a
 * single global variable... so here it is:
 */
FILE* logfile;

pid_t start_ssh_tunnel(char* hostname, char* port, char* proxy_port);
void stop_ssh_tunnel(pid_t process_id);
int tunneld_main(char* ssh_hostname, char* ssh_port,
                 char* proxy_port, char* tunneld_port,
                 int accept_remote);
void write_log_connect(int num_connections);
void write_log(const char* message);
void print_usage(const char* program_name);
void process_options(int argc, char** argv, int* nofork, char** log_filename,
                     char** remote_host, char** remote_port,
                     char** proxy_port, char** tun_port,
                     int* accept_remote);

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

pid_t start_ssh_tunnel(char* hostname, char* port, char* proxy_port)
{
    write_log("Starting ssh process.");
    char* argv[] = {
        "ssh",
        "-T",
        "-n",
        "-N",
        "-D",
        proxy_port,
        "-p",
        port,
        hostname,
        NULL
    };
    
    int process_id = fork();
    if (process_id < 0)
    {
        write_log("Error while trying to fork ssh process. Exiting.");
        exit(EXIT_FAILURE);
    }
    if (process_id == 0)
    {
        /* In child process, execute ssh */
        if(execvp("ssh", argv) == -1)
        {
            write_log("Error while trying to exec ssh process. Exiting.");
            exit(EXIT_FAILURE);
        }
    }
    /* Only get here if we're in the parent process */
    return process_id;
}

void stop_ssh_tunnel(pid_t process_id)
{
    write_log("Stopping ssh process.");
    if(kill(process_id, SIGTERM) != 0)
    {
        perror("kill");
        exit(EXIT_FAILURE);
    }
    wait(NULL);
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
                sleep(20); /* give ssh time to establish a connection */
                /* a better approach might be to try connecting to proxy_port,
                 * sleep for a bit if it doesn't work, then retry.
                 */
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

void print_usage(const char* program_name)
{
    fprintf(stderr,
            "Usage:\n %s [-d port] [-f] [-l file] [-p port] [-r] [-t port] hostname\n\n",
            program_name);
    fprintf(stderr,
            " -d port\n    Local port for SSH SOCKS5 proxy.\n    Default: 1080.\n\n");
    fprintf(stderr,
            " -f\n    Don't fork. Remain attached to terminal and log to stderr.\n\n");
    fprintf(stderr,
            " -l file\n    Append log messages to file.\n\n");
    fprintf(stderr, 
        " -p port\n    Remote port for SSH connection.\n    Default: 22.\n\n");
    fprintf(stderr,
        " -r\n    Accept remote connections on control port.\n    Default: Accept only local connections.\n\n");
    fprintf(stderr,
            " -t port\n    Local port to listen on for control connections.\n    Default: 1081.\n\n");
}

void process_options(int argc, char** argv, int* nofork, char** log_filename,
                     char** remote_host, char** remote_port,
                     char** proxy_port, char** tun_port,
                     int* accept_remote)
{
    /*
     * Usage:
     *   progname [-f] [-d port] [-l logfile] [-p port] [-r] [-t port] hostname
     * 
     * Options:
     * -d port
     *  Local port to use for SOCKS5 proxy (ssh -D port)
     * -f
     *  Don't fork; stays attached to terminal and logs to stderr
     * -l logfile
     *  Append log messages to the filename specified.
     *  Ignored if -f was given.
     * -p port
     *  Remote port for ssh -D
     * -r
     *  Accept remote connections on the control port
     *  Default: Accept only local connections
     * -t port
     *  Local port to use for control connections
     *
     * hostname must be specified. Default options as follows:
     *  proxy port : 1080
     *  logfile : none
     *  tunneld port : 1081
     *  remote port : 22
     *
     * The default behaviour is to fork and detach from the
     * controlling terminal. Only the first occurrence of an
     * option argument will be used.
     */
    int opt;
    
    *nofork = 0; /* set default */
    *accept_remote = 0; /* set default */
    *proxy_port = NULL;
    *log_filename = NULL;
    *remote_port = NULL;
    *tun_port = NULL;
    *remote_host = NULL;

    while ((opt = getopt(argc, argv, "d:fl:p:rt:")) != -1)
    {
        switch(opt)
        {
            case 'd': /* local proxy port */
                if (*proxy_port == NULL)
                    *proxy_port = strdup(optarg);
                break;
            case 'f': /* nofork */
                *nofork = 1;
                break;
            case 'l': /* log filename */
                if (*log_filename == NULL)
                    *log_filename = strdup(optarg);
                break;
            case 'p': /* remote port */
                if (*remote_port == NULL)
                    *remote_port = strdup(optarg);
                break;
            case 'r':
                *accept_remote = 1;
                break;
            case 't': /* tunneld port */
                if (*tun_port == NULL)
                    *tun_port = strdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 > argc)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    *remote_host = strdup(argv[optind]);

    /* Set default values */
    if (*proxy_port == NULL)
    {
        char* default_proxy_port = "1080";
        *proxy_port = strdup(default_proxy_port);
    }
    if (*remote_port == NULL)
    {
        char* default_remote_port = "22";
        *remote_port = strdup(default_remote_port);
    }
    if (*tun_port == NULL)
    {
        char* default_tun_port = "1081";
        *tun_port = strdup(default_tun_port);
    }
}

