#ifndef SSH_TUNNELD_LOGGING_H
#define SSH_TUNNELD_LOGGING_H

#include <stdio.h>

void write_log_connect(int num_connections);
void write_log(const char* message);

extern FILE* logfile;

#endif
