#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "structs.h"
#include "functions.h"

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condNonEmpty;
pthread_cond_t condNonFull;

// Initialise the cylic buffer
void initCyclicBuffer (CyclicBuffer* cBuf, int cyclicBufferSize) {
    cBuf->start = 0;
    cBuf->end = -1;
    cBuf->size = cyclicBufferSize;
    // cBuf->paths =  malloc(sizeof(char*));
    cBuf->paths =  malloc(_POSIX_PATH_MAX);
    cBuf->pathCount = 0;
}

void insertToCyclicBuffer (CyclicBuffer* cBuf, char* path) {
    pthread_mutex_lock(&mtx);

    // printf("Thread %lu Inserting...\n", (long)pthread_self());
    // printf("Thread %lu No of paths %d vs bufSize %d\n",(long)pthread_self(),  cBuf->pathCount, cBuf->size);

    // Wait until cyclic buffer is non full
    while (cBuf->pathCount >= cBuf->size) {
        // printf("Thread %lu waiting, cause paths %d >= cBuf->size %d...\n", (long)pthread_self(), cBuf->pathCount, cBuf->size);
        pthread_cond_wait(&condNonFull, &mtx);
        // sleep(1);
    }
    // Put path in cyclic buffer's 'end' position
    cBuf->end = (cBuf->end + 1) % (cBuf->size);
    cBuf->pathCount++;
    // cBuf->paths = realloc(cBuf->paths, sizeof(char*)*(cBuf->pathCount));
    cBuf->paths = realloc(cBuf->paths, _POSIX_PATH_MAX*(cBuf->pathCount));
    
    // cBuf->paths[cBuf->end] = malloc(strlen(path) + 1);
    // strcpy(cBuf->paths[cBuf->end], path);

    cBuf->paths[cBuf->end] = strdup(path);

    printf("Thread %lu: Inserted [%s] at cBuf->paths[%d]\n", (long)pthread_self(), cBuf->paths[cBuf->end], cBuf->end);
                
    
    pthread_mutex_unlock(&mtx);
}

char* extractFromCyclicBuffer (CyclicBuffer* cBuf, char** path) {
    pthread_mutex_lock(&mtx);

    // printf("Thread %lu Extracting...\n", (long)pthread_self());
    // Wait until cyclic buffer is non empty
    while (cBuf->pathCount <= 0) {
        // printf("Thread %lu waiting, cause paths %d <= 0...\n", (long)pthread_self(), cBuf->pathCount);
        pthread_cond_wait(&condNonEmpty, &mtx);
        // sleep(1);
    }
    // printf("Thread %lu stopped waiting, cause paths %d > 0\n", (long)pthread_self(), cBuf->pathCount);
    // printf("Thread %lu Extracting %s from position cBuf->paths[%d]\n", (long)pthread_self(), cBuf->paths[cBuf->start], cBuf->start);

    // Extract path from cyclic buffer's 'start' position
    (*path) = malloc( strlen(cBuf->paths[cBuf->start]) + 1 );
    strcpy( (*path), cBuf->paths[cBuf->start] );
    // free(cBuf->paths[cBuf->start]);
    cBuf->start = (cBuf->start + 1) % (cBuf->size);
    cBuf->pathCount--;

    pthread_mutex_unlock(&mtx);

    return (*path);

}

// Free memory allocated for cyclic buffer
void freeCyclicBuffer (CyclicBuffer* cBuf) {
    free(cBuf->paths);
    // pthread_cond_destroy(&condNonEmpty);
    // pthread_cond_destroy(&condNonFull);
    pthread_mutex_destroy(&mtx);
}
