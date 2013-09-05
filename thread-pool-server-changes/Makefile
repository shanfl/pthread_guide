# Compiler/Linker
CC = gcc
LD = gcc

# compiler/linker flags
CFLAGS = -g -Wall -D_GNU_SOURCE
LDFLAGS = -g

# files removal
RM = /bin/rm -f

# library to use when linking the main program
LIBS = -lpthread

# program's object files
PROG_OBJS = handler_thread.o handler_threads_pool.o main.o requests_queue.o

# program's executable
PROG = thread-pool-server

# top-level rule
all: $(PROG)

$(PROG): $(PROG_OBJS)
	$(LD) $(LDFLAGS) $(PROG_OBJS) $(LIBS) -o $(PROG)

# compile C source files into object files.
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# clean everything
clean:
	$(RM) $(PROG_OBJS) $(PROG)

