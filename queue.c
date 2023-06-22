#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "queue.h"
#include "sem.h"

static struct queueElement {
    char *cmd;
    char *out;
    int flags;
    struct queueElement *next;
} *head;

SEM *s;
SEM *readyToWrite;

int queue_init(void) {
    s = semCreate(0);
    if(!s) {
        //errno already set by semCreate;
        free(s);
        return -1;
    }
    readyToWrite = semCreate(1);
    if(!readyToWrite) {
        //errno already set by semCreate;
        free(readyToWrite);
        return -1;
    }
    return 0;
}

void queue_deinit(void) {
    free(s);
    free(readyToWrite);
}

int queue_put(char *cmd, char *out, int flags) {
    P(readyToWrite);        //entering critical write area

    struct queueElement *lauf = head;
    struct queueElement *schlepp = NULL;

    while (lauf) {
        schlepp = lauf;
        lauf = lauf->next;
    }

    lauf = malloc(sizeof(struct queueElement));
    if (lauf == NULL) {
        //TODO: errno = 
        return -1;
    }

    lauf->cmd = cmd;
    lauf->out = out;
    lauf->flags = flags;

    lauf->next = NULL;

    if (schlepp == NULL) {
        head = lauf;
    } else {
        schlepp->next = lauf;
    }

    V(readyToWrite);    //indicate completed write, next thread can add/read to/from queue

    V(s);               //indicate new item to be read in queue
    return 0;
}

int queue_get(char **cmd, char **out, int *flags) {

    P(s);               //wait until an element is in list
    P(readyToWrite);    //entering critical area where queue might be changed

    if(head) {
        //write to pointer
        *cmd = head->cmd;
        *out = head->out;
        *flags = head-> flags;
        //remove from queue
        struct queueElement *tmp = head;    //save current element to free it later 
        head = head->next;                  //head either NULL if no further element was in queue (head->next would be NULL), or head is next element
        //free queue element
        free(tmp);

        V(readyToWrite);        //exiting critical area where queue might be changed
        return 0;
    }else{
        return -1;  //No element was in list despite it being indicated by semaphore (idk how that would even happen but error handling = good)
    }

}

