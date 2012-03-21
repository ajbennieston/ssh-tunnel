#define _XOPEN_SOURCE 500

#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

