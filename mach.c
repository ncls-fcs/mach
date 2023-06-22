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

#define TERMINATION_SIGNAL 0
#define ERROR_SIGNAL -1
#define RUNNING_SIGNAL 1
#define COMPLETED_SIGNAL 2

static SEM *blockSemaphore;

//TODO: wait for output thread and safely exit and deallocate (probably needs another semaphore so main waits for output thread to exit)

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
    if(queue_put(commandString, NULL, RUNNING_SIGNAL)){
        //error in queue_put
        perror("queue_put");
        exit(EXIT_FAILURE);
    }
    //free(commandString);
    char *out;
    int output = run_cmd(commandString, &out);
    if(output >= 0) {
        queue_put(commandString, out, COMPLETED_SIGNAL);
    }else{
        //Failure
        queue_put(commandString, NULL, ERROR_SIGNAL); 
    }

    V(blockSemaphore);
    return NULL;
}

static void *printThread(void *n) {
    //TODO: let main wait for finished writing ( for example change to joined thread or some)
    //will print anytime a new entry is available in queue.
    char *cmd, *out;
    int flags;
    
    while(1){
        if (queue_get(&cmd, &out, &flags)) {
            perror("queue_get");
            exit(EXIT_FAILURE);
        }
        if(flags == TERMINATION_SIGNAL) {
            pthread_exit(NULL);
        }

        if(flags == RUNNING_SIGNAL) {
            printf("Running '%s' ...\n", cmd);
        }else if(flags == COMPLETED_SIGNAL){
            printf("Completed '%s': \"%s\".\n", cmd, out);
            free(out);
            free(cmd);
        }else if(flags == ERROR_SIGNAL){
            printf("Failure '%s'\n", cmd);
            free(out);
            free(cmd);
        }
    }
}

void waitForThreads() {
    P(blockSemaphore);          //waits until every thread has called its V()-function
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


    char line[MAX_LINE_LENGTH + 1];

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
            for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                V(blockSemaphore);
            }
            waitForThreads();
            break;
        }
        

        if(strlen(line) == 1) {
            //stop after each empty line/wait for processes have finished (proccesses will call V() function after completion. Need to wait with P() function until Semaphore is 1 again i.e. all proccesses have finished)
            //wait for threads to finish and then restart for new block
            
            /* remove "unused" semaphore blockades: */
            for (int i = 0; i < (maxNumberOfThreads - linesUntilEmptyLine) + 1; i++) {
                V(blockSemaphore);
            }
            
            waitForThreads();
            semDestroy(blockSemaphore);
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
            semDestroy(blockSemaphore);
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
    //isEOF = 1;
    queue_put(NULL, NULL, 0);           //indicate to printing task that the file ended and it should terminate (by putting signal into queue, main automatically also waits for queue to exit)
    pthread_join(printThreadID, NULL);  //wait for printing thread to exit

    if(fclose(machFileStream)){
        perror("fclose");
        //main exits anyway
    }
    semDestroy(blockSemaphore);
    queue_deinit();
    return 0;
}

