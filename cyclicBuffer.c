#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "structs.h"
#include "functions.h"

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condNonEmpty;
pthread_cond_t condNonFull;

// Initialise the cylic buffer
void initCyclicBuffer (CyclicBuffer* cBuf, int cyclicBufferSize) {
    cBuf->start = 0;
    cBuf->end = -1;
    cBuf->size = cyclicBufferSize;
    cBuf->paths =  malloc(sizeof(char*));
    cBuf->pathCount = 0;
}

void insertCyclicBuffer (CyclicBuffer* cBuf, char* path) {
    pthread_mutex_lock(&mtx);

    // Wait until cyclic buffer is non full
    while (cBuf->pathCount >= cBuf->size) {
        pthread_cond_wait(&condNonFull, &mtx);
    }
    // Put path in cyclic buffer's 'end' position
    cBuf->end = (cBuf->end + 1) % (cBuf->size);
    cBuf->pathCount++;
    cBuf->paths = realloc(cBuf->paths, sizeof(char*)*(cBuf->pathCount));
    cBuf->paths[cBuf->end] = malloc(strlen(path) + 1);
    strcpy(cBuf->paths[cBuf->end], path);

    pthread_mutex_unlock(&mtx);
}

char* extractCyclicBuffer (CyclicBuffer* cBuf, char** path) {
    pthread_mutex_lock(&mtx);

    // Wait until cyclic buffer is non empty
    while (cBuf->pathCount <= 0) {
        pthread_cond_wait(&condNonEmpty, &mtx);
    }
    // Extract path from cyclic buffer's 'start' position
    (*path) = malloc( strlen(cBuf->paths[cBuf->start]) );
    strcpy( (*path), cBuf->paths[cBuf->start] );
    free(cBuf->paths[cBuf->start]);
    cBuf->start = (cBuf->start + 1) % (cBuf->size);
    cBuf->pathCount--;   

    pthread_mutex_unlock(&mtx);
}

// Free memory allocated for cyclic buffer
void freeCyclicBuffer (CyclicBuffer* cBuf) {
    free(cBuf->paths);
}
