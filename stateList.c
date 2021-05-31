#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "functions.h"

// Insert country to States linked list
State* insertState (State** head, char* country) {
    State* newNode;
    State* current;

    newNode = malloc(sizeof(State));
    newNode->next = NULL;

    newNode->name = malloc(strlen(country)+1);
    strcpy(newNode->name, country);

    // Check if to insert on head
    if (!*head) {
        newNode->next = *head;
        *head = newNode;
    }
    // Move across list
    else {
        current = *head;
        while (current->next) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
    return newNode;
}

// Check if country exists in States linked list
State* stateExists (State* head, char* country) {
    State* current = head;
    
    if (!head) {
        return NULL;
    }

    while (current) {
        if (!strcmp(current->name, country)) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Print States linked list
void printStateList (State* state) {
    while (state) {
        printf("State: %s\n", state->name);
        state = state->next;
    }
    printf("Finished printing states\n");
}

// Free memory allocated for linked list 
void freeStateList (State* head) {
    State* current = head;
    State* tmp;

    while (current) {
        tmp = current;
        current = current->next;
        free(tmp->name);
        free(tmp);
    }
}