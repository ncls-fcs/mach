CC = gcc
CFLAGS = -std=c11 -g -pthread -pedantic -Wall -Werror -D_XOPEN_SOURCE=700

.PHONY: all clean

all: mach

clean:
	rm -f mach mach.o queue.o

mach: mach.o queue.o
	$(CC) $(CFLAGS) -o mach mach.o queue.o pub/run.o pub/sem.o

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c

mach.o: mach.c
	$(CC) $(CFLAGS) -c mach.c

##P----> #--------------------------------------------------------------------#
         # Punktabzug in dieser Datei: 0.0 Punkte                             #
         #-------------------------------------------------------------------#/
