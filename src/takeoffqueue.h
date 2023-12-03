#ifndef TAKE_OFF_QUEUE
#define TAKE_OFF_QUEUE

void signal_inair_condition();
void init_takeOff();
void takeoff_thread_init();
void enqueue(const char* planeID);
int find_position(const char* planeID);
char* find_taxi_list(const char* planeID);
void takeOffDestroy();

#endif