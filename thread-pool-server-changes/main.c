#include <stdio.h>             /* standard I/O routines                      */
#include <pthread.h>           /* pthread functions and data structures      */
#include <stdlib.h>            /* rand() and srand() functions               */
#include <unistd.h>            /* sleep()                                    */
#include <assert.h>            /* assert()                                   */

#include "requests_queue.h"         /* requests queue routines/structs       */
#include "handler_thread.h"         /* handler thread functions/structs      */
#include "handler_threads_pool.h"   /* handler thread list functions/structs */

/* number of initial threads used to service requests, and max number */
/* of handler threads to create during "high pressure" times.         */
#define NUM_HANDLER_THREADS 3
#define MAX_NUM_HANDLER_THREADS 14

/* number of requests on the queue warranting creation of new threads */
#define HIGH_REQUESTS_WATERMARK 15
#define LOW_REQUESTS_WATERMARK 3

/* global mutex for our program. assignment initializes it. */
/* note that we use a RECURSIVE mutex, since a handler      */
/* thread might try to lock it twice consecutively.         */
pthread_mutex_t request_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* global condition variable for our program. assignment initializes it. */
pthread_cond_t  got_request   = PTHREAD_COND_INITIALIZER;

/* are we done creating new requests? */
int done_creating_requests = 0;

/* like any C program, program's execution begins in main */
int
main(int argc, char* argv[])
{
    int        i;                                /* loop counter          */
    struct timespec delay;			 /* used for wasting time */
    struct requests_queue* requests = NULL;  /* pointer to requests queue */
    struct handler_threads_pool* handler_threads = NULL;
					       /* list of handler threads */

    /* create the requests queue */
    requests = init_requests_queue(&request_mutex, &got_request);
    assert(requests);

    /* create the handler threads list */
    handler_threads =
	init_handler_threads_pool(&request_mutex, &got_request, requests);
    assert(handler_threads);

    /* create the request-handling threads */
    for (i=0; i<NUM_HANDLER_THREADS; i++) {
	add_handler_thread(handler_threads);
    }

    /* run a loop that generates requests */
    for (i=0; i<600; i++) {
	int num_requests; // number of requests waiting to be handled.
	int num_threads;  // number of active handler threads.

	add_request(requests, i);

	num_requests = get_requests_number(requests);
	num_threads = get_handler_threads_number(handler_threads);

	/* if there are too many requests on the queue, spawn new threads */
	/* if there are few requests and too many handler threads, cancel */
	/* a handler thread.          					  */
	if (num_requests > HIGH_REQUESTS_WATERMARK &&
	    num_threads < MAX_NUM_HANDLER_THREADS) {
		printf("main: adding thread: '%d' requests, '%d' threads\n",
		       num_requests, num_threads);
		add_handler_thread(handler_threads);
	}
	if (num_requests < LOW_REQUESTS_WATERMARK &&
		 num_threads > NUM_HANDLER_THREADS) {
	    printf("main: deleting thread: '%d' requests, '%d' threads\n",
		   num_requests, num_threads);
	    delete_handler_thread(handler_threads);
	}

	/* pause execution for a little bit, to allow      */
	/* other threads to run and handle some requests.  */
	if (rand() > 3*(RAND_MAX/4)) { /* this is done about 25% of the time */
	    delay.tv_sec = 0;
	    delay.tv_nsec = 1;
	    nanosleep(&delay, NULL);
	}
    }
    /* modify the flag to tell the handler threads no   */
    /* new requests will be generated.                  */
    {
        int rc;

        rc = pthread_mutex_lock(&request_mutex);
        done_creating_requests = 1;
        rc = pthread_cond_broadcast(&got_request);
        rc = pthread_mutex_unlock(&request_mutex);
    }

    /* cleanup */
    delete_handler_threads_pool(handler_threads);
    delete_requests_queue(requests);
    
    printf("Glory,  we are done.\n");

    return 0;
}
