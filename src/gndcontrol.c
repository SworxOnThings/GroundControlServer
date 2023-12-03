// This is the main program for the air traffic ground control server.

// The job of this module is to set the system up and then turn over control
// to the airs_protocol module which will handle the actual communication
// protocol between clients (airplanes) and the server.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "airplane.h"
#include "airs_protocol.h"
#include "clienthandler.h"
#include "flightlist.h"
#include "takeoffqueue.h"

int create_listener(char *port) {
    int sock_fd;
    if ((sock_fd=socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("socket");
        return -1;
    }

    // Avoid time delay in reusing port - important for debugging, but
    // probably not used in a production server.

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // First, use getaddrinfo() to fill in address struct for later bind

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    struct addrinfo *result;
    int rval;
    if ((rval=getaddrinfo(NULL, port, &hints, &result)) != 0) 
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rval));
        close(sock_fd);
        return -1;
    }

    // Assign a name/addr to the socket - just blindly grabs first result
    // off linked list, but really should be exactly one struct returned.

    int bret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    result = NULL;  // Not really necessary, but ensures no use-after-free

    if (bret < 0) 
    {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    // Finally, set up listener connection queue
    int lret = listen(sock_fd, 128);
    if (lret < 0) 
    {
        perror("listen");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

/************************************************************************
 * Part 1 main: Only 1 airplane, doing I/O via stdin and stdout.
 */
int main(int argc, char *argv[]) 
{
    int listener = create_listener(PORT);
    if(listener == -1)
    {
        return 1;
    }

    flightlist_init();
    init_takeOff();
    takeoff_thread_init();
    int clientSocket;

    struct sockaddr_in peerAddress;
    socklen_t peerAddressLength = (socklen_t)sizeof(peerAddress);
    // queue is created when you call listen(). THat is done in create_listener
    while((clientSocket = accept(listener, (struct sockaddr*)&peerAddress, 
            &peerAddressLength)) != -1)
    {
        launch_client_handler(clientSocket, peerAddress);
    }

    flightlist_destroy();
    takeOffDestroy();
    shutdown(listener, SHUT_RD);
    close(listener);
    return 0;
}
