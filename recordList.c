#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "functions.h"

// Insert record to linked list in a sorted manner
Record* insertSortedRecord (Record** head, char* citizenID, char* fName, char* lName, State* state, int age, char* virus) {
    Record* newNode;
    Record* current;
    newNode = malloc(sizeof(Record));
    newNode->next = NULL;

    newNode->citizenID = malloc(strlen(citizenID)+1);
    strcpy(newNode->citizenID, citizenID);

    newNode->firstName = malloc(strlen(fName)+1);
    strcpy(newNode->firstName, fName);

    newNode->lastName = malloc(strlen(lName)+1);
    strcpy(newNode->lastName, lName);

    newNode->country = state;

    newNode->age = age;
    
    // Record may have more than one viruses
    newNode->virusCount = 1;
    newNode->virus = malloc(sizeof(char*)*(newNode->virusCount));
    newNode->virus[0] = malloc(strlen(virus)+1);
    strcpy(newNode->virus[0], virus);

    // Check if to insert on head
    if (!*head || (strcmp((*head)->citizenID, citizenID)) > 0) {
        newNode->next = *head;
        *head = newNode;
    }
    // Move across list
    else {
        current = *head;
        while ((current->next) && (strcmp(current->next->citizenID, citizenID) <= 0)) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
    return newNode;
}

// Insert additional virus to existing record
Record* insertVirusOnly (Record** head, char* citizenID, char* virus) {
    Record* current = *head;
    while (current) {
        if (!strcmp(current->citizenID, citizenID)) {
            current->virusCount++;
            current->virus = realloc(current->virus, sizeof(char*)*(current->virusCount));
            current->virus[current->virusCount-1] = malloc(strlen(virus)+1);
            strcpy(current->virus[current->virusCount-1], virus);
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Validate incoming records
int checkDuplicate (Record* head, char* citizenID, char* fName, char* lName, State* state, int age, char* virus) {
    // First element to be added, no possible duplicates
    if (!head) {
        return 0;
    }

    while (head) {
        // Check if same citizenID exists
        if (!strcmp(head->citizenID, citizenID)) {
            // Check if name, country, age are the same
            if (!strcmp(head->firstName, fName) && !strcmp(head->lastName, lName) && !strcmp(head->country->name, state->name) && (head->age == age)) {
                // Check if one of the record's viruses is the same
                for (int i=0; i<head->virusCount; i++){
                    // If virus is the same, reject incoming record
                    if (!strcmp(head->virus[i], virus)) {
                        printf("ERROR IN RECORD %s %s %s %s %d %s \n", citizenID, fName, lName, state->name, age, virus);
                        return 1;
                    }
                }
                // If new virus, add it to record's virus list
                return 2;
            }
            else {
                printf("ERROR IN RECORD %s %s %s %s %d %s \n", citizenID, fName, lName, state->name, age, virus);
                return 1;
            }
        }
        head = head->next;
    }
    return 0;
}

// Validate that citizendID record exists
int checkExistence (Record* head, char* citizenID) {
    // No possible existence
    if (!head) {
        return 0;
    }

    while (head) {
        // Check if citizenID found
        if (!strcmp(head->citizenID, citizenID)) {
            return 1;
        }
        head = head->next;
    }
    return 0;
}

// Print linked list of records
void printRecordsList (Record* record) {
    while (record) {

        printf("List record: %s %s %s %s %d ", record->citizenID, record->firstName, record->lastName, record->country->name, record->age); 
        for (int i=0; i<record->virusCount; i++) {
            printf("%s, ", record->virus[i]);
        }
        printf("\n");
        record = record->next;
    }
}

// Print single record
void printSingleRecord (Record* record, char* citizenID) {
    Record* current = record;
    while (current) {
        if (!strcmp(current->citizenID, citizenID)) {
            printf("CITIZENID %s %s %s %s %d ", current->citizenID, current->firstName, current->lastName, current->country->name, current->age);
            for (int i=0; i<current->virusCount; i++) {
                printf("%s ", current->virus[i]);
            }
            printf("\n");
            return;
        }
        current = current->next;
    }
}

// Free memory allocated for linked list of records
void freeRecordList (Record* head) {
    Record* current = head;
    Record* tmp;

    while (current) {
        tmp = current;
        current = current->next;
        free(tmp->citizenID);
        free(tmp->firstName);
        free(tmp->lastName);
        for (int i=0; i<tmp->virusCount; i++) {
            free(tmp->virus[i]);
        }
        free(tmp->virus);
        free(tmp);
    }
}