#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "alist.h"
#include "flightlist.h"
#include "airs_protocol.h"
#include "debug.h"

//static files are not included in the header
static alist takeOff_queue;

static pthread_t pthread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t thread_condition = PTHREAD_COND_INITIALIZER;

//forward declaration for use in pthread_start
static void dequeue();

void signal_inair_condition()
{
    pthread_cond_signal(&thread_condition);
}

static void* pthread_start(void* arg)
{
    while(1)
    {   
        DEBUG_PRINT("%s", "ATTEMPTING TO ACQUIRE TAKEOFF MUTEX");
        if(pthread_mutex_lock(&mutex) != 0)
        {
            fprintf(stderr, "Mutex could not lock");
            exit(1);     
        }    
        DEBUG_PRINT("%s", "ACQUIRED TAKEOFF MUTEX");
        DEBUG_PRINT("%s", "waiting for planes to enter the queue");

        while(alist_size(&takeOff_queue) == 0)
        {
            pthread_cond_wait(&thread_condition, &mutex);
        }

        DEBUG_PRINT("%s", "at least one plane is in the queue");


        char *cleared_plane = strdup(alist_get(&takeOff_queue, 0));

        if(pthread_mutex_unlock(&mutex) != 0)
        {
            fprintf(stderr, "Mutex unlock failed. You have done something incorrect.");
            exit(1);
        }

        DEBUG_PRINT("%s", "ATTEMPTING TO ACQUIRE FLIGHTLIST LOCK");
        
        if(pthread_mutex_lock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not lock flightlist mutex in take off queue");
            exit(1);
        }

        DEBUG_PRINT("%s", "ACQUIRED FLIGHTLIST LOCK");
        airplane* plane = find_plane(&hasPlaneID, cleared_plane);
        if(plane == NULL)
        {
            printf("Plane %s has either disconnected or does not exist", cleared_plane);
            free(cleared_plane);
        }
        else
        {
            //free before waiting
            free(cleared_plane);
            set_state(plane, PLANE_CLEAR);
            fprintf(plane->fp_send, "TAKEOFF\n");
            printf("Plane %s has been cleared for take off\n", plane->id);
            DEBUG_PRINT("Waiting for plane %s to go INAIR", plane->id);
            while(read_state(plane) != PLANE_INAIR)
            {
                if(pthread_cond_wait(&thread_condition, &flightlist_lock) != 0)
                {
                    fprintf(stderr, "Thread condition wait failed in pthread_start");
                    exit(1);
                }
            }
            printf("Plane %s is now in air\n", plane->id);

            if(pthread_mutex_lock(&mutex) != 0)
            {
                fprintf(stderr, "Could not lock flightlist mutex in take off queue");
                exit(1);
            }
            
            dequeue();

            if(pthread_mutex_unlock(&mutex) != 0)
            {
                fprintf(stderr, "Could not unlock flightlist mutex in take off queue");
                exit(1);
            }


            set_state(plane, PLANE_DONE);
            printf("Plane %s is done\n", plane->id);
        }

        if(pthread_mutex_unlock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not unlock flightlist mutex in take off queue");
            exit(1);
        }
        DEBUG_PRINT("%s", "RELINQUISHING FLIGHTLIST LOCK");

        DEBUG_PRINT("%s", "Going to sleep.");
        sleep(4);
        DEBUG_PRINT("%s", "Waking up.");
    }
}

void takeoff_thread_init()
{
    if(pthread_create(&pthread, NULL, pthread_start, NULL) != 0)
    {
        fprintf(stderr, "Failed to create take off thread");
        exit(1);
    }

    pthread_detach(pthread);

}

static void takeOffQueueCleaner(void* flightIdString)
{
    free(flightIdString);
}

void init_takeOff()
{
    alist_init(&takeOff_queue, &takeOffQueueCleaner); 
}

void enqueue(const char* planeID)
{
    char* duplicate = strdup(planeID);
    if(duplicate == NULL)
    {
        fprintf(stderr, "Take off queue->enqueue: Out of memory.");
        exit(1);
    }

    if(pthread_mutex_lock(&mutex) != 0)
    {
        fprintf(stderr, "Mutex could not lock");
        exit(1);     
    }

    alist_add(&takeOff_queue, duplicate);
    printf("Enqueued plane: %s\n", duplicate);

    if(pthread_mutex_unlock(&mutex) != 0)
    {
        fprintf(stderr, "Mutex could not unlock");
        exit(1);     
    }

    pthread_cond_signal(&thread_condition);
}

int find_position(const char* planeID)
{
    if(pthread_mutex_lock(&mutex) != 0)
    {
        fprintf(stderr, "Could not lock mutex in find position");
        exit(1);
    }

    int size = alist_size(&takeOff_queue);
    for(int i = 0; i < size; ++i)
    {
        const char* element = alist_get(&takeOff_queue, i);
        if(strcmp(element, planeID) == 0)
        {
            if(pthread_mutex_unlock(&mutex) != 0)
            {
                fprintf(stderr, "Could not unlock mutex in find position");
                exit(1);
            }  
            DEBUG_PRINT("Found %s at %d", element, i);
            return i;
        }
    }

    if(pthread_mutex_unlock(&mutex) != 0)
    {
        fprintf(stderr, "Could not unlock mutex in find position");
        exit(1);
    } 

    return -1; // Not found   
}

char* find_taxi_list(const char* planeID)
{
    char*  buffer = NULL;
    size_t size;

    FILE* f = open_memstream(&buffer, &size);
    if(f == NULL)
    {
        perror("Could not open memory stream in find taxi list");
        return NULL;
    }

    if(pthread_mutex_lock(&mutex) != 0)
    {
        fprintf(stderr, "Could not lock mutex in find position");
        exit(1);
    }

    int queue_size = alist_size(&takeOff_queue);
    for(int i = 0; i < queue_size; ++i)
    {
        const char* element = alist_get(&takeOff_queue, i);
        if(strcmp(element, planeID) == 0)
        {
            if(pthread_mutex_unlock(&mutex) != 0)
            {
                fprintf(stderr, "Could not unlock mutex in find position");
                exit(1);
            }   

            fclose(f);
            return buffer;
        }

        if(i != 0)
        {
            fprintf(f, ", %s", element);
        }
        else
        {
            fprintf(f, "%s", element);
        }
    }

    if(pthread_mutex_unlock(&mutex) != 0)
    {
        fprintf(stderr, "Could not unlock mutex in find position");
        exit(1);
    } 

    fclose(f);
    free(buffer);
    return NULL;
}

static void dequeue()
{
    alist_remove(&takeOff_queue, 0);
}

void takeOffDestroy()
{
    alist_destroy(&takeOff_queue);
    if(pthread_cond_destroy(&thread_condition) != 0)
    {
        fprintf(stderr, "Could not destroy condition variable in take off queue");
        exit(1);
    }

    if(pthread_mutex_destroy(&mutex) != 0)
    {
        fprintf(stderr, "Could not destroy mutex in Take off queue");
        exit(1);
    }
}