// The airplane module contains the airplane data type and management functions

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "airplane.h"

/************************************************************************
 * plane_init initializes an airplane structure in the initial PLANE_UNREG
 * state, with given send and receive FILE objects.
 */
void airplane_init(airplane *plane, FILE *fp_send, FILE *fp_recv) 
{
    plane->state = PLANE_UNREG;
    plane->fp_send               = fp_send;
    plane->fp_recv               = fp_recv;
    plane->id[0]                 = '\0';
    // local static means local to the function. Only initialized one time, the first  
    // time the function is called. Static is storage duration, in this context
    static int next_plane_number = 0;
    plane->plane_number          = ++next_plane_number;
    if(pthread_mutex_init(&plane->mutex, NULL) != 0)
    {
        fprintf(stderr, "Could not initialize plane mutex");
        exit(1);
    }
}

int read_state(airplane* plane)
{
    if(pthread_mutex_lock(&plane->mutex) != 0) 
    {
        fprintf(stderr, "Could not lock plane mutex in airplane");
        exit(1);
    }

    int state = plane->state;

    if(pthread_mutex_unlock(&plane->mutex) != 0) 
    {
        fprintf(stderr, "Could not unlock plane mutex in airplane");
        exit(1);
    }

    return state;
}

void set_state(airplane* plane, int state)
{
    if(pthread_mutex_lock(&plane->mutex) != 0) 
    {
        fprintf(stderr, "Could not lock plane mutex in airplane");
        exit(1);
    }   

    plane->state = state;

    if(pthread_mutex_unlock(&plane->mutex) != 0) 
    {
        fprintf(stderr, "Could not unlock plane mutex in airplane");
        exit(1);
    }
}

/************************************************************************
 * plane_destroy frees up any resources associated with an airplane, like
 * file handles, so that it can be free'ed.
 */
void airplane_destroy(airplane *plane) 
{
    fclose(plane->fp_send);
    fclose(plane->fp_recv);
    if(pthread_mutex_destroy(&plane->mutex) != 0)
    {
        fprintf(stderr, "Could not destroy plane mutex");
        exit(1);
    }
}
