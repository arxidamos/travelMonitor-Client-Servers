#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
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

    // Assign countries to each Monitor
    mapCountryDirs(dir_path, numMonitors, childMonitor);

    for (int i=0; i<numMonitors; i++) {        
        // Create socket
        if ((sockfd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error creating socket");
            exit(1);
        }

        if ((rem = gethostbyname("localhost")) == NULL) {
            perror("Error with gethostbyname");
            exit(1);
        }
        // Assign address and port   
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        memcpy(&(servAddr.sin_addr.s_addr), rem->h_addr, rem->h_length);
        servAddr.sin_port = htons(port);
    
        // sprintf(portString, "%d", port);

        // int pathArgs = childMonitor[i].countryCount; // Number of path arguments
        int pathArgs = childMonitor[i].countryCount;
        int restofArgs = 12;    // Number of rest of arguments (and NULL)
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
            printf("Calling execl in child %d at port %d\n", (int)childpids[i], port);
            execv("./monitorServer", argsArray);
            perror("Error with execv");
        }
        // Connect to Monitor Servers
        int status;
        do {
            status = connect(sockfd[i], (struct sockaddr*)&servAddr, sizeof(servAddr));
            // printf("%d) Status %d\n", i, status);
            // sleep(1);
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

    printf("OK proxwra gamw to arxidi sou\n");

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
                    // read(sockfd[i], buffer, 1);
                    // printf("Read %c\n", buffer[0]);
                    // if (buffer[0] == '1') {
                    //     readyMonitors++;
                    // }
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
            // fgets(input, size, stdin);
                // input[strlen(input)-1] = '\0'; // Cut terminating '\n' from string
                // // Get the command
                // command = strtok(input, " ");
                // if (!strcmp(command, "/exit")) {
                //     // Send SIGKILL signal to every child
                //     free(dir_path);
                //     closedir(input_dir);
                //     free(buffer);
                //     for (int i=0; i<numMonitors; i++) {
                //         kill(childpids[i], SIGKILL);
                //         close(sockfd[i]);
                //     }

                //     for (int i=0; i<numMonitors; i++) {
                //         for (int j=0; j<childMonitor[i].countryCount; j++) {
                //             free(childMonitor[i].country[j]);
                //         }
                //         free(childMonitor[i].country);
                //     }                

                //     exit(0);
                //     // return 1;2
                // }
                // if (!strcmp(command, "/print")) {
                    
                //     for (int i=0; i<numMonitors; i++) {
                //         readyMonitors--;
                //         write(sockfd[i], "p", 1);
                //     }
                //     // return 1;
                // }
                // if (!strcmp(command, "/close")) {
                //     for (int i=0; i<numMonitors; i++) {
                //         write(sockfd[i], "e", 1);
                //     }
                //     // return 1;
            // }


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




//     while (1) {

//             }
//         }
//         // Monitors ready. Receive user queries
//         else {
//             printf("Type a command:\n");
//             int userCommand = getUserCommand(&stats, &readyMonitors, numMonitors, childMonitor, bloomsHead, dir_path, input_dir, readfd, writefd, bufferSize, bloomSize, &accepted, &rejected);
//             // Command is NULL
//             if (userCommand == -1) {
//                 fflush(stdin);
//                 continue;
//             }
//             // Command is /exit
//             else if (userCommand == 1) {
//                 exit(0);
//             }
//         }
//     }
// }