/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200112L

#include "options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *checked_strdup(const char *s)
{
    /* Wrap strdup() and check for NULL return value,
     * indicating that memory allocation failed. */
    char *duplicate = strdup(s); /* NULL check follows */
    if (duplicate == NULL)
    {
        fprintf(stderr, "Unable to allocate memory. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    return duplicate;
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

void process_options(int argc, char** argv, struct program_options* options)
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
    
    /* Set defaults */
    options->nofork = 0; 
    options->accept_remote = 0; 
    options->proxy_port = NULL;
    options->log_filename = NULL;
    options->remote_port = NULL;
    options->tunnel_port = NULL;
    options->remote_host = NULL;

    while ((opt = getopt(argc, argv, "d:fl:p:rt:")) != -1)
    {
        switch(opt)
        {
            case 'd': /* local proxy port */
                if (options->proxy_port == NULL)
                    options->proxy_port = optarg;
                break;
            case 'f': /* nofork */
                options->nofork = 1;
                break;
            case 'l': /* log filename */
                if (options->log_filename == NULL)
                    options->log_filename = optarg;
                break;
            case 'p': /* remote port */
                if (options->remote_port == NULL)
                    options->remote_port = optarg;
                break;
            case 'r':
                options->accept_remote = 1;
                break;
            case 't': /* tunneld port */
                if (options->tunnel_port == NULL)
                    options->tunnel_port = optarg;
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

    options->remote_host = argv[optind];

    /* Set default values */
    if (options->proxy_port == NULL)
    {
        char* default_proxy_port = "1080";
        options->proxy_port = checked_strdup(default_proxy_port);
    }
    if (options->remote_port == NULL)
    {
        char* default_remote_port = "22";
        options->remote_port = checked_strdup(default_remote_port);
    }
    if (options->tunnel_port == NULL)
    {
        char* default_tun_port = "1081";
        options->tunnel_port = checked_strdup(default_tun_port);
    }
}

