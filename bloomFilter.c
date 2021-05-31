#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "functions.h"

// Hash aux function djb2
unsigned long djb2 (unsigned char *str) {
    unsigned long hash = 5381;
    int c; 
    while ( (c = *str++) ) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// Hash aux function sdbm
unsigned long sdbm (unsigned char *str) {
    unsigned long hash = 0;
    int c;
    while ( (c = *str++) ) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// Hash function hash_i
unsigned long hash_i (unsigned char *str, unsigned int i) {
    return djb2(str) + i*sdbm(str) + i*i;
}

// Create new Bloom Filter for new virus
BloomFilter* createBloom (BloomFilter* bloomsHead, char* virus, int size, int k) {
    BloomFilter* newBloom;
    newBloom = malloc(sizeof(BloomFilter));
    newBloom->size = size;
    newBloom->k = k;
    newBloom->virus = malloc(strlen(virus)+1);
    strcpy(newBloom->virus, virus); 
    newBloom->bitArray = calloc(size, 1); // malloc size elements of 1 byte each, initialize them to 0
    
    // Put new Bloom in head
    if (bloomsHead) {
        newBloom->next = bloomsHead;
    }
    else {
        newBloom->next = NULL;
    }
    return newBloom;
}

// Insert new element in Bloom Filter
void insertInBloom (BloomFilter* bloomsHead, char* citizenID, char* virus) {
    BloomFilter* current = bloomsHead;
    unsigned char* id = (unsigned char*)citizenID;
    unsigned long hash;
    unsigned int set = 1; // All 0s but rightmost bit=1
    int x;
    // Search for this virus' BF
    while (current) {
        if (!strcmp(current->virus, virus)) {
            // Hash citizenID
            for (unsigned int i=0; i<(current->k-1); i++) {
                // Hash the ID
                hash = hash_i(id, i);
                // Get the equivalent bit position that needs to be set to 1
                x = hash%(current->size*8);
                // An int is 32 bits, x/32 is the index in the bitArray, x%32 is the bit's position inside that index
                
                // Shift left the *set* bit
                set = set << (x%32);
                current->bitArray[x/32] = current->bitArray[x/32] | set;
                set = 1;
            } 
            return;
        }
        current = current->next;
    }
}

// Check if Bloom Filter for this virus exists
int virusBloomExists (BloomFilter* bloomsHead, char* virus) {
    // No Blooms Filter yet
    if (!bloomsHead) {
        return 0;
    }

    BloomFilter* current = bloomsHead;
    while (current) {
        if (!strcmp(current->virus, virus)) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Print list of Bloom Filters
void printBloomsList (BloomFilter* head) {
    BloomFilter* current = head;
    while (current) {
        printf("BF : %s\n", current->virus);
        current = current->next;
    }
}

// Free memory allocated for Bloom Filters
void freeBlooms (BloomFilter* head) {
    BloomFilter* current = head;
    BloomFilter* tmp;

    while (current) {
        tmp = current;
        current = current->next;
        free(tmp->bitArray);
        free(tmp->virus);
        free(tmp);
    }
}

// Get Bloom Filter for given virus
BloomFilter* insertBloomInParent (BloomFilter** bloomsHead, char* virus, int size, int k) {
    // Check if Bloom Filter for this virus exists
    BloomFilter* current = *bloomsHead;
    while (current) {
        if (!strcmp(current->virus, virus)) {
            // Return pointer to that virus' Bloom Filter
            return current;
        }
        current = current->next;
    }
    return NULL;
}