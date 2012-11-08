/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#ifndef SSH_TUNNELC_OPTIONS_H
#define SSH_TUNNELC_OPTIONS_H

struct program_options {
    /* Address of the proxy to connect to */
    char* proxy_host;
    char* proxy_port;
    /* Port used by ssh-tunneld */
    char* tunnel_port;
    /* Remote tunnelled endpoint */
    char* remote_host;
    char* remote_port;
};

void print_usage(const char* program_name);
void process_arguments(int argc, char** argv, struct program_options* options);

#endif
