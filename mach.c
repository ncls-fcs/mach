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
    pthread_detach(pthread_self());
    //runs command, passes at the end its output to queue to be printed
    char *commandString = (char *)command;
    printf("%s", commandString);
    free(commandString);
    V(blockSemaphore);
    return NULL;
}

static void *printThread(void *n) {
    //will print anytime a new entry is available in queue. (P() before read/print statement)
    return NULL;
}

int main(int argc, char **argv) {

    /*
    start output thread
    */
    printThread(NULL);

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
    blockSemaphore = semCreate(-maxNumberOfThreads);    //Semaphore that blocks execution of the next block until every thread has called its V()-function
    if(!blockSemaphore) {
        die("semCreate");
    }
    pthread_t threadIDs[maxNumberOfThreads];            //array of thread id´s

    while(1) {
        fgets(line, MAX_LINE_LENGTH + 2, machFileStream);
        //error handling for fgets
        if(ferror(machFileStream)) {
            perror("fgets");
            exit(EXIT_FAILURE);
        }else if (feof(machFileStream)) {
            //TODO: cleanup and memory free etc.
            printf("EOF");
            for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                V(blockSemaphore);
            }
            
            P(blockSemaphore);          //waits until every thread has called its V()-function
            printf("Threads ready\n");
            break;
        }
        

        if(strlen(line) == 1) {
            //stop after each empty line/wait for processes have finished (proccesses will call V() function after completion. Need to wait with P() function until Semaphore is 1 again i.e. all proccesses have finished)
            //wait for threads to finish and then restart for new block
            
            /*remove "unused" semaphore blockades:*/
            for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                V(blockSemaphore);
            }
            
            P(blockSemaphore);          //waits until every thread has called its V()-function
            printf("Threads ready\n");
            blockSemaphore = semCreate(-maxNumberOfThreads);   //reset semaphore
            if(!blockSemaphore) {
                die("semCreate");
            }
            linesUntilEmptyLine = 0;    //resets lines for new block
            continue;                   //starts new block execution

        }else if(linesUntilEmptyLine >= maxNumberOfThreads) {
            V(blockSemaphore);          //calls V() one time that Semaphore is > 0 after every thread returned
            P(blockSemaphore);          //waits until every thread has called its V()-function
            printf("Threads ready\n");

            //if we wait for threads because the maxium quantity of threads was reached, we still need to handle the current line -> solution: after wait is completed, start new thread with current line and then carry on like normal
            
            blockSemaphore = semCreate(-maxNumberOfThreads);   //reset semaphore
            if(!blockSemaphore) {
                die("semCreate");
            }
            linesUntilEmptyLine = 0;    //resets lines for new block
            char *currentLine = strdup(line);
            pthread_create(&threadIDs[linesUntilEmptyLine], NULL, &lineThread, currentLine); 
            linesUntilEmptyLine += 1;
            continue;
        }else{
            //read individual rows in file and start new thread for each row. also increment lineCounter to indicate already run lines
            char *currentLine = strdup(line);
            pthread_create(&threadIDs[linesUntilEmptyLine], NULL, &lineThread, currentLine);
            
        }
        linesUntilEmptyLine += 1;
        
    }
}
