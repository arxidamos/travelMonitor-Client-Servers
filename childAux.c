#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include "functions.h"
#include "structs.h"

// Get files from dirs
void readDirs (MonitorDir** monitorDir, char** dirPath, int dirPathsNumber) {
    DIR* dir;
    
    // Iterate over all directories
    for (int i=0; i<dirPathsNumber; i++) {

        if ( !(dir = opendir(dirPath[i])) ) {
            perror("Error with opening directory");
            exit(1);
        }

        // Get number of this directory's files 
        int filesCount = 0;
        struct dirent* current;
        while ( (current = readdir(dir)) ) {
            // Count all files except for "." and ".."
            if ( strcmp(current->d_name, ".") && strcmp(current->d_name, "..") ) {
                filesCount++;
            }
        }
        // Reset directory's stream
        rewinddir(dir);

        // Get the file names
        char** files = malloc(sizeof(char*)*filesCount);
        int j = 0;
        while ( (current = readdir(dir)) ) {
            if ( strcmp(current->d_name, ".") && strcmp(current->d_name, "..") ) {
                files[j] = malloc(strlen(dirPath[i]) + 1+ strlen(current->d_name) + 1);
                strcpy(files[j], dirPath[i]);
                strcat(files[j], "/");
                strcat(files[j], current->d_name);
                j++;
            }
        }
        // Sort file names alphabetically
        qsort(files, filesCount, sizeof(char*), compare);
        // Reset directory's stream
        rewinddir(dir);

        // Store all info in a structure
        *monitorDir = insertDir(monitorDir, dir, dirPath[i], files, filesCount);
        for (int j=0; j<filesCount; j++) {
            free(files[j]);
        }
        free(files);
        closedir(dir);
    }
}

// Analyse incoming message in Monitor
void analyseMessage (MonitorDir** monitorDir, Message* message, int sockfd, int outfd, int* bufSize, int* bloomSize, BloomFilter** bloomsHead, State** stateHead, Record** recordsHead, SkipList** skipVaccHead, SkipList** skipNonVaccHead, int* accepted, int* rejected, int numThreads, pthread_t* threads) {
    // Message 't': Parent sends travelRequest query
    if (message->code[0] == 't') {
        char* citizenID;
        char* token2;
        char* token3;
        char* virus;
        char* countryTo;
        char* dateString;
        Date date;
        // 1st token: citizenID
        citizenID = strtok_r(message->body, ";", &token2);
        // 2nd token: virus
        virus = strtok_r(token2, ";", &token3);
        // 3rd token: countryTo
        countryTo = strtok_r(token3, ";", &dateString);
        // 4th token: dateString
        sscanf(dateString, "%d-%d-%d", &date.day, &date.month, &date.year);

        // Check in structures
        char* answer = processTravelRequest(*skipVaccHead, citizenID, virus, date);

        // Answer to Parent, also pass 'countryTo'
        char* fullString = malloc((strlen(countryTo) + 1 + strlen(answer) + 1)*sizeof(char));
        strcpy(fullString, countryTo);
        strcat(fullString, ";");
        strcat(fullString, answer);
        sendMessage('t', fullString, outfd, *bufSize);
        free(fullString);
    }
    // Message '+': Parent informs to increment counters
    else if (message->code[0] == '+') {
        if (!strcmp(message->body, "YES")) {
            (*accepted)++;
        }
        else if (!strcmp(message->body, "NO") || !strcmp(message->body, "BUT")) {
            (*rejected)++;
        }
    }
    // Message 's': Parent sends search vacc status query
    else if (message->code[0] == 's') {
        char* citizenID = malloc((message->length +1)*sizeof(char));
        strcpy(citizenID, message->body);
        // Check if citizenID exists in records
        if (checkExistence((*recordsHead), citizenID)) {
            searchVaccinationStatus((*skipVaccHead), (*skipNonVaccHead), citizenID);
        }
        // Report processing finished to Parent
        sendMessage('F', "", outfd, (*bufSize));
        free(citizenID);
    }
    // Message 'a': Parent sends addVaccinationRecords query
    else if (message->code[0] == 'a') {
        processAddCommand(monitorDir, outfd, *bufSize, message->body, numThreads);
    }
    // Message '!': Parent informs to exit
    else if (message->code[0] == '!') {
        // Inform threads to finish
        createLogFileChild(monitorDir, accepted, rejected);
        for (int i=0; i<numThreads; i++) {
            insertToCyclicBuffer(&cBuf, "finish");
            pthread_cond_signal(&condNonEmpty);
        }
        // Wait for threads to terminate
        for (int i=0; i<numThreads; i++) {
            pthread_join(threads[i], 0);
        }
        
        // Free allocated memory, close socket
        freeCyclicBuffer(&cBuf);
        freeMonitorDirList(*monitorDir);
        if (close(sockfd) < 0) {
            perror("Error with closing newSockfd");
            exit(1);
        }
        free(message->code);
        free(message->body);
        free(message);
        freeStateList(*stateHead);
        freeRecordList(*recordsHead);
        freeSkipLists(*skipNonVaccHead);
        freeSkipLists(*skipVaccHead);
        freeBlooms(*bloomsHead);

        // exit
        exit(0);
    }
    return;
}

// Read new file in directory
void processAddCommand(MonitorDir** monitorDir, int sockfd, int bufSize, char* path, int numThreads) {
    // Find MonitorDir in charge of country
    MonitorDir* current = (*monitorDir);
    while (current) {
        if ( !strcmp(path, current->country) ) {
            break;
        }
        current = current->next;
    }

    // Get each dir's files alphabetically
    struct dirent** directory;
    int filesCount = 0;
    filesCount = scandir(path , &directory, NULL, alphasort);

    int newFileFound = 0;
    // Iterate through files
    for (int i=0; i<filesCount; i++) {
        if ( strcmp(directory[i]->d_name, ".") && strcmp(directory[i]->d_name, "..") ) {
            // Adjust file name to compare with struct's files
            char* file = malloc( (strlen(path) + 1 + strlen(directory[i]->d_name) + 1)*sizeof(char) );
            strcpy(file, path);
            strcat(file, "/");
            strcat(file, directory[i]->d_name);

            // File is not included in struct's files
            if ( !(fileInDir(current, file)) ) {
                newFileFound = 1;
                
                // Add file to MonitorDir struct
                insertFile(&current, file);
                printf("File data inserted\n");
                // Put file in cyclic buffer and call threads
                sendNewPathToThreads(file, numThreads);

                // Initialization finished, send Blooms to parent
                updateParentBlooms(bloomsHead, sockfd, bufSize);
                // Report process finished to Parent
                sendMessage('F', "", sockfd, bufSize);
            }
            free(file);
        }
    }
    for (int i=0; i<filesCount; i++) {
        free(directory[i]);
    }
    free(directory);
    // Report process finished to waiting Parent, even if no new file added
    if (!newFileFound) {
        // Report process finished to Parent
        sendMessage('F', "", sockfd, bufSize);
    }
}

// Send Blooms to Parent
void updateParentBlooms(BloomFilter* bloomsHead, int outfd, int bufSize) {
    BloomFilter* current = bloomsHead;
    // Iterate through all Bloom Filters' bit arrays
    while (current) {
        // Store every non-zero number and its index
        int length = 0;
        int* indices = malloc(sizeof(int));
        for (int i=0; i<(current->size/sizeof(int)); i++) {
            // Store only non zero indices
            if ( current->bitArray[i] != 0) {
                indices = realloc(indices, (length+2)*sizeof(int));
                indices[length] = i; // Save indexSize
                indices[length+1] = current->bitArray[i]; // Save content
                length += 2;
            }
        }

        // Stringify the 'indices-content' array
        char* bArray = malloc(current->size);
        int pos = 0;
        for (int i=0; i<length; i++) {
            pos += sprintf(bArray+pos, "%d-", indices[i]);
        }

        // Final message: BF's virus + ';' + BF's bit array's indices-content
        char* charArray = malloc( (strlen(current->virus) + 1 + strlen(bArray) + 1) * sizeof(char));
        strcpy(charArray, current->virus);
        strcat(charArray, ";");
        strcat(charArray, bArray);

        // Send BF info message to Parent
        sendMessage('B', charArray, outfd, bufSize);
        free(indices);
        free(bArray);
        free(charArray);

        current = current->next;
    }
}

// Compare two files (aux function for qsort)
int compare (const void * a, const void * b) {
  return strcmp(*(char* const*)a, *(char* const*)b );
}

// Print stats to a log file
void createLogFileChild (MonitorDir** monitorDir, int* accepted, int* rejected) {
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

    // 1st print: this Monitor's countries
    MonitorDir* current = (*monitorDir);
    while (current) {

        // MonitorDir.country: "input_dir/" + "country"
        char* token1 = strdup(current->country);
        char* token2;
        token1 = strtok_r(token1, "/", &token2);

        // Keep printing to file till all countries written
        if ( (write(filefd, token2, strlen(token2)) == -1) ) {
            perror("Error with writing to log_file");
            exit(1);
        }
        if ( (write(filefd, "\n", 1) == -1) ) {
            perror("Error with writing to log_file");
            exit(1);
        }
        free(token1);
        current = current->next;
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