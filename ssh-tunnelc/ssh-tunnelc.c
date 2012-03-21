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

void print_usage(const char* program_name);
void sig_handler(int signum);
void process_arguments(int argc, char** argv, char** proxy_host, char** proxy_port,
                       char** tun_port, char** ssh_host, char** ssh_port);

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

    /* Send a message to ssh-tunneld telling it we
     * want to open an ssh connection through the tunnel
     */
    connection_start();

    /* Register a signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handler;
    sigaddset(&(sa.sa_mask), SIGTERM);
    sigaddset(&(sa.sa_mask), SIGCHLD);
    sigaddset(&(sa.sa_mask), SIGHUP);
    sa.sa_flags = SA_NOCLDSTOP;
    if(sigaction(SIGCHLD, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
    if(sigaction(SIGTERM, &sa, NULL) != 0)
    {
        perror("sigaction");
    }
    if(sigaction(SIGHUP, &sa, NULL) != 0)
    {
        perror("sigaction");
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
        case SIGHUP:
            connection_stop();
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
    }
}

void print_usage(const char* program_name)
{
    fprintf(stderr,
            "Usage:\n %s [-h hostname] [-p port] [-t port] ssh_hostname ssh_port\n\n", program_name);
    fprintf(stderr,
            " -h hostname\n    SOCKS5 proxy and ssh-tunneld hostname.\n    Default: 127.0.0.1.\n\n");
    fprintf(stderr,
            " -p port\n    SOCKS5 proxy port.\n    Default: 1080.\n\n");
    fprintf(stderr,
            " -t port\n    ssh-tunneld control port.\n    Default: 1081.\n\n");
}

void process_arguments(int argc, char** argv, char** proxy_host, char** proxy_port,
                       char** tun_port, char** ssh_host, char** ssh_port)
{
    /*
     * Usage: progname [-h hostname] [-p port] [-t port] ssh_hostname ssh_port
     *
     * Options:
     * -h hostname
     *    sets proxy_host : hostname of both the SOCKS5 proxy *and* the ssh-tunneld process
     * -p port
     *    sets proxy_port : port for the SOCKS5 proxy
     * -t port
     *    sets tun_port : port for the ssh-tunneld process
     *
     * ssh_hostname and ssh_port are set from the remaining values of argv after option
     * processing has completed. These must always be present.
     */
    int opt;

    int set_proxy_host = 0;
    int set_proxy_port = 0;
    int set_tun_port = 0;

    while ((opt = getopt(argc, argv, "h:p:t:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                /* Set proxy host */
                if (! set_proxy_host)
                {
                    *proxy_host = strdup(optarg);
                    set_proxy_host = 1;
                }
                break;
            case 'p':
                if (! set_proxy_port)
                {
                    *proxy_port = strdup(optarg);
                    set_proxy_port = 1;
                }
                break;
            case 't':
                if (! set_tun_port)
                {
                    *tun_port = strdup(optarg);
                    set_tun_port = 1;
                }
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }
    
    if (optind + 2 > argc)
    {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    /* The remaining options should now be the ssh host and port */
    *ssh_host = strdup(argv[optind]);
    *ssh_port = strdup(argv[optind+1]);

    /* And set default values if we didn't receive them from the options */
    if (! set_proxy_host)
    {
        char* default_proxy_host = "127.0.0.1";
        *proxy_host = strdup(default_proxy_host);
    }
    if (! set_proxy_port)
    {
        char* default_proxy_port = "1080";
        *proxy_port = strdup(default_proxy_port);
    }
    if (! set_tun_port)
    {
        char* default_tun_port = "1081";
        *tun_port = strdup(default_tun_port);
    }
}
