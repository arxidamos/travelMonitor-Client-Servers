#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "structs.h"
#include "functions.h"

// Put paths in cyclic buffer and call threads
void sendPathsToThreads(MonitorDir* monitorDir, int numThreads) {
    MonitorDir* current = monitorDir;
    while (current) {
        for (int i=0; i<current->fileCount; i++) {
            insertToCyclicBuffer(&cBuf, current->files[i]);
            pthread_cond_broadcast(&condNonEmpty);
        }
        current = current->next;
    } 

    // // Once done, inform threads to finish 
    // for (int i=0; i<numThreads; i++) {
    //     // Wake all threads
    //     insertToCyclicBuffer(&cBuf, "finish");
    //     pthread_cond_signal(&condNonEmpty);
    // }
}

// Put new path in cyclic buffer and call threads
void sendNewPathToThreads(char* file, int numThreads) {
    insertToCyclicBuffer(&cBuf, file);
    pthread_cond_broadcast(&condNonEmpty);

    // // Once done, inform threads to finish 
    // for (int i=0; i<numThreads; i++) {
    //     // Wake all threads
    //     insertToCyclicBuffer(&cBuf, "finish");
    //     pthread_cond_signal(&condNonEmpty);
    // }
}


// Read path from cyclic buffer and call processFile
void* threadConsumer (void* ptr) {
    printf("Thread %lu: begin=====================\n",(long)pthread_self());
    char* path;
    // Repeat until "finish" received from main thread
    while ( (strcmp(extractFromCyclicBuffer(&cBuf, &path), "finish")) ) {
        processFile(path);
        pthread_cond_signal(&condNonFull);
    }
    // Wake up childMain one last time for each thread
    pthread_cond_signal(&condNonFull);
    printf("Thread %lu: exit=====================\n",(long)pthread_self());
    return NULL;

}

// Read file and store its data
void processFile (char* filePath) {
    // Initialize variables
    FILE* inputFile;
    size_t textSize;
    char* text = NULL;
    char* token = NULL;
    char* citizenID;
    char* fName;
    char* lName;
    char* country;
    int age;
    char* virus;
    Date vaccDate;
    State* state;
    Record* record;
    int k =16;
    int recordAccepted = 1;

    // Read from file
    inputFile = fopen(filePath, "r");
    if(inputFile == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", filePath);
        exit(1);
    }
    // Create structs
    while (getline(&text, &textSize, inputFile) != -1) {
        // Record to be inserted in structs, unless faulty
        recordAccepted = 1;
        // Get citizenID
        token = strtok(text, " ");
        citizenID = malloc(strlen(token)+1);
        strcpy(citizenID, token);

        // Get firstName
        token = strtok(NULL, " ");
        fName = malloc(strlen(token)+1);
        strcpy(fName, token);

        // Get lastName
        token = strtok(NULL, " ");
        lName = malloc(strlen(token)+1);
        strcpy(lName, token);

        // Get country
        token = strtok(NULL, " ");
        country = malloc(strlen(token)+1);
        strcpy(country, token);

        // Get age
        token = strtok(NULL, " ");
        sscanf(token, "%d", &age);

        // Get virus
        token = strtok(NULL, " ");
        virus = malloc(strlen(token)+1);
        strcpy(virus, token);

        // Check if YES/NO
        token = strtok(NULL, " ");

        // Get vaccine date, if YES
        if(!strcmp("YES", token)){
            token = strtok(NULL, " ");
            sscanf(token, "%d-%d-%d", &vaccDate.day, &vaccDate.month, &vaccDate.year);
        }
        else {
            // If NO, then record should have no date
            if ( (token = strtok(NULL, " ")) ) {
                printf("ERROR IN RECORD %s %s %s %s %d %s - Has 'NO' with date %s", citizenID, fName, lName, country, age, virus, token);
                recordAccepted = 0;
            }
            else {
                vaccDate.day=0;
                vaccDate.month=0;
                vaccDate.year=0;
            }
        }
        // Insert records into structures
        if (recordAccepted) {
            // Add country in State linked list
            // Add each country once only
            if (!stateExists(stateHead, country)) {
                state = insertState(&stateHead, country);
            }
            else {
                state = stateExists(stateHead, country);
            }

            // Add record in Record linked list
            // Check if record is unique
            int check = checkDuplicate(recordsHead, citizenID, fName, lName, state, age, virus);
            // Record is unique
            if (!check) {
                record = insertSortedRecord(&recordsHead, citizenID, fName, lName, state, age, virus);
                recordAccepted = 1;
            }
            // Record exists for different virus
            else if (check == 2) {
                record = insertVirusOnly(&recordsHead, citizenID, virus);
                recordAccepted = 1;
            }
            // Record exists for the same virus
            else if (check == 1) {
                recordAccepted = 0;
            }
        }

        if (recordAccepted) {
            // Add vaccinated record in Bloom Filter
            if (vaccDate.year != 0) {
                // Check if we have Bloom Filter for this virus
                if (virusBloomExists(bloomsHead, virus)) {
                    insertInBloom(bloomsHead, citizenID, virus);
                }
                // Create new Bloom Filter for this virus
                else {
                    bloomsHead = createBloom(bloomsHead, virus, bloomSize, k);
                    insertInBloom(bloomsHead, citizenID, virus);
                }
            }      

            // Add record in Skip List
            // Separate structure for vaccined Skip Lists
            if (vaccDate.year != 0) {
                if (virusSkipExists(skipVaccHead, virus)) {
                    insertInSkip(skipVaccHead, record, virus, vaccDate);
                }
                else {
                    skipVaccHead = createList(skipVaccHead, virus);
                    insertInSkip(skipVaccHead, record, virus, vaccDate);
                }
            }
            // Separate structure for non-vaccined Skip Lists
            else {
                if (virusSkipExists(skipNonVaccHead, virus)) {
                    insertInSkip(skipNonVaccHead, record, virus, vaccDate);
                }
                else {
                    skipNonVaccHead = createList(skipNonVaccHead, virus);
                    insertInSkip(skipNonVaccHead, record, virus, vaccDate);
                }
                
            }
        }
        free(citizenID);
        free(fName);
        free(lName);
        free(country);
        free(virus);
    }
    fclose(inputFile);
    free(filePath);
    return;
}