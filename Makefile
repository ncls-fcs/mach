CC = gcc
CFLAGS = -std=c11 -g -pthread -pedantic -Wall -Werror -D_XOPEN_SOURCE=700

.PHONY: all clean

all: mach

clean:
	rm -f mach mach.o queue.o

mach: mach.o queue.o run.o sem.o
	$(CC) $(CFLAGS) -o mach mach.o queue.o dep/run.o dep/sem.o

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c

mach.o: mach.c
	$(CC) $(CFLAGS) -c mach.c