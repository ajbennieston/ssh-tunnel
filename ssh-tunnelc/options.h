#ifndef SSH_TUNNELC_OPTIONS_H
#define SSH_TUNNELC_OPTIONS_H

void print_usage(const char* program_name);
void process_arguments(int argc, char** argv, char** proxy_host, char** proxy_port,
                       char** tun_port, char** ssh_host, char** ssh_port);

#endif
