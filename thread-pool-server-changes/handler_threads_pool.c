#include <stdio.h>              /* standard I/O routines                    */
#include <pthread.h>            /* pthread functions and data structures    */
#include <stdlib.h>             /* malloc() and free()                      */
#include <assert.h>      /* assert()                                  */

#include "handler_threads_pool.h" /* handler threads pool functions/structs */

/*
 * create a handler threads pool. associate it with the given mutex
 * and condition variables.
 */
struct handler_threads_pool*
init_handler_threads_pool(pthread_mutex_t* p_mutex,
			  pthread_cond_t*  p_cond_var,
			  struct requests_queue* requests)
{
    struct handler_threads_pool* pool =
      (struct handler_threads_pool*)malloc(sizeof(struct handler_threads_pool));

    if (!pool) {
	fprintf(stderr, "init_handler_threads_pool: out of memory. exiting\n");
	exit(1);
    }
    /* initialize queue */
    pool->threads = NULL;
    pool->last_thread = NULL;
    pool->num_threads = 0;
    pool->max_thr_id = 0;
    pool->p_mutex = p_mutex;
    pool->p_cond_var = p_cond_var;
    pool->requests = requests;

    return pool;
}

/* spawn a new handler thread and add it to the threads pool. */
void
add_handler_thread(struct handler_threads_pool* pool)
{
    struct handler_thread* a_thread;      /* thread's data       */
    struct handler_thread_params* params; /* thread's parameters */

    /* sanity check */
    assert(pool);

    /* create the new thread's structure and initialize it */
    a_thread = (struct handler_thread*)malloc(sizeof(struct handler_thread));
    if (!a_thread) {
	fprintf(stderr, "add_handler_thread: out of memory. exiting\n");
	exit(1);
    }
    a_thread->thr_id = pool->max_thr_id++;
    a_thread->next = NULL;

    /* create the thread's parameters structure */
    params = (struct handler_thread_params*)
	                           malloc(sizeof(struct handler_thread_params));
    if (!params) {
	fprintf(stderr, "add_handler_thread: out of memory. exiting\n");
	exit(1);
    }
    params->thread_id = a_thread->thr_id;
    params->request_mutex = pool->p_mutex;
    params->got_request = pool->p_cond_var;
    params->requests = pool->requests;

    /* spawn the thread, and place its ID in the thread's structure */
    pthread_create(&a_thread->thread,
		   NULL,
		   handle_requests_loop,
		   (void*)params);

    /* add the thread's structure to the end of the pool's list. */
    if (pool->num_threads == 0) { /* special case - list is empty */
        pool->threads = a_thread;
        pool->last_thread = a_thread;
    }
    else {
        pool->last_thread->next = a_thread;
        pool->last_thread = a_thread;
    }

    /* increase total number of threads by one. */
    pool->num_threads++;
}

/* remove the first thread from the threads pool (do NOT cancel the thread) */
static struct handler_thread* 
remove_first_handler_thread(struct handler_threads_pool* pool)
{
    struct handler_thread* a_thread = NULL;	/* temporary holder */

    /* sanity check */
    assert(pool);

    if (pool->num_threads > 0 && pool->threads) {
	a_thread = pool->threads;
	pool->threads = a_thread->next;
	a_thread->next = NULL;
        pool->num_threads--;
    }

    return a_thread;
}

/* delete the first thread from the threads pool (and cancel the thread) */
void
delete_handler_thread(struct handler_threads_pool* pool)
{
    struct handler_thread* a_thread; /* the thread to cancel */

    /* sanity check */
    assert(pool);

    a_thread = remove_first_handler_thread(pool);
    if (a_thread) {
	pthread_cancel(a_thread->thread);
        free(a_thread);
    }
}

/* get the number of handler threads currently in the threads pool */
int
get_handler_threads_number(struct handler_threads_pool* pool)
{
    /* sanity check */
    assert(pool);

    return pool->num_threads;
}

/*
 * free the resources taken by the given requests queue,
 * and cancel all its threads.
 */
void
delete_handler_threads_pool(struct handler_threads_pool* pool)
{
    void* thr_retval;	              /* thread's return value */
    struct handler_thread* a_thread;  /* one thread's structure */

    /* sanity check */
    assert(pool);

    /* use pthread_join() to wait for all threads to terminate. */
    while (pool->num_threads > 0) {
	a_thread = remove_first_handler_thread(pool);
	assert(a_thread);	/* sanity check */
	pthread_join(a_thread->thread, &thr_retval);
        free(a_thread);
    }
}

