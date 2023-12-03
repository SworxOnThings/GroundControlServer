// Module to implement the ground controller application-layer protocol.

// The protocol is fully defined in the README file. This module
// includes functions to parse and perform commands sent by an
// airplane (the docommand function), and has functions to send
// responses to ensure proper and consistent formatting of these
// messages.

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "airplane.h"
#include "airs_protocol.h"
#include "flightlist.h"
#include "takeoffqueue.h"
#include "debug.h"
/************************************************************************
 * Call this response function if a command was accepted
 */
void send_ok(airplane *plane) {
    fprintf(plane->fp_send, "OK\n");
}

/************************************************************************
 * Call this response function if an error can be described by a simple
 * string.
 */
void send_err(airplane *plane, char *desc) {
    fprintf(plane->fp_send, "ERR %s\n", desc);
}

/************************************************************************
 * Call this response function if you want to embed a specific string
 * argument (sarg) into an error reply (which is now a format string).
 */
void send_err_sarg(airplane *plane, char *fmtstring, char *sarg) {
    fprintf(plane->fp_send, "ERR ");
    fprintf(plane->fp_send, fmtstring, sarg);
    fprintf(plane->fp_send, "\n");
}

static bool is_alphanumeric(char* rest)
{
    while (*rest != '\0') {
        if (!isalnum(*rest)) {
            return false; 
        }
        rest++;
    }
    return true;
}

bool hasPlaneID(airplane* plane, void* context)
{
    // don't need a cast in C, because C implicitly gains the type information, 
    // regardless if the type info is correct, because C is unsafe like that
    char* plane_id = context;
    return strcmp(plane->id, plane_id) == 0;
}

static bool isInUse(char* plane_id)
{
    airplane *plane = find_plane(hasPlaneID, (void*)plane_id);
    return plane != NULL;
}

/************************************************************************
 * Handle the "REG" command.
 */
static void cmd_reg(airplane *plane, char *rest) {
    if(rest == NULL)
    {
        send_err(plane, "Please enter a valid registration ID\n");
        return;
    }

    if(read_state(plane) != PLANE_UNREG)
    {
        char copy[PLANE_MAXID+1];
        if(pthread_mutex_lock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not lock mutex in gndcontrol-reg\n");
            return;
        }

        memcpy(copy, plane->id, PLANE_MAXID+1);

        if(pthread_mutex_unlock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not unlock mutex in gndcontrol-reg\n");
            return;
        }

        send_err_sarg(plane, "Plane already registered as %s", copy);
        return;
    }
    
    size_t length = strlen(rest);
    if(length == 0)
    {
        send_err(plane, "REG missing flight id");
        return;
    } else if(length > PLANE_MAXID)
    {
        send_err(plane, "Invalid flight id -- too long");
        return;
    }

    if(is_alphanumeric(rest))
    {
        if(pthread_mutex_lock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not lock mutex in gndcontrol-reg\n");
            return;
        }
        
        if(isInUse(rest))
        {
            send_err(plane, "ID already in use.\n");
            if(pthread_mutex_unlock(&flightlist_lock) != 0)
            {
                fprintf(stderr, "Could not unlock mutex in gndcontrol-reg\n");
                return;
            }
            return;            
        }

        strcpy(plane->id, rest);

        if(pthread_mutex_unlock(&flightlist_lock) != 0)
        {
            fprintf(stderr, "Could not unlock mutex in gndcontrol-reg\n");
            return;
        }

        send_ok(plane);
        set_state(plane, PLANE_ATTERMINAL);
    }
    else
    {
        send_err(plane, "Invalid flight id -- only alphanumeric characters allowec");
    }
}

/************************************************************************
 * Handle the "REQTAXI" command.
 */
static void cmd_reqtaxi(airplane *plane) 
{
    if(read_state(plane) == PLANE_ATTERMINAL)
    {
        enqueue(plane->id);
        set_state(plane, PLANE_TAXIING);
        send_ok(plane);
    }
    else
    {
        send_err(plane, "Plane is not at the terminal");
    }
}

/************************************************************************
 * Handle the "REQPOS" command.
 */
static void cmd_reqpos(airplane *plane)
{
    if(plane->state == PLANE_TAXIING)
    {
        int index = find_position(plane->id);
        //assert(index != -1); //in case of bug
        if(index == -1)
        {
            DEBUG_PRINT("plane id doesn't exist: %s, plane number: %d, plane state: %d", 
            plane->id, plane->plane_number, plane->state);
        }
        fprintf(plane->fp_send, "OK %d\n", index+1);       
    }
    else
    {
        send_err(plane, "REQPOS can only be issued when plane is taxiing");
    }
}

/************************************************************************
 * Handle the "REQAHEAD" command.
 */
static void cmd_reqahead(airplane *plane)
{
    if(plane->state == PLANE_TAXIING)
    {
        char* taxi_list = find_taxi_list(plane->id);
        //assert(taxi_list != NULL);
        fprintf(plane->fp_send, "OK %s\n", taxi_list == NULL ? "" : taxi_list);
        free(taxi_list);
    }
    else
    {
        send_err(plane, "REQAHEAD can only be issued when plane is taxiing");
    }

}

/************************************************************************
 * Handle the "INAIR" command.
 */
static void cmd_inair(airplane *plane)
{
    if(read_state(plane) == PLANE_CLEAR)
    {
        set_state(plane, PLANE_INAIR);
        signal_inair_condition();
        printf("Plane %s is in air\n", plane->id);
        fprintf(plane->fp_send, "NOTICE: Disconnecting from ground control -" 
        "please connect to air control\n");
    }
    else
    {
        send_err(plane, "INAIR can only be issued when clear");
    }
}

/************************************************************************
 * Handle the "BYE" command.
 */
static void cmd_bye(airplane *plane) {
    set_state(plane, PLANE_DONE);
}

static bool is_registered(airplane *plane)
{
    return read_state(plane) != PLANE_UNREG;
}

/************************************************************************
 * Parses and performs the actions in the line of text (command and
 * optionally arguments) passed in as "command".
 */
void docommand(airplane *plane, char *command) {
    char *saveptr;
    char *cmd = strtok_r(command, " \t\r\n", &saveptr);
    if (cmd == NULL) 
    {  // Empty line (no command) -- just ignore line
        return;
    }

    // Get arguments (everything after command, trimmed)
    char *args = strtok_r(NULL, "\r\n", &saveptr);
    if (args != NULL) 
    {
        args = trim(args);
    }

    // TODO: Only some commands are recognized below. Must include all
    if (strcmp(cmd, "REG") == 0) 
    {
        cmd_reg(plane, args);
    } 
    else if (strcmp(cmd, "REQTAXI") == 0) 
    {
        if( !is_registered(plane) )
        {
            send_err(plane, "Unregistered plane -- cannot process request");
        }
        else
        {
            cmd_reqtaxi(plane);
        }
    } 
    else if (strcmp(cmd, "REQPOS") == 0)
    {
        if( !is_registered(plane) )
        {
            send_err(plane, "Unregistered plane -- cannot process request");
        }
        else
        {
            cmd_reqpos(plane);
        }
    } 
    else if(strcmp(cmd, "REQAHEAD") == 0)
    {
        if( !is_registered(plane) )
        {
            send_err(plane, "Unregistered plane -- cannot process request");
        }
        else
        {
            cmd_reqahead(plane);
        }
    } 
    else if(strcmp(cmd, "INAIR") == 0)
    {
        if( !is_registered(plane) )
        {
            send_err(plane, "Unregistered plane -- cannot process request");
        }
        else
        {
            cmd_inair(plane);
        }
    } 
    else if (strcmp(cmd, "BYE") == 0) 
    {
        cmd_bye(plane);
    } else 
    {
        DEBUG_PRINT("cmd: %s, args: %s", cmd, args == NULL ? "" : args);
        send_err(plane, "Unknown command");
    }
}
