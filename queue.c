#include <errno.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"

// TODO: includes, structs, variables

static struct queueElement {
    char *cmd;
    char *out;
    int flags;
    struct queueElement *next;
} *head;

SEM *s;

int queue_init(void) {
    s = semCreate(0);
    if(!s) {
        perror("semCreate");
        exit(EXIT_FAILURE);
    }
    return -1;
}

void queue_deinit(void) {
    // TODO: implement me
}

int queue_put(char *cmd, char *out, int flags) {
    // TODO: implement me
    // V(s)
    return -1;
}

int queue_get(char **cmd, char **out, int *flags) {
    // TODO: implement me

    // P(s)
    //
    return -1;
}
