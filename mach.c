#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "sem.h"
#include "queue.h"
#include "run.h"

#define MAX_LINE_LENGTH 4096

SEM *blockSemaphore;

static void die(const char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

static int parse_positive_int_or_die(char *str) {
    errno = 0;
    char *endptr;
    long x = strtol(str, &endptr, 10);
    if (errno != 0) {
        die("invalid number");
    }
    // Non empty string was fully parsed
    if (str == endptr || *endptr != '\0') {
        fprintf(stderr, "invalid number\n");
        exit(EXIT_FAILURE);
    }
    if (x <= 0) {
        fprintf(stderr, "number not positive\n");
        exit(EXIT_FAILURE);
    }
    if (x > INT_MAX) {
        fprintf(stderr, "number too large\n");
        exit(EXIT_FAILURE);
    }
    return (int)x;
}

static void *lineThread(void *command) {
    //runs command, passes at the end its output to queue to be printed
    char *commandString = (char *)command;
    printf("%s", commandString);
    free(commandString);
    V(blockSemaphore);
    return NULL;
}

int main(int argc, char **argv) {

    /*
    start output thread
    */

    //parse argv
    if(argc != 3) {
        fprintf(stderr, "usage: mach <number of threads> <mach-file>");
        exit(EXIT_FAILURE);
    }

    int maxNumberOfThreads = parse_positive_int_or_die(argv[1]);
    char *makeFilePath = argv[2];

    //open file
    char *readMode = "r";
    FILE *machFileStream;

    if((machFileStream = fopen(makeFilePath, readMode)) == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }


    char *line = malloc(sizeof(char) * MAX_LINE_LENGTH + 1);
    if(line == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    //int currentline = 0;
    int linesUntilEmptyLine = 0;
    while(!feof(machFileStream)) {
        //count the lines until an empty line and create a new semaphore with the negation of that value + 1 (semaphore will be used to indicate when all commands until that empty line have finished running) BUT maximum of max_threads 
        blockSemaphore = semCreate(-maxNumberOfThreads);   //Semaphore that blocks execution of the next block until every thread has called its V()-function

        while(fgets(line, MAX_LINE_LENGTH + 2, machFileStream)) {
            pthread_t threadIDs[maxNumberOfThreads];    //array of thread id´s

            if(strlen(line) == 1) {
                //TODO: check maxium threads

                //wait for threads to finish and then restart for new block
                
                /*remove "unused" semaphore blockades:*/
                for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                    V(blockSemaphore);
                }
                
                P(blockSemaphore);          //waits until every thread has called its V()-function
                linesUntilEmptyLine = 0;    //resets lines for new block
                printf("Threads ready\n");
                blockSemaphore = semCreate(-maxNumberOfThreads);   //reset semaphore
                continue;                   //starts new block execution
            }else{
                //read individual rows in file and start new thread for each row. also increment lineCounter to indicate already run lines
                char *currentLine = strdup(line);
                pthread_create(&threadIDs[linesUntilEmptyLine], NULL, &lineThread, currentLine);
                

                //stop after each empty line/wait for processes have finished (proccesses will call V() function after completion. Need to wait with P() function until Semaphore is 1 again e.g all proccesses have finished)
                /*
                for (int i = 0; i < linesUntilEmptyLine; i++) {
                    pthread_join(threadIDs[i], )
                }
                */
            }
            linesUntilEmptyLine += 1;
        }
        //error handling for fgets
        if(ferror(machFileStream)) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }
        
    }
    //
}
