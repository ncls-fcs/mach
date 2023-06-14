#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <unistd.h>

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
    //adds command to queue, runs command, passes its output to queue to be printed, then indicates being ready by calling V() on blockSemaphore

    pthread_detach(pthread_self());
    char *commandString = (char *)command;
    //adds cmd to queue
    if(queue_put(commandString, NULL, 1)){
        //error in queue_put
        perror("queue_put");
        exit(EXIT_FAILURE);
    }
    //free(commandString);
    char *out;
    int output = run_cmd(commandString, &out);
    if(output >= 0) {
        queue_put(commandString, out, 2);
    }else{
        //Failure
        queue_put(commandString, NULL, -1); 
    }

    V(blockSemaphore);
    return NULL;
}

static void *printThread(void *n) {
    //will print anytime a new entry is available in queue.
    char *cmd, *out;
    int flags;
    
    while(1){
        if (queue_get(&cmd, &out, &flags)) {
            // error handling ...
            printf("queue_get failed");
        }
        if(flags == 1) {
            printf("Running '%s' ...\n", cmd);
        }else if(flags == 2){
            printf("Completed '%s': \"%s\".\n", cmd, out);
        }else if(flags == -1){
            printf("Failure '%s'\n", cmd);
        }
    }
    return NULL;
    //TODO: free resources and wait for thread to end before main exits (thread_join)
}

void waitForThreads() {
    P(blockSemaphore);          //waits until every thread has called its V()-function
    printf("Threads ready\n");
}


int main(int argc, char **argv) {
    if(queue_init()) {
        perror("queue_init");
        exit(EXIT_FAILURE);
    }
    /*
    start output thread
    */
    pthread_t printThreadID;
    pthread_create(&printThreadID, NULL, &printThread, NULL);

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
            
            waitForThreads();
            break;
        }
        

        if(strlen(line) == 1) {
            //stop after each empty line/wait for processes have finished (proccesses will call V() function after completion. Need to wait with P() function until Semaphore is 1 again i.e. all proccesses have finished)
            //wait for threads to finish and then restart for new block
            
            /*remove "unused" semaphore blockades:*/
            for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                V(blockSemaphore);
            }
            
            waitForThreads();
            blockSemaphore = semCreate(-maxNumberOfThreads);   //reset semaphore
            if(!blockSemaphore) {
                die("semCreate");
            }
            linesUntilEmptyLine = 0;    //resets lines for new block
            continue;                   //starts new block execution
        }
        //if not in empty line, remove trailing newline
        line[strlen(line)-1] = '\0';
        
        if(linesUntilEmptyLine >= maxNumberOfThreads) {
            V(blockSemaphore);          //calls V() one time that Semaphore is > 0 after every thread returned
            waitForThreads();

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
        }
            
        //read individual rows in file and start new thread for each row. also increment lineCounter to indicate already run lines
        char *currentLine = strdup(line);
        pthread_create(&threadIDs[linesUntilEmptyLine], NULL, &lineThread, currentLine);
            
        linesUntilEmptyLine += 1;
        
    }
    free(blockSemaphore);
    return 0;
}
