#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "structs.h"
#include "functions.h"

// Initialise the cylic buffer
void initCyclicBuffer (CyclicBuffer* cBuf, int cyclicBufferSize) {
    cBuf->start = 0;
    cBuf->end = -1;
    cBuf->size = cyclicBufferSize;
    cBuf->paths = malloc(_POSIX_PATH_MAX);
    cBuf->pathCount = 0;
}

// Insert to cyclic buffer
void insertToCyclicBuffer (CyclicBuffer* cBuf, char* path) {
    pthread_mutex_lock(&mtx);

    // Wait until cyclic buffer is non full
    while (cBuf->pathCount >= cBuf->size) {
        pthread_cond_wait(&condNonFull, &mtx);
    }
    // Put path in cyclic buffer's 'end' position
    cBuf->end = (cBuf->end + 1) % (cBuf->size);
    cBuf->pathCount++;
    cBuf->paths = realloc(cBuf->paths, _POSIX_PATH_MAX*(cBuf->pathCount));
    cBuf->paths[cBuf->end] = strdup(path);
    
    pthread_mutex_unlock(&mtx);
}

// Extract from cyclic buffer
char* extractFromCyclicBuffer (CyclicBuffer* cBuf, char** path) {
    pthread_mutex_lock(&mtx);

    // Wait until cyclic buffer is non empty
    while (cBuf->pathCount <= 0) {
        pthread_cond_wait(&condNonEmpty, &mtx);
    }

    // Extract path from cyclic buffer's 'start' position
    (*path) = malloc( strlen(cBuf->paths[cBuf->start]) + 1 );
    strcpy( (*path), cBuf->paths[cBuf->start] );
    free(cBuf->paths[cBuf->start]);
    cBuf->start = (cBuf->start + 1) % (cBuf->size);
    cBuf->pathCount--;
    pthread_mutex_unlock(&mtx);

    return (*path);
}

// Free memory allocated for cyclic buffer
void freeCyclicBuffer (CyclicBuffer* cBuf) {
    free(cBuf->paths);
    pthread_cond_destroy(&condNonEmpty);
    pthread_cond_destroy(&condNonFull);
    pthread_mutex_destroy(&mtx);
}
