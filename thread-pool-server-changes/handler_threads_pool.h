#ifndef HANDLER_THREADS_QUEUE_H
# define HANDLER_THREADS_QUEUE_H

#include <stdio.h>              /* standard I/O routines                 */
#include <pthread.h>            /* pthread functions and data structures */

#include "requests_queue.h"     /* requests queue routines/structs       */
#include "handler_thread.h"     /* handler thread functions/structs      */

/* format of a single thread structure. */
struct handler_thread {
    pthread_t thread;		   /* thread's handle.                      */
    int       thr_id;		   /* 'id' of thread.                       */
    struct handler_thread* next;   /* pointer to next thread, NULL if none. */
};

/* structure for a handler threads pool */
struct handler_threads_pool {
    struct handler_thread* threads;     /* head of linked list of threads.  */
    struct handler_thread* last_thread; /* pointer to last thread.          */
    int num_threads;		        /* number of threads in pool.       */
    int max_thr_id;			/* maximal thread 'id' used so far. */
    pthread_mutex_t* p_mutex;	        /* pool's mutex.                    */
    pthread_cond_t*  p_cond_var;        /* pool's condition variable.       */
    struct requests_queue* requests;    /* requests queue                   */
};

/*
 * create a handler threads pool. associate it with the given mutex
 * and condition variables.
 */
extern struct handler_threads_pool*
init_handler_threads_pool(pthread_mutex_t* p_mutex,
			  pthread_cond_t*  p_cond_var,
			  struct requests_queue* requests);

/* spawn a new handler thread and add it to the threads pool. */
extern void
add_handler_thread(struct handler_threads_pool* pool);

/* delete the first thread from the threads pool (and cancel the thread) */
extern void
delete_handler_thread(struct handler_threads_pool* pool);

/* get the number of handler threads currently in the threads pool */
extern int
get_handler_threads_number(struct handler_threads_pool* pool);

/*
 * free the resources taken by the given requests queue,
 * and cancel all its threads.
 */
extern void
delete_handler_threads_pool(struct handler_threads_pool* pool);

#endif /* HANDLER_THREADS_QUEUE_H */
