#ifndef SSH_TUNNELC_CONTROL_H
#define SSH_TUNNELC_CONTROL_H

void connection_start(void);
void connection_stop(void);

extern char* tunneld_host;
extern char* tunneld_port;

#endif
