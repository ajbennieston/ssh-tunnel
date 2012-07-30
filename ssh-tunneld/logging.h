/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#ifndef SSH_TUNNELD_LOGGING_H
#define SSH_TUNNELD_LOGGING_H

#include <stdio.h>

void write_log_connect(int num_connections);
void write_log(const char* message);

extern FILE* logfile;

#endif
