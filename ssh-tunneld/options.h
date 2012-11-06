/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#ifndef SSH_TUNNELD_OPTIONS_H
#define SSH_TUNNELD_OPTIONS_H

void print_usage(const char* program_name);

struct program_options {
    /* Remote details */
    char* remote_host;
    char* remote_port;
    /* Local details */
    char* proxy_port;
    char* tunnel_port;
    /* Logging */
    char* log_filename;
    /* Option switches */
    int nofork;
    int accept_remote;
};

void process_options(int argc, char** argv, struct program_options* options);

#endif
