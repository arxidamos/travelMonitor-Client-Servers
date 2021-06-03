#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
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
void analyseMessage (MonitorDir** monitorDir, Message* message, int outfd, int* bufSize, int* bloomSize, BloomFilter** bloomsHead, State** stateHead, Record** recordsHead, SkipList** skipVaccHead, SkipList** skipNonVaccHead, int* accepted, int* rejected, int numThreads) {

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
        processUsr1(monitorDir, outfd, *bufSize, message->body, numThreads);
    }
    return;
}

// Read new file in directory after SIGUSR1
void processUsr1(MonitorDir** monitorDir, int sockfd, int bufSize, char* path, int numThreads) {

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

            printf("Checking file %s\n", file);



            

            // File is not included in struct's files
            if ( !(fileInDir(current, file)) ) {

                newFileFound = 1;
                
                // Add file to MonitorDir struct
                insertFile(&current, file);
                printf("File inserted\n");
                // Put file in cyclic buffer and call threads
                sendNewPathToThreads(file, numThreads);

                // Initialization finished, send Blooms to parent
                updateParentBlooms(bloomsHead, sockfd, bufSize);
                // Report process finished to Parent
                sendMessage('F', "", sockfd, bufSize);
            }
        }
    }
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
        free(charArray);

        current = current->next;
    }
}

// Compare two files (aux function for qsort)
int compare (const void * a, const void * b) {
  return strcmp(*(char* const*)a, *(char* const*)b );
}