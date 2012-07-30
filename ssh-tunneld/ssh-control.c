/*
 * This file is part of ssh-tunnel.
 * See the LICENSE file in the top-level directory
 * of the source distribution for further details.
 */

#define _XOPEN_SOURCE 500

#include "ssh-control.h"
#include "logging.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

pid_t start_ssh_tunnel(char* hostname, char* port, char* proxy_port)
{
    write_log("Starting ssh process.");
    char* argv[] = {
        "ssh",
        "-T",
        "-n",
        "-N",
        "-D",
        proxy_port,
        "-p",
        port,
        hostname,
        NULL
    };
    
    int process_id = fork();
    if (process_id < 0)
    {
        write_log("Error while trying to fork ssh process. Exiting.");
        exit(EXIT_FAILURE);
    }
    if (process_id == 0)
    {
        /* In child process, execute ssh */
        if(execvp("ssh", argv) == -1)
        {
            write_log("Error while trying to exec ssh process. Exiting.");
            exit(EXIT_FAILURE);
        }
    }
    /* Only get here if we're in the parent process */
    return process_id;
}

void stop_ssh_tunnel(pid_t process_id)
{
    write_log("Stopping ssh process.");
    if(kill(process_id, SIGTERM) != 0)
    {
        perror("kill");
        exit(EXIT_FAILURE);
    }
    wait(NULL);
}

