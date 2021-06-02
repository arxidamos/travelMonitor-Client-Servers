#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include "functions.h"
#include "structs.h"

// Assign countries to each Monitor
void mapCountryDirs (char* dir_path, int numMonitors, ChildMonitor childMonitor[]) {
    struct dirent** directory;
    int dirCount;
    // Scan directory in alphabetical order
    dirCount = scandir(dir_path, &directory, NULL, alphasort);
    
    // Store each directory to the right Monitor, round robin
    int i=0;
    for (int j=0; j<dirCount; j++) {
       char* dirName = directory[j]->d_name;
        // Avoid assigning dirs . and ..
        if (strcmp(dirName, ".") && strcmp(dirName, "..")) {

            // Store each country in its childMonitor structure
            if (childMonitor[i].countryCount == 0) {
                childMonitor[i].countryCount++;
                childMonitor[i].country = malloc(sizeof(char*)*(childMonitor[i].countryCount));
                // childMonitor[i].country[0] = malloc(strlen(dirName)+1);
                // strcpy(childMonitor[i].country[0], dirName);
                childMonitor[i].country[0] = malloc(strlen(dir_path) + strlen(dirName)+1);
                strcpy(childMonitor[i].country[0], dir_path);
                strcat(childMonitor[i].country[0], dirName);
            }
            else {
                childMonitor[i].countryCount++;
                childMonitor[i].country = realloc(childMonitor[i].country, sizeof(char*)*(childMonitor[i].countryCount));
                childMonitor[i].country[childMonitor[i].countryCount-1] = malloc(strlen(dir_path) + strlen(dirName)+1);
                strcpy(childMonitor[i].country[childMonitor[i].countryCount-1], dir_path);
                strcat(childMonitor[i].country[childMonitor[i].countryCount-1], dirName);
            }
            i = (i+1) % numMonitors;
        }
    }
    for (int i = 0; i < dirCount; i++) {
        free(directory[i]);
    }
    free(directory);

    // // When mapping ready, send 'F' message
    // for (int i=0; i<numMonitors; i++) {
    //     sendMessage('F', "", outfd[i], bufSize);
    // }
    // printf("FINISHED MAPPING COUNTRIES\n");
}

// Create arg array for execv call
void insertExecvArgs(char* argsArray[], int port, char* numThreadsString, char* socketBufferSizeString, char* cyclicBufferSizeString, char* bloomSizeString, char* dir_path, ChildMonitor* childMonitor, int index, int length) {
    char portString[6];
    sprintf(portString, "%d", port);

    argsArray[0] = strdup("monitorServer");
    argsArray[1] = strdup("-p");
    argsArray[2] = strdup(portString);
    argsArray[3] = strdup("-t");
    argsArray[4] = strdup(numThreadsString);
    argsArray[5] = strdup("-b");
    argsArray[6] = strdup(socketBufferSizeString);
    argsArray[7] = strdup("-c");
    argsArray[8] = strdup(cyclicBufferSizeString);
    argsArray[9] = strdup("-s");
    argsArray[10] = strdup(bloomSizeString);
    for (int x=11; x<(childMonitor[index].countryCount+11); x++) {
        argsArray[x] = strdup(childMonitor[index].country[x-11]);
    }
    argsArray[childMonitor[index].countryCount+11] = NULL;

    for (int j=0; j<length; j++) {
        printf("%s ", argsArray[j]);
    }
}




// Analyse incoming message in Parent
void analyseChildMessage(int* sockfd, Message* message, ChildMonitor* childMonitor, int numMonitors, int *readyMonitors, int bufSize, BloomFilter** bloomsHead, int bloomSize, int* accepted, int* rejected, Stats* stats) {
    // Message 'F': Monitor reports processing finished
    if (message->code[0] =='F') {
        (*readyMonitors)++;
    }
    // Message 'B': Monitor sends Bloom Filter
    if (message->code[0] == 'B') {
        BloomFilter* bloomFilter = NULL;
        int k = 16;

        // Message: BF's virus + ; + BF's bit array's indices-content
        char* token1;
        char* token2;
        token1 = strtok_r(message->body, ";", &token2);

        // Update this virus' BF bit array
        if ( (bloomFilter = insertBloomInParent(bloomsHead, token1, bloomSize, k)) ) {
            updateBitArray(bloomFilter, token2);
        }
        else {
            *bloomsHead = createBloom(*bloomsHead, token1, bloomSize, k);
            updateBitArray(*bloomsHead, token2);
        }
    }
    // Message 't': Monitor answers travelRequest query
    if (message->code[0] == 't') {
        char* countryTo;
        char* answer;

        // 1st token: countryTo, 2nd token: answer string
        countryTo = strtok_r(message->body, ";", &answer);

        if (!strcmp(answer, "YES")) {
            printf("REQUEST ACCEPTED – HAPPY TRAVELS\n");
            (*accepted)++;
            informStats(stats, HIT);
        }
        else if (!strcmp(answer, "NO")) {
            printf("REQUEST REJECTED – YOU ARE NOT VACCINATED\n");
            (*rejected)++;
            informStats(stats, MISS);
        }
        else if (!strcmp(answer, "BUT")) {
            printf("REQUEST REJECTED – YOU WILL NEED ANOTHER VACCINATION BEFORE TRAVEL DATE\n");
            (*rejected)++;
            informStats(stats, MISS); 
        }
        (*readyMonitors)++;
        
        // Inform the Monitor in charge of countryTo
        for (int i=0; i<numMonitors; i++) {
            for (int j=0; j<childMonitor[i].countryCount; j++) {
                if ( !strcmp(childMonitor[i].country[j], countryTo) ) {
                    // Send increment counter message
                    sendMessage('+', answer, sockfd[i], bufSize);
                    break;
                }
            }
        }
    }
    // Message '1': Monitor reports reading initial messages
    if (message->code[0] == '1') {
        // Get pid of child that sent message
        int index;
        pid_t pid = (pid_t)atoi(message->body);
        for (int i=0; i<numMonitors; i++) {
            if (pid == childMonitor[i].pid) {
                index = i;
                break;
            }
        }
        // Re-open connection non-blockingly
        close(sockfd[index]);
        char pipeParentReads[25];
        sprintf(pipeParentReads, "./named_pipes/readPipe%d", index);
        // Open reading & writing fds for child process, non-blockingly
        if ((sockfd[index] = open(pipeParentReads, O_RDONLY | O_NONBLOCK)) == -1) {
            perror("Error opening named pipe for reading");
            exit(1);
        }
    }
}

// Replace terminated child
void replaceChild (pid_t pid, char* dir_path, int bufSize, int bloomSize, int numMonitors, int* readfd, int* writefd, ChildMonitor* childMonitor) {
    // Find the terminated child in parent's structure
    int i=0;
    for (i=0; i<numMonitors; i++) {
        if (pid == childMonitor[i].pid) {
            break;
        }
    }

    // Close reading & writing fds for child
    if ( close(readfd[i]) == -1 ) {
        perror("Error closing named pipe for reading");
        exit(1);
    }
    if ( close(writefd[i]) == -1 ) {
        perror("Error closing named pipe for writing");
        exit(1);
    }

    // Name reading & writing named pipes for read and write
    char pipeParentReads[25];
    char pipeParentWrites[25];
    sprintf(pipeParentReads, "./named_pipes/readPipe%d", i);
    sprintf(pipeParentWrites, "./named_pipes/writePipe%d", i);

    // Remove reading & writing named pipes for child
    if ( unlink(pipeParentReads) == -1 ) {
        perror("Error deleting named pipe for reading");
        exit(1);
    }
    if ( unlink(pipeParentWrites) == -1 ) {
        perror("Error deleting named pipe for writing");
        exit(1);
    }

    // Create reading & writing named pipes for read and write
    if (mkfifo(pipeParentReads, RW) == -1) {
        perror("Error creating named pipe");
        exit(1);
    }
    if (mkfifo(pipeParentWrites, RW) == -1) {
        perror("Error creating named pipe");
        exit(1);
    }

    // Create new child process
    if ((pid = fork()) == -1) {
        perror("Error with fork");
        exit(1);
    }
    // Child executes "child" program
    if (pid == 0) {
        execl("./child", "child", pipeParentReads, pipeParentWrites, dir_path, NULL);
        perror("Error with execl");
    }
    // Open reading & writing named pipes for child
    if ((readfd[i] = open(pipeParentReads, O_RDONLY)) == -1) {
        perror("Error opening named pipe for reading");
        exit(1);
    }
    if ((writefd[i] = open(pipeParentWrites, O_WRONLY)) == -1) {
        perror("Error opening named pipe for writing");
        exit(1);
    }

    // Update pid in parent's structure
    childMonitor[i].pid = pid;

    // Convert bufSize and bloomSize to strings
    char bufSizeString[15];
    sprintf(bufSizeString, "%d", bufSize);
    char bloomSizeString[15];
    sprintf(bloomSizeString, "%d", bloomSize);
    // Send bufSize and bloomSize as first two messages
    sendMessage ('1', bufSizeString, writefd[i], bufSize);
    sendMessage ('2', bloomSizeString, writefd[i], bufSize);
    
    // Assign the same countries to new Monitor
    resendCountryDirs(dir_path, numMonitors, writefd[i], childMonitor[i], bufSize);
    printf("FINISHED RESENDING COUNTRIES\n");
}

// Send previous Monitor's countries to new Monitor
void resendCountryDirs (char* dir_path, int numMonitors, int outfd, ChildMonitor childMonitor, int bufSize) {
    for (int i=0; i<childMonitor.countryCount; i++) {
        sendMessage('C', childMonitor.country[i], outfd, bufSize);
    }
    // When mapping ready, send 'F' message
    sendMessage('F', "", outfd, bufSize);
}

// Receive commands from user
int getUserCommand(Stats* stats, int* readyMonitors, int numMonitors, ChildMonitor* childMonitor, BloomFilter* bloomsHead, char* dir_path, DIR* input_dir, int* sockfd, int bufSize, int bloomSize, int* accepted, int* rejected) {
    char* command = NULL;
    int size = 512;
    char input[size];

    // Get user commands
    if ( (fgets(input, size, stdin) == NULL) ) {
        // Check if some signal's flag is on
        // if ( (checkSignalFlagsParent(stats, input_dir, dir_path, bufSize, bloomSize, readyMonitors, numMonitors, incfd, outfd, childMonitor, accepted, rejected, bloomsHead) == 1) ) {
        //     // SIGINT or SIGQUIT caught
        //     return 1;
        // }
        return -1;
    }
    input[strlen(input)-1] = '\0'; // Cut terminating '\n' from string
    
    char* citizenID;
    char* virus;
    Date date, date1, date2;

    // Get the command
    command = strtok(input, " ");
    if (!strcmp(command, "/exit")) {
        // Send SIGKILL signal to every child
        for (int i = 0; i < numMonitors; ++i) {
            printf("SIGKILL sent to child\n");
            kill(childMonitor[i].pid, SIGKILL);
        }

        // Wait till every child finishes
        for (int i = 0; i < numMonitors; ++i) {
            waitpid(-1, NULL, 0);
            printf("Child listened SIGKILL, terminated\n");

        }
        // Create log file
        // createLogFileParent (numMonitors, childMonitor, accepted, rejected);
        
        // Deallocate memory
        exitApp(stats, input_dir, dir_path, bufSize, bloomSize, readyMonitors, numMonitors, sockfd, childMonitor, accepted, rejected, bloomsHead);
        return 1;
    }
    else if (!strcmp(command, "/vaccineStatusBloom")) {
        // Get citizenID
        command = strtok(NULL, " ");
        if (command) {
            citizenID = malloc(strlen(command)+1);
            strcpy(citizenID, command);

            // Get virusName
            command = strtok(NULL, " ");
            if(command) {
                virus = malloc(strlen(command)+1);
                strcpy(virus, command);
                vaccineStatusBloom(bloomsHead, citizenID, virus);
                free(virus);
            }
            else {
                printf("Please enter a virus name\n");
            }
            free(citizenID);
        }
        else {
            printf("Please enter a citizenID and a virus name\n");
        }
    }
    else if (!strcmp(command, "/travelRequest")) {
        // Get citizenID
        command = strtok(NULL, " ");
        if (command) {
            citizenID = malloc(strlen(command)+1);
            strcpy(citizenID, command);

            // Get date
            command = strtok(NULL, " ");
            if (command) {
                sscanf(command, "%d-%d-%d", &date.day, &date.month, &date.year);

                // Get countryFrom
                command = strtok(NULL, " ");
                if (command) {
                    char* countryFrom = malloc(strlen(command)+1);
                    strcpy(countryFrom, command);

                    // Get countryTo
                    command = strtok(NULL, " ");
                    if (command) {
                        char* countryTo = malloc(strlen(command)+1);
                        strcpy(countryTo, command);

                        // Get virusName
                        command = strtok(NULL, " ");
                        if (command) {
                            virus = malloc(strlen(command)+1);
                            strcpy(virus, command);
                            
                            travelRequest(stats, readyMonitors, bloomsHead, childMonitor, numMonitors, sockfd, bufSize, accepted, rejected, citizenID, countryFrom, countryTo, virus, date);

                            free(virus);
                        }
                        else {
                            printf("Please also enter the following parameter: virusName.\n");
                        }
                        free(countryTo);
                    }
                    else {
                        printf("Please also enter the following parameters: countryTo virusName.\n");
                    }
                    free(countryFrom);
                }
                else {
                    printf("Please also enter the following parameters: countryFrom countryTo virusName.\n");
                }
            }
            else {
                printf("Please also enter the following parameters: date countryFrom countryTo virusName.\n");
            }
            free(citizenID);
        }
        else {
            printf("Please enter the following parameters: citizenID date countryFrom countryTo virusName.\n");
        }
    }
    else if (!strcmp(command, "/travelStats")) {
        // Get virusName
        command = strtok(NULL, " ");
        if (command) {
            char* virus = malloc(strlen(command)+1);
            strcpy(virus, command);

            // Get date1
            command = strtok(NULL, " ");
            if (command) {
                sscanf(command, "%d-%d-%d", &date1.day, &date1.month, &date1.year);

                // Get date2
                command = strtok(NULL, " ");
                if (command) {
                    sscanf(command, "%d-%d-%d", &date2.day, &date2.month, &date2.year);

                    // Get [country]
                    command = strtok(NULL, " ");
                    if (command) {
                        char* country = malloc(strlen(command)+1);
                        strcpy(country, command);

                        travelStats((*stats), virus, date1, date2, country);

                        free(country);
                    }
                    // No [country]
                    else {
                        travelStats((*stats), virus, date1, date2, NULL);
                    }
                }
                else {
                    printf("Please also enter the following parameters: date2 [country].\n");
                }
            }
            else {
                printf("Please also enter the following parameters: date1 date2 [country].\n");
            }
            free(virus);
        }
        else {
            printf("Please enter the following parameters: virusName date1 date2 [country].\n");
        }
    }
    else if (!strcmp(command, "/addVaccinationRecords")) {
        // Get citizenID
        command = strtok(NULL, " ");
        if (command) {
            char* country = malloc(strlen(command)+1);
            strcpy(country, command);

            // Find the child that controls this country
            for (int i=0; i<numMonitors; i++) {
                for (int j=0; j<childMonitor[i].countryCount; j++) {
                    if (!strcmp(childMonitor[i].country[j], country)) {
                        // Send SIGUSR1 to child
                        kill(childMonitor[i].pid, SIGUSR1);
                        break;
                    }
                }
            }
            free(country);
        }
        else {
            printf("Please enter country parameter.\n");
        }

    }
    else if (!strcmp(command, "/searchVaccinationStatus")) {
        // Get citizenID
        command = strtok(NULL, " ");
        if (command) {
            citizenID = malloc(strlen(command)+1);
            strcpy(citizenID, command);

            // Send search message to all childs
            for (int i=0; i<numMonitors; i++) {
                (*readyMonitors)--;
                sendMessage('s', citizenID, sockfd[i], bufSize);
            }
            free(citizenID);
        }
        else {
            printf("Please citizenID parameter.\n");
        }
    }
    else {
        printf("Command '%s' is unknown\n", command);
        printf("Please type a known command:\n");
    }
    return 0;
}

// Print log file
void createLogFileParent (int numMonitors, ChildMonitor* childMonitor, int* accepted, int* rejected) {
        // Create file's name, inside "log_files" dir
        char* dirName = "log_files/";
        char* fileName = "log_file.";
        char* fullName = malloc(sizeof(char)*(strlen(dirName) + strlen(fileName) + LENGTH + 1));
        sprintf(fullName, "%s%s%d", dirName, fileName, (int)getpid());

        // Create file
        int filefd;
        if ( (filefd = creat(fullName, RWE)) == -1) {
            perror("Error with creating log_file");
            exit(1);
        }
        free(fullName);

        // 1st print: countries
        for (int i=0; i<numMonitors; i++) {
            for (int j=0; j<childMonitor[i].countryCount; j++) {
                // Keep printing to file till all countries written
                if ( (write(filefd, childMonitor[i].country[j], strlen(childMonitor[i].country[j])) == -1) ) {
                    perror("Error with writing to log_file");
                    exit(1);
                }
                if ( (write(filefd, "\n", 1) == -1) ) {
                    perror("Error with writing to log_file");
                    exit(1);
                }
            }
        }

        // 2nd print: total requests
        char* info = "TOTAL TRAVEL REQUESTS ";
        char* numberString = malloc((strlen(info) + LENGTH)*sizeof(char));
        sprintf(numberString, "%s%d\n", info, (*accepted + *rejected));
        if ( (write(filefd, numberString, strlen(numberString)) == -1) ) {
            perror("Error with writing to log_file");
            exit(1);
        }

        // 3rd print: accepted requests
        info = "ACCEPTED ";
        numberString = realloc(numberString, (strlen(info) + LENGTH)*(sizeof(char)) );
        sprintf(numberString, "%s%d\n", info, (*accepted));
        if ( (write(filefd, numberString, strlen(numberString)) == -1) ) {
            perror("Error with writing to log_file");
            exit(1);
        }

        // 4th print: rejected requests
        info = "REJECTED ";
        numberString = realloc(numberString, (strlen(info) + LENGTH)*(sizeof(char)) );
        sprintf(numberString, "%s%d\n", info, (*rejected));
        if ( write(filefd, numberString, strlen(numberString)) == -1 ) {
            perror("Error with writing to log_file");
            exit(1);
        }
        free(numberString);
}

// Update Bloom Filter's bit array, based on child's message
void updateBitArray (BloomFilter* bloomFilter, char* bitArray) {
    // Recreate the indices-content array
    char* token;
    int* newIndices = malloc(sizeof(int));
    int length = 0;
    // 
    while ( (token = strtok_r(bitArray, "-", &bitArray)) ) {
        // Token: index - BitArray: content
        newIndices = realloc(newIndices, (length+1)*sizeof(int));
        newIndices[length] = atoi(token);
        length++;
    }

    int i=0;
    while (i < length-1) {
        int index = newIndices[i];
        int content = newIndices[i+1];
        bloomFilter->bitArray[index] = content;
        i += 2;
    }
    free(newIndices);
}

// Deallocate memory and exit
void exitApp(Stats* stats, DIR* input_dir, char* dir_path, int bufSize, int bloomSize, int* readyMonitors, int numMonitors, int* sockfd,  ChildMonitor* childMonitor, int* accepted, int* rejected, BloomFilter* bloomsHead) {
    struct dirent** directory;
    int dirCount;
    // Scan directory in alphabetical order
    dirCount = scandir(dir_path, &directory, NULL, alphasort);
    if ( (dirCount-2) < numMonitors) {
        numMonitors = dirCount-2;
    }
    for (int i=0; i<numMonitors; i++) {
        for (int j=0; j<childMonitor[i].countryCount; j++) {
            free(childMonitor[i].country[j]);
        }
        free(childMonitor[i].country);
    }

    for (int i = 0; i < dirCount; i++) {
        free(directory[i]);
    }
    free(directory);
    free(dir_path);
    closedir(input_dir);
    freeStats(stats);
    freeBlooms(bloomsHead);
}