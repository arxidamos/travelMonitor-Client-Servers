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


void readDirs (MonitorDir** monitorDir, char** dirPath, int dirPathsNumber) {
    DIR* dir;
    
    // Iterate over all directories
    for (int i=0; i<dirPathsNumber; i++) {

        printf("Iteration %d, trying %s\n", i, dirPath[i]);
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
                files[j] = malloc(strlen(dirPath[i]) + strlen(current->d_name) + 1);
                strcpy(files[j], dirPath[i]);
                // strcat(files[i], "/");
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

// Compare two files (aux function for qsort)
int compare (const void * a, const void * b) {
  return strcmp(*(char* const*)a, *(char* const*)b );
}