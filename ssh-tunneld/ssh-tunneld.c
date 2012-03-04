#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

/* I'd like to get rid of this global variable... */
FILE* logfile;

pid_t start_ssh_tunnel(char* hostname, char* proxy_port);
void stop_ssh_tunnel(pid_t process_id);
int tunneld_main();
void write_log_connect(int num_connections);
void write_log(const char* message);

void sig_handler(int signum)
{
	switch(signum)
	{
		case SIGTERM:
			write_log("Received SIGTERM. Stopping.");
			kill(0, SIGTERM); /* Send child processes the same signal */
			exit(EXIT_SUCCESS);
		default:
			break;
	}
}

pid_t start_ssh_tunnel(char* hostname, char* proxy_port)
{
	write_log("Starting ssh process.");
	char* argv[] = {
		"ssh",
		"-T",
		"-n",
		"-N",
		"-D",
		proxy_port,
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

int tunneld_main()
{
	int socket_fd, new_fd; /* listen on socket_fd, accept new connections -> new_fd */
	struct addrinfo *result, *rp; /* Structures to hold addresses from getaddrinfo() */
	struct addrinfo hints; /* hints to getaddrinfo() */
	
	char buf[1]; /* future-proof; if we have bigger messages we can expand this here */
	unsigned int n_connected = 0; /* number of clients using the SSH tunnel */
	pid_t ssh_tunnel_process = 0; /* process ID for "ssh -D ..." */

	/* Hint that we want to bind to any interface...
	 * Would be better to bind to local interface only (by default)
	 */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	int yes = 1;

	/* Use the hints to find address(es) to bind to */
	if(getaddrinfo(NULL, "1081", &hints, &result) != 0)
	{
		write_log("Error looking up address. Exiting.");
		exit(EXIT_FAILURE);
	}

	/* Try to bind to an interface on requested port*/
	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (socket_fd == -1)
			continue; /* try next address */

		if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			write_log("Error in setsockopt. Exiting.");
			exit(EXIT_FAILURE);
		}

		if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break; /* successfully bound */

		close(socket_fd);
	}
	if (rp == NULL)
	{
		/* no address was successfully bound */
		write_log("Error binding to port 1081. Exiting.");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result); /* don't need this any more */

	/* listen on the port we just bound
	 * Keep a maximum of 10 pending connections in the "backlog"
	 */
	if (listen(socket_fd, 10) == -1)
	{
		write_log("Error listening on port 1081. Exiting.");
		exit(EXIT_FAILURE);
	}

	write_log("tunneld: Started.");
	/* now accept connections and deal with them one by one */
	while(1)
	{
		/*
		 * No need to use select()... accept() blocks until a client connects, and
		 * the processing time is typically short. In any case, if another client
		 * connects while we're in the sleep() period waiting for an ssh tunnel,
		 * we want to wait anyway, rather than creating a second tunnel!
		 */
		new_fd = accept(socket_fd, NULL, NULL); /* don't care about client address */
		if (new_fd == -1)
		{
			write_log("Error while accepting connection. Continuing.");
			continue;
		}

		/* Read a message from the client to see what it wants us to do */
		if(recv(new_fd, buf, 1, 0) != 1)
		{
			write_log("Received unexpected data. Closing connection.");
			close(new_fd);
			continue;
		}

		if (buf[0] == 'C') /* client wants to connect through tunnel */
		{
			if (n_connected == 0)
			{
				/* no tunnel exists; start it */
				ssh_tunnel_process = start_ssh_tunnel("soulor", "1080");
				sleep(20); /* give ssh time to establish a connection */
				/* a better approach might be to try connecting to 1080,
				 * sleep for a bit if it doesn't work, then retry.
				 */
			}
			n_connected += 1;
			write_log_connect(n_connected);

			/* tell the client it can proceed */
			char message = 'C';
			send(new_fd, &message, sizeof(message), 0);
		}
		else if (buf[0] == 'D') /* client telling us it is done with tunnel */
		{
			n_connected -= 1;
			write_log_connect(n_connected);
			if (n_connected <= 0)
			{
				/* nothing using the tunnel any more; stop it. */
				n_connected = 0;
				stop_ssh_tunnel(ssh_tunnel_process);
				ssh_tunnel_process = 0;
			}
			/* tell the client we acted on their message */
			char message = 'D';
			send(new_fd, &message, sizeof(message), 0);
		}
		close(new_fd); /* close client socket */
	}

	return 0;
}

void write_log_connect(int num_connections)
{
	char buffer[128];
	snprintf(buffer, sizeof(buffer)-1, "Connections: %d", num_connections);
	write_log(buffer);
}

void write_log(const char* message)
{
	/* write a log message that is prefixed with the current date & time */
	if (logfile != NULL)
	{
		char time_string[32];
		time_t t = time(NULL);
		char* str_time = ctime(&t); /* obtain time as string */
		strncpy(time_string, str_time, sizeof(time_string));
		size_t stime = strlen(time_string);
		time_string[stime-1] = '\0'; /* remove '\n' from time string */
		fprintf(logfile, "[%s] %s\n", time_string, message);
		fflush(logfile);
	}
}

int main()
{
	/* Become a daemon, then run tunneld_main() */

	/* 1. Fork */
	pid_t process_id = fork();
	if(process_id == 0)
	{
		/* in child process */
		umask(0); /* set umask to something sensible */

		/* open a logfile */
		logfile = fopen("/home/andrew/tunneld.log", "a");
		if (logfile == NULL)
		{
			perror("fopen");
			exit(EXIT_FAILURE);
		}

		/* become session leader */
		pid_t session_id = setsid();
		if (session_id < 0)
		{
			write_log("Could not create session. Exiting.");
			exit(EXIT_FAILURE);
		}

		/* change the working directory */
		if(chdir("/") < 0)
		{
			write_log("Could not change directory to /. Exiting.");
			exit(EXIT_FAILURE);
		}

		/* Close standard file descriptors */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		/* Set up signal handler */
		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_handler = sig_handler;
		sigaddset(&(sa.sa_mask), SIGTERM);
		if (sigaction(SIGTERM, &sa, NULL) != 0)
		{
			write_log("Could not set signal handler for SIGTERM. Exiting.");
			exit(EXIT_FAILURE);
		}

		/* Run tunneld_main() */
		tunneld_main();

	}
	else if (process_id > 0)
	{
		/* in parent process; exit */
		_exit(EXIT_SUCCESS);
	}
	else
	{
		/* something went wrong */
		perror("fork");
		exit(EXIT_FAILURE);
	}
	return 0;
}
