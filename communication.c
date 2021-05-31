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

// Read message
int getMessage (Message* incMessage, int incfd, int bufSize) {
    // 1st get the code
    incMessage->code = calloc(1, sizeof(char));
    if ( (incMessage->code = readMessage(incMessage->code, 1, incfd, bufSize)) == NULL) {
        return -1;
    }

    // 2nd get the header, containing the length of the actual message
    char* header = calloc((LENGTH + 1), sizeof(char));
    header = readMessage(header, LENGTH, incfd, bufSize);
    header[strlen(header)] = '\0';
    incMessage->length = atoi(header+1);

    // 3rd get the actual message
    incMessage->body = calloc( (incMessage->length+1), sizeof(char));
    incMessage->body = readMessage(incMessage->body, incMessage->length, incfd, bufSize);
    incMessage->body[incMessage->length] = '\0';

    free(header);
    return 1;
}

// Read message aux function
char* readMessage(char* msg, int length, int fd, int bufSize) {
    // Initialize buffer
    char* buf = calloc(bufSize, sizeof(char));

    // Get the message
    int receivedSoFar = 0;
    while (receivedSoFar < length) {
        int received;
        int left = length - receivedSoFar;
        // Bytes left unread < buf size => read them all
        if (left < bufSize) {
            if ( (received = read(fd, buf, left)) == -1 ) {
                perror("Didn't read message");
                return(NULL);
            }
        }
        // Bytes left unread > buf size => read buf size only
        else {
            if ( (received = read(fd, buf, bufSize)) == -1 ) {
                perror("Didn't read message");
                return(NULL);
            }
        }
        strncpy(msg + receivedSoFar, buf, received);
        // Keep receiving until whole message received
        receivedSoFar += received;
    }
    free(buf);
    return msg;
}

// Send message
void sendMessage (char code, char* body, int fd, int bufSize) {
    // Initialize buffer
    char* buf = calloc(bufSize, sizeof(char));

    // Message: [charCode] + [length of message] + [message] + [\0]
    char* msg = malloc( sizeof(char)*(1 + LENGTH + strlen(body) + 1) );
    // First char: code
    msg[0] = code;
    // Next LENGTH chars: message length
    snprintf(msg+1, LENGTH+1, "%0*d", LENGTH, (int)strlen(body));
    // Next strlen(message) chars: message
    snprintf(msg+LENGTH+1, strlen(body)+1, "%s", body);
    // Last char: '\0'
    msg[LENGTH+strlen(body)+1] = '\0';

    // Send the message
    int sentSoFar = 0;
    while (sentSoFar < strlen(msg)) {
        int sent;
        int left = strlen(msg) - sentSoFar;
        // Bytes left for sending fit in buf => send them all
        if (left < bufSize) {
            strncpy(buf, msg + sentSoFar, left);
            if ( (sent = write(fd, buf, left)) == -1) {
                perror("Error with writing message");
                exit(1);
            }
        }
        // Bytes left don't fit in buf => send only bufSize bytes
        else {
            strncpy(buf, msg + sentSoFar, bufSize);
            if ( (sent = write(fd, buf, bufSize)) == -1) {
                perror("Error with writing message");
                exit(1);
            }
        }
        // Keep sending until whole message sent
        sentSoFar += sent;
    }
    free(buf);
    free(msg);
    return;
}
