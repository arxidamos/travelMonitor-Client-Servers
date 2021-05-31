#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"
#include "functions.h"

// Initialize Stats structure
void initStats(Stats* stats) {
    (*stats).count = 0;
    (*stats).hitAndMiss = malloc(sizeof(int)*(*stats).count);
    (*stats).virus = malloc(sizeof(char*)*(*stats).count);
    (*stats).country = malloc(sizeof(char*)*(*stats).count);
    (*stats).date = malloc(sizeof(Date)*(*stats).count);
}

// Store request's info
void addToStats(Stats* stats, char* virus, char* countryTo, Date date) {
    (*stats).count++;

    (*stats).virus = realloc((*stats).virus, (sizeof(char*)*(*stats).count));
    (*stats).virus[(*stats).count-1] = malloc(strlen(virus)+1);
    strcpy((*stats).virus[(*stats).count-1], virus);

    (*stats).country = realloc((*stats).country, (sizeof(char*)*(*stats).count));
    (*stats).country[(*stats).count-1] = malloc(strlen(countryTo)+1);
    strcpy((*stats).country[(*stats).count-1], countryTo);
    
    (*stats).date = realloc((*stats).date, (sizeof(Date)*(*stats).count));
    (*stats).date[(*stats).count-1].year = date.year;
    (*stats).date[(*stats).count-1].month = date.month;
    (*stats).date[(*stats).count-1].day = date.day;

    // Prepare counter for rest of the info
    (*stats).count--;
}

// Store if request's hit or missed
void informStats (Stats* stats, int hitMiss) {
    (*stats).count++;
    
    (*stats).hitAndMiss = realloc((*stats).hitAndMiss, (sizeof(int)*(*stats).count));
    // stats.hitAndMiss[stats.count-1] = malloc(sizeof(int));
    (*stats).hitAndMiss[(*stats).count-1] = hitMiss; // 0: request rejected, 1: accepted
}

// Deallocate memory
void freeStats(Stats* stats) {
    for (int i=0; i<(*stats).count; i++) {
        free((*stats).country[i]);
        free((*stats).virus[i]);
    }

    free((*stats).country);
    free((*stats).date);
    free((*stats).hitAndMiss);
    free((*stats).virus);
}