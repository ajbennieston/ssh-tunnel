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
