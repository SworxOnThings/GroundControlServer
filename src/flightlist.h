#ifndef FLIGHT_LIST_H
#define FLIGHT_LIST_H

#include <stdbool.h>
#include <pthread.h>

#include "airplane.h"

typedef bool (*FindCallback) (airplane*, void *);

//exporting global variable
extern pthread_mutex_t flightlist_lock;

void flightlist_init(void);

void flightlist_destroy(void);
void flightlist_addplane(airplane plane);
airplane* find_plane(FindCallback callback, void* context);
void flightlist_removeplane(int plane_number);

#endif