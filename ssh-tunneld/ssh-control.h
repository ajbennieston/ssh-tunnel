#ifndef SSH_TUNNELD_SSHCONTROL_H
#define SSH_TUNNELD_SSHCONTROL_H

#include <sys/types.h>

pid_t start_ssh_tunnel(char* hostname, char* port, char* proxy_port);
void stop_ssh_tunnel(pid_t process_id);

#endif
