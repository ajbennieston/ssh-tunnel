#define _XOPEN_SOURCE 500
#include "control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

/* Global variables */
char* tunneld_host;
char* tunneld_port;

/* Internal helper functions - declarations */
int establish_connection(const char* hostname, const char* port);
void send_message(const char* hostname, const char* port, char message);

/* Definitions of functions declared in the header */
void connection_start(void)
{
    send_message(tunneld_host, tunneld_port, 'C');
}

void connection_stop(void)
{
    send_message(tunneld_host, tunneld_port, 'D');
}

/* Internal helper functions - definitions */
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
    /* Connect to ssh-tunneld and deliver the message
     * to either open or close a connection.
     * Expect ssh-tunneld to be listening on port 1081.
     */
    int sock_fd = establish_connection(hostname, port);
    if (sock_fd == -1)
    {
        fprintf(stderr, "Could not connect to ssh-tunneld running on %s:%s\n", hostname, port);
        exit(EXIT_FAILURE);
    }
    /* now send a message to the ssh-tunneld */
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
        fprintf(stderr, "Received incorrect response from ssh-tunneld. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    /* finally close the socket */
    close(sock_fd);
}


