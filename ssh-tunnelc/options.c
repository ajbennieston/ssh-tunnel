/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
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
            "Usage:\n %s [-h hostname] [-p port] [-t port] ssh_hostname ssh_port\n\n", program_name);
    fprintf(stderr,
            " -h hostname\n    SOCKS5 proxy and ssh-tunneld hostname.\n    Default: 127.0.0.1.\n\n");
    fprintf(stderr,
            " -p port\n    SOCKS5 proxy port.\n    Default: 1080.\n\n");
    fprintf(stderr,
            " -t port\n    ssh-tunneld control port.\n    Default: 1081.\n\n");
}

void process_arguments(int argc, char** argv, struct program_options* options)
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
                    options->proxy_host = optarg;
                    set_proxy_host = 1;
                }
                break;
            case 'p':
                if (! set_proxy_port)
                {
                    options->proxy_port = optarg;
                    set_proxy_port = 1;
                }
                break;
            case 't':
                if (! set_tun_port)
                {
                    options->tunnel_port = optarg;
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
    options->remote_host = argv[optind];
    options->remote_port = argv[optind+1];

    /* And set default values if we didn't receive them from the options */
    if (! set_proxy_host)
    {
        char* default_proxy_host = "127.0.0.1";
        options->proxy_host = checked_strdup(default_proxy_host);
    }
    if (! set_proxy_port)
    {
        char* default_proxy_port = "1080";
        options->proxy_port = checked_strdup(default_proxy_port);
    }
    if (! set_tun_port)
    {
        char* default_tun_port = "1081";
        options->tunnel_port = checked_strdup(default_tun_port);
    }
}
