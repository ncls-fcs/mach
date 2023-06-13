CC = gcc
CFLAGS = -std=c11 -pedantic -Wall -Werror -D_XOPEN_SOURCE=700

.PHONY: all clean

all: mach

clean:
	rm -f mach mach.o queue.o

mach: mach.o
	$(CC) $(CFLAGS) -o mach mach.o pub/run.o pub/sem.o

queue.o:
	$(CC) $(CFLAGS) -c queue.c

mach.o:
	$(CC) $(CFLAGS) -c mach.c