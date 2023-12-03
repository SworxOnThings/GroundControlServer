#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "flightlist.h"
#include "alist.h"
#include "airplane.h"

static alist list; 

pthread_mutex_t flightlist_lock = PTHREAD_MUTEX_INITIALIZER;

static void airplane_free(void *plane)
{
    airplane_destroy((airplane*)plane);
    free(plane);
}

void flightlist_init(void)
{
    alist_init(&list, airplane_free);
}

void flightlist_destroy(void)
{
    pthread_mutex_destroy(&flightlist_lock);
    alist_destroy(&list);    
}

void flightlist_addplane(airplane plane)
{
    airplane* newPlane = malloc(sizeof(airplane));
    if(newPlane == NULL)
    {
        perror("cannot add plane. Out of memory");       
        exit(1);
    }

    //copy plane into new plane
    memcpy(newPlane, &plane, sizeof(plane));

    if(pthread_mutex_lock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "cannot lock mutex in add");
        exit(1);
    }
    alist_add(&list, newPlane);
    if(pthread_mutex_unlock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "cannot unlock mutex in add");
        exit(1);
    }    
}

/*
 plane number does not correspond to the index in the list, because planes can 
 be added or deleted. That's why the loop is necessary.
*/ 
airplane* find_plane(FindCallback callback, void* context)
{

    int size = alist_size(&list);

    for(int i = 0; i < size; ++i)
    {
        void* p = alist_get(&list, i);
        airplane* plane = (airplane*)p;
        if(callback(plane, context))
        {
            return plane;
        }
    }

    return NULL;
}

void flightlist_removeplane(int plane_number)
{
    if(pthread_mutex_lock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "cannot lock mutex in remove");
        exit(1);
    }

    int size = alist_size(&list);

    for(int i = 0; i < size; ++i)
    {
        void* p = alist_get(&list, i);
        airplane* plane = (airplane*)p;
        if(plane->plane_number == plane_number)
        {
            alist_remove(&list, i);
            if(pthread_mutex_unlock(&flightlist_lock) != 0)
            {
                fprintf(stderr, "cannot unlock mutex in remove");
                exit(1);
            } 
            return;
        }
    }

    if(pthread_mutex_unlock(&flightlist_lock) != 0)
    {
        fprintf(stderr, "cannot unlock mutex in remove");
        exit(1);
    } 
}