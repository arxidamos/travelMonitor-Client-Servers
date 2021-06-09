#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include "structs.h"
#include "functions.h"

// Buffer, mutex, condition vars
CyclicBuffer cBuf;
pthread_mutex_t mtx;
pthread_cond_t condNonEmpty;
pthread_cond_t condNonFull;

// Common structures to store records data
BloomFilter* bloomsHead;
State* stateHead;
Record* recordsHead;
SkipList* skipVaccHead;
SkipList* skipNonVaccHead;
int bloomSize;

int main(int argc, char* argv[]) {
    int port, numThreads, socketBufferSize, cyclicBufferSize;

    // Scan command line arguments
	for (int i=0; i<argc; ++i) {
        if (!strcmp("-p", argv[i])) {
            port = atoi(argv[i+1]);
        }
		else if (!strcmp("-t", argv[i])) {
			numThreads = atoi(argv[i+1]);
        }        
        else if (!strcmp("-b", argv[i])) {
            socketBufferSize = atoi(argv[i+1]);
        }
        else if (!strcmp("-c", argv[i])) {
            cyclicBufferSize = atoi(argv[i+1]);
        }
		else if (!strcmp("-s", argv[i])) {
			bloomSize = atoi(argv[i+1]);
		}
	}
    int pathsNumber = argc - 11;
    char* path[pathsNumber];
    int index = 0;
    // Get paths (after first 10 args)
    for (int i=11; i<argc; i++) {
        path[index] = argv[i];
        index++;
    }
    // Get this Monitor's dirs and respective files
    MonitorDir* monitorDir = NULL;
    readDirs(&monitorDir, path, pathsNumber);

    // Initialise structures
    bloomsHead = NULL;
    stateHead = NULL;
    recordsHead = NULL;
    skipVaccHead = NULL;
    skipNonVaccHead = NULL;
    // Seed the time generator
    srand(time(NULL));
    int accepted = 0;
    int rejected = 0;

    // Initialise cyclic buffer's fields, mutex, condition vars
    initCyclicBuffer(&cBuf, cyclicBufferSize);
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init(&condNonEmpty, NULL);
    pthread_cond_init(&condNonFull, NULL);

    // Create numThreads threads
    pthread_t threads[numThreads];
    for (int i=0; i<numThreads; i++) {
        pthread_create(&threads[i], NULL, threadConsumer, NULL);
    }

    // Put paths in cyclic buffer, call threads
    sendPathsToThreads(monitorDir, numThreads);

    // Set up connection
    struct sockaddr_in servAddr;
    struct sockaddr_in clientAddr;
    unsigned int clientLength = sizeof(clientAddr);
    struct hostent* rem;
    char hostName[_POSIX_PATH_MAX];
    int sockfd;
    int newSockfd;
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }
    // Accept multiple connections
    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) == -1) {
        perror("Error with setsockopt");
        exit(1);
    }
    // Assign address and port
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    // Bind socket to this address
    if ( (bind(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr))) < 0 ) {
        perror("Error with bind");
        exit(1);
    }
    // Listen for connections
    if (listen(sockfd, 5) < 0) {
        perror("Error with listen");
        exit(1);
    }
    printf("Listening for connections to port %d\n", port);
    // Accept connection
    if ( (newSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientLength)) < 0 ) {
        perror("Error with accept");
        exit(1);
    }
    if (gethostname(hostName, _POSIX_PATH_MAX) < 0 ) {
        perror("Error with gethostname");
        exit(1);
    }
    if ((rem = gethostbyname(hostName)) == NULL) {
        perror("Error with gethostbyname");
        exit(1);
    }
    // Send Bloom Filters (done by threads) to parent-client
    updateParentBlooms(bloomsHead, newSockfd, socketBufferSize);
    // Report ready to parent-client
    sendMessage('F', "", newSockfd, socketBufferSize);

    // Keep receiving from parent with select
    fd_set incfds;
    
    while (1) {
        FD_ZERO(&incfds);
        FD_SET(newSockfd, &incfds);
        // Select() on incfds
        int retVal;
        if ( (retVal = select(FD_SETSIZE, &incfds, NULL, NULL, NULL)) == -1) {
            perror("Error with select");
        }
        if (retVal == 0) {
            continue;
        }
        // Check if available data in this fd
        if (FD_ISSET(newSockfd, &incfds)) {

            // Get incoming messages
            Message* incMessage = malloc(sizeof(Message));
            if (getMessage(incMessage, newSockfd, socketBufferSize) == -1) {
                continue;
            }

            // Decode incoming messages
            analyseMessage(&monitorDir, incMessage, sockfd, newSockfd, &socketBufferSize, &bloomSize, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected, numThreads, threads);
            free(incMessage->code);
            free(incMessage->body);
            free(incMessage);

            FD_CLR(newSockfd, &incfds);
        }
    }
}