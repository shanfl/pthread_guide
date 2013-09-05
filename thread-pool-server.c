#include <stdio.h>       /* standard I/O routines                     */
#include <pthread.h>     /* pthread functions and data structures     */
#include <stdlib.h>      /* rand() and srand() functions              */

/* number of threads used to service requests */
#define NUM_HANDLER_THREADS 3


/* global mutex for our program. assignment initializes it. */
/* note that we use a RECURSIVE mutex, since a handler      */
/* thread might try to lock it twice consecutively.         */
pthread_mutex_t request_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* global condition variable for our program. assignment initializes it. */
pthread_cond_t  got_request   = PTHREAD_COND_INITIALIZER;

int num_requests = 0;	/* number of pending requests, initially none */

/* format of a single request. */
struct request {
    int number;		    /* number of the request                  */
    struct request* next;   /* pointer to next request, NULL if none. */
};

struct request* requests = NULL;     /* head of linked list of requests. */
struct request* last_request = NULL; /* pointer to last request.         */

/*
 * function add_request(): add a request to the requests list
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request number, linked list mutex.
 * output:    none.
 */
void
add_request(int request_num,
	    pthread_mutex_t* p_mutex,
	    pthread_cond_t*  p_cond_var)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to newly added request.     */

    /* create structure with new request */
    a_request = (struct request*)malloc(sizeof(struct request));
    if (!a_request) { /* malloc failed?? */
	fprintf(stderr, "add_request: out of memory\n");
	exit(1);
    }
    a_request->number = request_num;
    a_request->next = NULL;

    /* lock the mutex, to assure exclusive access to the list */
    rc = pthread_mutex_lock(p_mutex);

    /* add new request to the end of the list, updating list */
    /* pointers as required */
    if (num_requests == 0) { /* special case - list is empty */
	requests = a_request;
	last_request = a_request;
    }
    else {
	last_request->next = a_request;
	last_request = a_request;
    }

    /* increase total number of pending requests by one. */
    num_requests++;

#ifdef DEBUG
    printf("add_request: added request with id '%d'\n", a_request->number);
    fflush(stdout);
#endif /* DEBUG */

    /* unlock mutex */
    rc = pthread_mutex_unlock(p_mutex);

    /* signal the condition variable - there's a new request to handle */
    rc = pthread_cond_signal(p_cond_var);
}

/*
 * function get_request(): gets the first pending request from the requests list
 *                         removing it from the list.
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request number, linked list mutex.
 * output:    pointer to the removed request, or NULL if none.
 * memory:    the returned request need to be freed by the caller.
 */
struct request*
get_request(pthread_mutex_t* p_mutex)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to request.                 */

    /* lock the mutex, to assure exclusive access to the list */
    rc = pthread_mutex_lock(p_mutex);

    if (num_requests > 0) {
	a_request = requests;
	requests = a_request->next;
	if (requests == NULL) { /* this was the last request on the list */
	    last_request = NULL;
	}
	/* decrease the total number of pending requests */
	num_requests--;
    }
    else { /* requests list is empty */
	a_request = NULL;
    }

    /* unlock mutex */
    rc = pthread_mutex_unlock(p_mutex);

    /* return the request to the caller. */
    return a_request;
}

/*
 * function handle_request(): handle a single given request.
 * algorithm: prints a message stating that the given thread handled
 *            the given request.
 * input:     request pointer, id of calling thread.
 * output:    none.
 */
void
handle_request(struct request* a_request, int thread_id)
{
    if (a_request) {
	printf("Thread '%d' handled request '%d'\n",
	       thread_id, a_request->number);
	fflush(stdout);
    }
}

/*
 * function handle_requests_loop(): infinite loop of requests handling
 * algorithm: forever, if there are requests to handle, take the first
 *            and handle it. Then wait on the given condition variable,
 *            and when it is signaled, re-do the loop.
 *            increases number of pending requests by one.
 * input:     id of thread, for printing purposes.
 * output:    none.
 */
void*
handle_requests_loop(void* data)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to a request.               */
    int thread_id = *((int*)data);  /* thread identifying number           */

#ifdef DEBUG
    printf("Starting thread '%d'\n", thread_id);
    fflush(stdout);
#endif /* DEBUG */

    /* lock the mutex, to access the requests list exclusively. */
    rc = pthread_mutex_lock(&request_mutex);

#ifdef DEBUG
    printf("thread '%d' after pthread_mutex_lock\n", thread_id);
    fflush(stdout);
#endif /* DEBUG */

    /* do forever.... */
    while (1) {
#ifdef DEBUG
    	printf("thread '%d', num_requests =  %d\n", thread_id, num_requests);
    	fflush(stdout);
#endif /* DEBUG */
	if (num_requests > 0) { /* a request is pending */
	    a_request = get_request(&request_mutex);
	    if (a_request) { /* got a request - handle it and free it */
    		/* unlock mutex - so other threads would be able to handle */
		/* other reqeusts waiting in the queue paralelly.          */
    		rc = pthread_mutex_unlock(&request_mutex);
		handle_request(a_request, thread_id);
		free(a_request);
    		/* and lock the mutex again. */
    		rc = pthread_mutex_lock(&request_mutex);
	    }
	}
	else {
	    /* wait for a request to arrive. note the mutex will be */
	    /* unlocked here, thus allowing other threads access to */
	    /* requests list.                                       */
#ifdef DEBUG
    	    printf("thread '%d' before pthread_cond_wait\n", thread_id);
    	    fflush(stdout);
#endif /* DEBUG */
	    rc = pthread_cond_wait(&got_request, &request_mutex);
	    /* and after we return from pthread_cond_wait, the mutex  */
	    /* is locked again, so we don't need to lock it ourselves */
#ifdef DEBUG
    	    printf("thread '%d' after pthread_cond_wait\n", thread_id);
    	    fflush(stdout);
#endif /* DEBUG */
	}
    }
}

/* like any C program, program's execution begins in main */
int
main(int argc, char* argv[])
{
    int        i;                                /* loop counter          */
    int        thr_id[NUM_HANDLER_THREADS];      /* thread IDs            */
    pthread_t  p_threads[NUM_HANDLER_THREADS];   /* thread's structures   */
    struct timespec delay;			 /* used for wasting time */

    /* create the request-handling threads */
    for (i=0; i<NUM_HANDLER_THREADS; i++) {
	thr_id[i] = i;
	pthread_create(&p_threads[i], NULL, handle_requests_loop, (void*)&thr_id[i]);
    }

    /* run a loop that generates requests */
    for (i=0; i<600; i++) {
	add_request(i, &request_mutex, &got_request);
	/* pause execution for a little bit, to allow      */
	/* other threads to run and handle some requests.  */
	if (rand() > 3*(RAND_MAX/4)) { /* this is done about 25% of the time */
	    delay.tv_sec = 0;
	    delay.tv_nsec = 10;
	    nanosleep(&delay, NULL);
	}
    }
    /* now wait till there are no more requests to process */
    sleep(5);

    printf("Glory,  we are done.\n");
    
    return 0;
}
