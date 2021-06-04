#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include "functions.h"
#include "structs.h"


int main(int argc, char **argv) {
    int numMonitors;
    int socketBufferSize;
    char* socketBufferSizeString;
    int cyclicBufferSize;
    char* cyclicBufferSizeString;
    int bloomSize;
    char* bloomSizeString;
    char* dir_path;
    DIR* input_dir;
    int numThreads;
    char* numThreadsString;

	// Scan command line arguments
    if (argc != 13) {
        fprintf(stderr, "Error: 12 parameters are required.\n");
        exit(EXIT_FAILURE);
    }
	for (int i=0; i<argc; ++i) {
        if (!strcmp("-m", argv[i])) {
            numMonitors = atoi(argv[i+1]);
            if (numMonitors <= 0) {
				fprintf(stderr, "Error: Number of Monitors must be a positive number.\n");
				exit(EXIT_FAILURE);
            }
        }
        else if (!strcmp("-b", argv[i])) {
            socketBufferSize = atoi(argv[i+1]);
            socketBufferSizeString = argv[i+1];
            if (socketBufferSize <= 0) {
				fprintf(stderr, "Error: Size of socket buffer must be a positive number.\n");
				exit(EXIT_FAILURE);
            }
        }
        else if (!strcmp("-c", argv[i])) {
            cyclicBufferSize = atoi(argv[i+1]);
            cyclicBufferSizeString = argv[i+1];
            if (cyclicBufferSize <= 0) {
				fprintf(stderr, "Error: Size of cyclic buffer must be a positive number.\n");
				exit(EXIT_FAILURE);
            }
        }
		else if (!strcmp("-s", argv[i])) {
			bloomSize = atoi(argv[i+1]);
            bloomSizeString = argv[i+1];
			if (bloomSize <= 0) {
				fprintf(stderr, "Error: Bloom filter size must be a positive number.\n");
				exit(EXIT_FAILURE);
			}
		}
		else if (!strcmp("-i", argv[i])) {
            dir_path = malloc(strlen(argv[i+1]) + 1);
            strcpy(dir_path, argv[i+1]);
            input_dir = opendir(dir_path);
            if(input_dir == NULL) {
                fprintf(stderr, "Cannot open directory: %s\n", argv[i+1]);
                return 1;
            }
        }
		else if (!strcmp("-t", argv[i])) {
			numThreads = atoi(argv[i+1]);
            numThreadsString = argv[i+1];
			if (numThreads <= 0) {
				fprintf(stderr, "Error: Number of threads must be a positive number.\n");
				exit(EXIT_FAILURE);
			}
        }        
	}

    // Create directory for the log file
    if (mkdir("./log_files", RWE) == -1) {
        perror("Error creating log_files directory");
        exit(1);
    }

    // Store pids with respective country dirs
    ChildMonitor childMonitor[numMonitors];
    // Initialize num of countries for each Monitor
    for (int i=0; i<numMonitors; i++) {
        childMonitor[i].countryCount = 0;
    }

    // Declare vars
    pid_t childpids[numMonitors];
    int port = 10000;
    int sockfd[numMonitors];
    struct sockaddr_in servAddr;
    struct hostent* rem;
    char hostName[_POSIX_PATH_MAX];

    // Assign countries to each Monitor
    mapCountryDirs(dir_path, numMonitors, childMonitor);

    for (int i=0; i<numMonitors; i++) {        
        // Create socket
        if ((sockfd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error creating socket");
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
        // Assign address and port   
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        memcpy(&(servAddr.sin_addr.s_addr), rem->h_addr, rem->h_length);
        servAddr.sin_port = htons(port);
    
        // Form the args array that we pass to execv
        int pathArgs = childMonitor[i].countryCount; // Number of path args
        int restofArgs = 12;    // Number of rest of args (and NULL)
        int length = pathArgs + restofArgs;
        char* argsArray[length];
        insertExecvArgs(argsArray, port, numThreadsString, socketBufferSizeString, cyclicBufferSizeString, bloomSizeString, dir_path, childMonitor, i, length);

        printf("Forking...\n");

        // Create child processes
        if ((childpids[i] = fork()) == -1) {
            perror("Error with fork");
            exit(1);
        }
        // Child executes "child" program
        if (childpids[i] == 0) {
            execv("./monitorServer", argsArray);
            perror("Error with execv");
        }
        // Connect to Monitor Servers (once they are up)
        int status;
        do {
            status = connect(sockfd[i], (struct sockaddr*)&servAddr, sizeof(servAddr));
        }
        while (status < 0);

        // Update parent's info for Monitors
        childMonitor[i].pid = childpids[i];

        // Deallocate argsArray memory
        for (int x=0; x<(pathArgs+restofArgs); x++) {
            free(argsArray[x]);
        }
        
        // Change port for next Monitor Server
        port++;
    }

    // Var to monitor if child is about to send message
    int readyMonitors = 0;
    // Vars for stats
    int accepted =0;
    int rejected = 0;
    Stats stats;
    initStats(&stats);
    // Initialise structure
    BloomFilter* bloomsHead = NULL;

    fd_set incfds;

    while (1) {
        // Monitor(s) about to send message
        if (readyMonitors < numMonitors) {
            // Zero the fd_set
            FD_ZERO(&incfds);
            for (int i=0; i<numMonitors; i++) {
                FD_SET(sockfd[i], &incfds);
            }
            // Select() on incfds
            int retVal;
            if ( (retVal = select(FD_SETSIZE, &incfds, NULL, NULL, NULL)) == -1) {
                perror("Error with select");
            }
            if (retVal == 0) {
                // No child process' state has changed
                continue;
            }
            // Iterate over fds to check if inside readFds
            for (int i=0; i<numMonitors; i++) {
                // Check if available data in this fd
                if (FD_ISSET(sockfd[i], &incfds)) {
                    // Read incoming messages
                    Message* incMessage = malloc(sizeof(Message));
                    getMessage(incMessage, sockfd[i], socketBufferSize);
                    
                    // Decode incoming messages
                    analyseChildMessage(sockfd, incMessage, childMonitor, numMonitors, &readyMonitors, socketBufferSize, &bloomsHead, bloomSize, &accepted, &rejected, &stats);

                    FD_CLR(sockfd[i], &incfds);
                    free(incMessage->code);
                    free(incMessage->body);
                    free(incMessage);
                }
            }
        }
        // Monitors ready. Receive user queries
        else {
            printf("Type a command:\n");

            int userCommand = getUserCommand(&stats, &readyMonitors, numMonitors, childMonitor, bloomsHead, dir_path, input_dir, sockfd, socketBufferSize, bloomSize, &accepted, &rejected);
            // Command is NULL
            if (userCommand == -1) {
                fflush(stdin);
                continue;
            }
            // Command is /exit
            else if (userCommand == 1) {
                exit(0);
            }
        }
    }
}