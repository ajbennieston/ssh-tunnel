#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

void print_usage(const char* program_name);
int establish_connection(const char* hostname, const char* port);
void connection_start(const char* hostname, const char* port);
void connection_stop(const char* hostname, const char* port);
void sig_handler(int signum);
void process_arguments(int argc, char** argv, char** proxy_host, char** proxy_port,
	char** tun_port, char** ssh_host, char** ssh_port);

char* tunneld_host;
char* tunneld_port;

int main(int argc, char** argv)
{
	char* proxy_host;
	char* proxy_port;
	char* ssh_host;
	char* ssh_port;

	process_arguments(argc, argv, &proxy_host, &proxy_port, &tunneld_port, &ssh_host, &ssh_port);
	/* Set tunneld_host to point to proxy_host */
	tunneld_host = proxy_host;
	size_t phost_len = strlen(proxy_host);
	size_t pport_len = strlen(proxy_port);
	size_t phost_port_len = phost_len + pport_len + 2;
	char* proxy_host_port = malloc(phost_port_len*sizeof(char));

	/* build string "proxyhost:proxyport" */
	strncpy(proxy_host_port, proxy_host, phost_len);
	proxy_host_port[phost_len] = ':';
	strncpy(proxy_host_port+phost_len+1, proxy_port, pport_len);
	proxy_host_port[phost_len+pport_len+1] = '\0';

	/* Send a message to csc-tunneld telling it we
	 * want to open an ssh connection through the tunnel
	 */
	connection_start(tunneld_host, tunneld_port);

	/* Register a signal handler */
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sig_handler;
	sigaddset(&(sa.sa_mask), SIGTERM);
	sigaddset(&(sa.sa_mask), SIGCHLD);
	sigaddset(&(sa.sa_mask), SIGHUP);
	sa.sa_flags = SA_NOCLDSTOP;
	if(sigaction(SIGCHLD, &sa, NULL) != 0)
	{
		perror("sigaction");
	}
	if(sigaction(SIGTERM, &sa, NULL) != 0)
	{
		perror("sigaction");
	}
	if(sigaction(SIGHUP, &sa, NULL) != 0)
	{
		perror("sigaction");
	}
	
	/* fork and execute:
	 * nc -X 5 -x proxyhost:proxyport host port
	 */
	pid_t id = fork();
	if (id < 0)
	{
		/* something went wrong... */
		fprintf(stderr, "fork() failed.\n");
		exit(EXIT_FAILURE);
	}
	else if (id == 0)
	{
		/* in child process */
		int status = execlp("nc", "nc", "-X", "5", "-x", proxy_host_port, argv[1], argv[2], NULL);
		/* if the exec succeeded, we should never get here */
		if (status == -1)
		{
			fprintf(stderr, "Failed to execute nc.\n");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		/* in parent process; id is the process id
		 * of the child. Wait for it to finish.
		 */
		wait(NULL);
	}
	return EXIT_SUCCESS;
}

void print_usage(const char* program_name)
{
	fprintf(stderr, "Usage: %s host port\n", program_name);
}

int establish_connection(const char* hostname, const char* port)
{
	/*
	 * Looks up hostname:port and attempts to connect to it.
	 * Returns a socket descriptor if a successful connection
	 * was established, or -1 on error.
	 */
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int socket_fd;
	int gai_return_value;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ((gai_return_value = getaddrinfo(hostname, port, &hints, &result)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_return_value));
		exit(EXIT_FAILURE);
	}

	/*
	 * Try each address returned by getaddrinfo
	 * in turn
	 */
	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (socket_fd == -1)
			continue;

		if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Successfully connected */

		close(socket_fd);
	}

	if (rp == NULL)
	{
		/* no attempt to connect succeeded */
		fprintf(stderr, "Could not connect to %s:%s\n", hostname, port);
		socket_fd = -1;
	}

	freeaddrinfo(result); /* no longer need the address structures */
	
	return socket_fd;
}

void send_message(const char* hostname, const char* port, char message)
{
	/* Connect to csc-tunneld and deliver the message
	 * to either open or close a connection.
	 * Expect csc-tunneld to be listening on port 1081.
	 */
	int sock_fd = establish_connection(hostname, port);
	if (sock_fd == -1)
	{
		fprintf(stderr, "Could not connect to csc-tunneld running on %s:%s\n", hostname, port);
		exit(EXIT_FAILURE);
	}
	/* now send a message to the csc-tunneld */
	if (send(sock_fd, &message, sizeof(message), 0) != 1)
	{
		perror("send");
		exit(EXIT_FAILURE);
	}
	/* read the response that tells us when the tunnel is active (or that our disconnect request
	 * was acknowledge)
	 */
	char buffer[1];
	if (recv(sock_fd, &buffer, sizeof(buffer), 0) != 1)
	{
		perror("recv");
		exit(EXIT_FAILURE);
	}
	if (buffer[0] != message)
	{
		fprintf(stderr, "Received incorrect response from csc-tunneld. Exiting.\n");
		exit(EXIT_FAILURE);
	}
	/* finally close the socket */
	close(sock_fd);
}

void connection_start(const char* hostname, const char* port)
{
	send_message(hostname, port, 'C');
}

void connection_stop(const char* hostname, const char* port)
{
	send_message(hostname, port, 'D');
}

void sig_handler(int signum)
{
	switch (signum)
	{
		case SIGCHLD:
		case SIGTERM:
		case SIGHUP:
			connection_stop(tunneld_host, tunneld_port);
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
	}
}

void process_arguments(int argc, char** argv, char** proxy_host, char** proxy_port,
	char** tun_port, char** ssh_host, char** ssh_port)
{
	/*
	 * Usage: progname [-h hostname] [-p port] [-t port] ssh_hostname ssh_port
	 *
	 * Options:
	 * -h hostname
	 *    sets proxy_host : hostname of both the SOCKS5 proxy *and* the csc-tunneld process
	 * -p port
	 *    sets proxy_port : port for the SOCKS5 proxy
	 * -t port
	 *    sets tun_port : port for the csc-tunneld process
	 *
	 * ssh_hostname and ssh_port are set from the remaining values of argv after option
	 * processing has completed. These must always be present.
	 */
	int opt;

	int set_proxy_host = 0;
	int set_proxy_port = 0;
	int set_tun_port = 0;

	while((opt = getopt(argc, argv, "h:p:t:")) != -1)
	{
		switch (opt)
		{
			case 'h':
				/* Set proxy host */
				if (! set_proxy_host)
				{
					*proxy_host = strdup(optarg);
					set_proxy_host = 1;
				}
				break;
			case 'p':
				if (! set_proxy_port)
				{
					*proxy_port = strdup(optarg);
					set_proxy_port = 1;
				}
				break;
			case 't':
				if (! set_tun_port)
				{
					*tun_port = strdup(optarg);
					set_tun_port = 1;
				}
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (optind + 2 > argc)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	/* The remaining options should now be the ssh host and port */
	*ssh_host = strdup(argv[optind]);
	*ssh_port = strdup(argv[optind+1]);

	/* And set default values if we didn't receive them from the options */
	if (! set_proxy_host)
	{
		char* default_proxy_host = "127.0.0.1";
		*proxy_host = strdup(default_proxy_host);
	}
	if (! set_proxy_port)
	{
		char* default_proxy_port = "1080";
		*proxy_port = strdup(default_proxy_port);
	}
	if (! set_tun_port)
	{
		char* default_tun_port = "1081";
		*tun_port = strdup(default_tun_port);
	}
}
