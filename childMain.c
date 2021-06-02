#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "structs.h"
#include "functions.h"

CyclicBuffer cBuf;
// pthread_mutex_t mtx;
// pthread_cond_t condNonEmpty;
// pthread_cond_t condNonFull;

// Structures to store records data
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
        // printf("path[%d]=%s\n", index, path[index]);
        index++;
    }
    // Get this Monitor's dirs and respective files
    MonitorDir* monitorDir = NULL;
    readDirs(&monitorDir, path, pathsNumber);
    // printMonitorDirList(monitorDir);

    ////////////////////////////////////////////////////////////////////////////////////
               
    // Common structures to store records data
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

    // Call threads and populate common structures with files' data
    threadFileReader(monitorDir, numThreads);


    // Wait for threads to terminate
    for (int i=0; i<numThreads; i++) {
        pthread_join(threads[i], 0);
        printf("Thread [%d] joined\n", i);
    }

    // printRecordsList(recordsHead);
    // printBloomsList(bloomsHead);
    ////////////////////////////////////////////////////////////////////////////////////
    struct sockaddr_in servAddr;
    struct sockaddr_in clientAddr;
    unsigned int clientLength = sizeof(clientAddr);
    struct hostent* rem;
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
    if ((rem = gethostbyname("localhost")) == NULL) {
        perror("Error with gethostbyname");
        exit(1);
    }
    // Report ready to parent-client
    updateParentBlooms(bloomsHead, newSockfd, socketBufferSize);
    sendMessage('F', "", newSockfd, socketBufferSize);

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
            analyseMessage(&monitorDir, incMessage, newSockfd, &socketBufferSize, &bloomSize, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected);

            // if (buffer[0] == 'p') {
                //     printf("Message received from Server %d\n", (int)getpid());
                //     // write(newSockfd, "8", 1);
                    
                //     // close(sockfd);
                //     // close(newSockfd);
                // }
                // if (buffer[0] == 'e') {
                //     close(sockfd);
                //     close(newSockfd);
                //     // for (int i=0; i<numThreads; i++) {
                //     //     pthread_join(threads[i], 0);
                //     // }

                //     freeCyclicBuffer(&cBuf);
                //     freeMonitorDirList(monitorDir);
                //     kill(getpid(), SIGKILL);
            // }        
            FD_CLR(newSockfd, &incfds);
        }
    }
}

void* threadConsumer (void* ptr) {
    printf("Thread %lu: begin=====================\n",(long)pthread_self());
    char* path;
    while ( (strcmp(extractFromCyclicBuffer(&cBuf, &path), "finish")) ) {
        // printf("Thread %lu: Extracted [%s]\n",(long)pthread_self(), path);
        processFile(path);
        pthread_cond_signal(&condNonFull);
    }
    // Wake up childMain one last time for each thread
    pthread_cond_signal(&condNonFull);
    printf("Thread %lu: exit=====================\n",(long)pthread_self());
    return NULL;

}


void threadFileReader(MonitorDir* monitorDir, int numThreads) {
    MonitorDir* current = monitorDir;
    while (current) {
        
        for (int i=0; i<current->fileCount; i++) {
            insertToCyclicBuffer(&cBuf, current->files[i]);
            pthread_cond_broadcast(&condNonEmpty);

            // printf("Broadcasted\n");

        }
        // free(cBuf.paths);
        current = current->next;
    } 

    for (int i=0; i<numThreads; i++) {
        // Unblock all threads
        insertToCyclicBuffer(&cBuf, "finish");
        pthread_cond_signal(&condNonEmpty);
    }
}



//     // Structure to store Monitor's directory info
//     MonitorDir* monitorDir = NULL;
//     // Structures to store records data
//     BloomFilter* bloomsHead = NULL;
//     State* stateHead = NULL;
//     Record* recordsHead = NULL;
//     SkipList* skipVaccHead = NULL;
//     SkipList* skipNonVaccHead = NULL;
//     // Seed the time generator
//     srand(time(NULL));
//     int accepted = 0;
//     int rejected = 0;

//    // Initialize bufSize and bloomSize
//     int bufSize = 10;
//     int bloomSize = 0;

//     // Get 1st message for bufSize
//     Message* firstMessage = malloc(sizeof(Message));
//     getMessage(firstMessage, incfd, bufSize);
//     analyseMessage(&monitorDir, firstMessage, outfd, &bufSize, &bloomSize, dir_path, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected);
 
//     // Get 2nd message for bloomSize
//     getMessage(firstMessage, incfd, bufSize);
//     analyseMessage(&monitorDir, firstMessage, outfd, &bufSize, &bloomSize, dir_path, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected);

//     free(firstMessage->code);
//     free(firstMessage->body);
//     free(firstMessage);

//     while (1) {
//         // Keep checking signal flags
//         blockSignals();
//         checkSignalFlags(&monitorDir, outfd, bufSize, bloomSize, dir_path, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected);
//         unblockSignals();

//         // Get incoming messages
//         Message* incMessage = malloc(sizeof(Message));
//         if (getMessage(incMessage, incfd, bufSize) == -1) {
//             continue;
//         }

//         // Decode incoming messages
//         analyseMessage(&monitorDir, incMessage, outfd, &bufSize, &bloomSize, dir_path, &bloomsHead, &stateHead, &recordsHead, &skipVaccHead, &skipNonVaccHead, &accepted, &rejected);
//     }
// }