#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "clienthandler.h"
#include "airplane.h"
#include "flightlist.h"
#include "airs_protocol.h"

typedef struct{
    struct sockaddr_in peerAddress;
    int planeNumber;
} ThreadInfo;

//callback function
bool hasPlaneNumber(airplane* plane, void* context)
{
    int planeNumber = (int)(intptr_t)context;
    return planeNumber == plane->plane_number;
}

void* pthread_start(void* arg)
{
    ThreadInfo* threadInfo = (ThreadInfo*)arg;
    int planeNumber = threadInfo->planeNumber;
    struct sockaddr_in peerAddress = threadInfo->peerAddress;
    free(threadInfo);

    char peerIpAddressBuffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peerAddress.sin_addr, peerIpAddressBuffer, 
    sizeof(peerIpAddressBuffer));

    pthread_t id = pthread_self();

    printf("Got connection from %s (client %ld)\n", peerIpAddressBuffer, id);

    if(pthread_mutex_lock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "Could not lock in pthread_start\n");
        return (void*)-1;
    }

    //using callback function here
    airplane* plane = find_plane(hasPlaneNumber, (void*)(intptr_t)planeNumber);

    if(pthread_mutex_unlock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "Could not unlock in pthread_start\n");
        return (void*)-1;
    }

    char *lineptr = NULL;
    size_t linesize = 0;

    while (read_state(plane) != PLANE_DONE) 
    {
        //getline will allocate memory for the line buffer
        if (getline(&lineptr, &linesize, plane->fp_recv) < 0) 
        {
            // Failed getline means the client disconnected
            break;
        }
        docommand(plane, lineptr);
    }
    flightlist_removeplane(plane->plane_number);

    printf("Client %ld disconnected.\n", id);
    free(lineptr);
    //success
    return (void*)0;
}

void launch_client_handler(int clientSocket, struct sockaddr_in peerAddress)
{
    int fd_send = dup(clientSocket);
    if(fd_send == -1)
    {
        perror("dup failed");
        exit(1);
    }
    int fd_recv = clientSocket;

    FILE* fsend = fdopen(fd_send, "w");
    if(fsend == NULL)
    {
        perror("fsend failed");
        close(fd_recv);
        close(fd_send);
        exit(1);
    }

    if (setvbuf(fsend, NULL, _IOLBF, 0) != 0) 
    {
        fprintf(stderr, "Couldn't enable line bufferent for ???. Sad.\n");
    }

    FILE* frecv = fdopen(fd_recv, "r");
    if(frecv == NULL)
    {
        perror("frecv failed");
        fclose(fsend);
        close(fd_recv);
        close(fd_send);
        exit(1);
    }

    if (setvbuf(frecv, NULL, _IOLBF, 0) != 0) 
    {
        fprintf(stderr, "Couldn't enable line bufferent for ???. Sad.\n");
    }

    airplane plane;
    airplane_init(&plane, fsend, frecv);

    flightlist_addplane(plane);

    ThreadInfo* threadInfo = malloc(sizeof(ThreadInfo));
    if(threadInfo == NULL)
    {
        fprintf(stderr, "Out of memory.\n");
        return;
    }
    threadInfo->peerAddress = peerAddress;
    threadInfo->planeNumber = plane.plane_number;

    pthread_t pthread;
    pthread_create(&pthread, NULL, &pthread_start, threadInfo);
    pthread_detach(pthread); 
}