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