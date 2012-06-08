/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#ifndef SSH_TUNNELD_SSHCONTROL_H
#define SSH_TUNNELD_SSHCONTROL_H

#include <sys/types.h>

pid_t start_ssh_tunnel(char* hostname, char* port, char* proxy_port);
void stop_ssh_tunnel(pid_t process_id);

#endif
