#include "uffd.h"

// Dequeues: removes first entry
static void *dequeue(void *queue[], int *pnentries) {
    void *ret = *pnentries == 0 ? NULL : queue[0];
    for (int i = 1; i < *pnentries; ++i) {
        queue[i - 1] = queue[i]; 
    }
    queue[--(*pnentries)] = NULL;
    return ret;
}

// Enqueues new page faulting address. If queue is full, dequeues then enqueues
// The scenario where a page fault occurs at an address that is already in the
// queue is impossible.
void *enqueue(void *addr, void *queue[], int *pnentries) {
    void *ret = NULL;
    if (*pnentries == FIFO_SIZE)
        ret = dequeue(queue, pnentries);
    queue[(*pnentries)++] = addr;
    return ret;
}

void dump_queue(void *queue[], int nentries) {
    if (nentries == 0) {
        fprintf(stderr, "<EMPTY_QUEUE>\n");
    } else {
        fprintf(stderr, "{%p", queue[0]);
        for (int i = 1; i < nentries; ++i) {
            fprintf(stderr, ", %p", queue[i]);
        }
        fprintf(stderr, "}\n");
    }
}