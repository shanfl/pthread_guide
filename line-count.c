#include <stdio.h>             /* standard I/O routines                      */
#include <pthread.h>           /* pthread functions and data structures      */

#define DATA_FILE "very_large_data_file"

/* global mutex for our program. assignment initializes it. */
pthread_mutex_t action_mutex = PTHREAD_MUTEX_INITIALIZER;

/* global condition variable for our program. assignment initializes it. */
pthread_cond_t  action_cond   = PTHREAD_COND_INITIALIZER;

/* flag to denote if the user requested to cancel the operation in the middle */
/* 0 means 'no'. */
int cancel_operation = 0;

/*
 * function: restore_coocked_mode - restore normal screen mode.
 * algorithm: uses the 'stty' command to restore normal screen mode.
 *            serves as a cleanup function for the user input thread.
 * input: none.
 * output: none.
 */

void
restore_coocked_mode(void* dummy)
{
#ifdef DEBUG
    printf("restore_coocked_mode: before 'stty -raw echo'\n\r");
    fflush(stdout);
#endif /* DEBUG */
    system("stty -raw echo");
#ifdef DEBUG
    printf("restore_coocked_mode: after 'stty -raw echo'\n\r");
    fflush(stdout);
#endif /* DEBUG */
}

/*
 * function: read_user_input - read user input while long operation in progress.
 * algorithm: put screen in raw mode (without echo), to allow for unbuffered
 *            input.
 *            perform an endless loop of reading user input. If user
 *            pressed 'e', signal our condition variable and end the thread.
 * input: none.
 * output: none.
 */
void*
read_user_input(void* data)
{
    int c;

    /* register cleanup handler */
    pthread_cleanup_push(restore_coocked_mode, NULL);

    /* make sure we're in asynchronous cancelation mode so   */
    /* we can be canceled even when blocked on reading data. */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* put screen in raw data mode */
    system("stty raw -echo");

    /* "endless" loop - read data from the user.            */
    /* terminate the loop if we got a 'e', or are canceled. */
    while ((c = getchar()) != EOF) {
	if (c == 'e') {
#ifdef DEBUG
	    printf("\n\ngot a 'e'\n\n\r");
	    fflush(stdout);
#endif /* DEBUG */
	    /* mark that there was a cancel request by the user */
	    cancel_operation = 1;
	    /* signify that we are done */
	    pthread_cond_signal(&action_cond);
	    pthread_exit(NULL);
	}
    }

    /* pop cleanup handler, while executing it, to restore cooked mode. */
    pthread_cleanup_pop(1);
}

/*
 * function: file_line_count - counts the number of lines in the given file.
 * algorithm: open the data file, read it character by character, and count
 *            all newline characters.
 * input: file name.
 * output: number of lines in the given file.
 */
void*
file_line_count(void* data)
{
    char* data_file = (char*)data;
    FILE* f = fopen(data_file, "r");
    int wc = 0;
    int c;

    if (!f) {
	perror("fopen");
	exit(1);
    }

    /* make sure we're in asynchronous cancelation mode so   */
    /* we can be canceled even when blocked on reading data. */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while ((c = getc(f)) != EOF) {
	if (c == '\n')
	    wc++;
    }

    fclose(f);

    /* signify that we are done. */
    pthread_cond_signal(&action_cond);

    return (void*)wc;
}

/* like any C program, program's execution begins in main */
int
main(int argc, char* argv[])
{
    pthread_t thread_line_count; /* 'handle' of line-counting thread.        */
    pthread_t thread_user_input; /* 'handle' of user-input thread.           */
    void* line_count;		 /* return value from line-counting thread.  */

    printf("Checking file size (press 'e' to cancel operation)...");
    fflush(stdout);

    /* spawn the line counting thread */
    pthread_create(&thread_line_count,
		   NULL,
		   file_line_count,
		   (void*)DATA_FILE);
    /* spawn the user-reading thread */
    pthread_create(&thread_user_input,
		   NULL,
		   read_user_input,
		   (void*)DATA_FILE);

    /* lock the mutex, and wait on the condition variable, */
    /* till one of the threads finishes up and signals it. */
    pthread_mutex_lock(&action_mutex);
    pthread_cond_wait(&action_cond, &action_mutex);
    pthread_mutex_unlock(&action_mutex);

#ifdef DEBUG
    printf("\n\rmain: we got signaled\n\n\r");
    fflush(stdout);
#endif /* DEBUG */

    /* check if we were signaled due to user operation        */
    /* cancelling, or because the line-counting was finished. */
    if (cancel_operation) {
	/* we join it to make sure it restores normal */
	/* screen mode before we print out.           */
        pthread_join(thread_user_input, NULL);
	printf("operation canceled\n");
	fflush(stdout);
        /* cancel the file-checking thread */
        pthread_cancel(thread_line_count);
    }
    else {
        /* join the file line-counting thread, to get its results */
        pthread_join(thread_line_count, &line_count);

        /* cancel and join the user-input thread.     */
	/* we join it to make sure it restores normal */
	/* screen mode before we print out.           */
        pthread_cancel(thread_user_input);
        pthread_join(thread_user_input, NULL);

	/* and print the result */
        printf("'%d' lines.\n", (int)line_count);
    }

    return 0;
}
