#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "structs.h"
#include "functions.h"

// Check if citizenID is vaccinated
void travelRequest (Stats* stats, int* readyMonitors, BloomFilter* head, ChildMonitor* childMonitor, int numMonitors, int* incfd, int* outfd, int bufSize, int* accepted, int* rejected, char* citizenID, char* countryFrom, char* countryTo, char* virus, Date date) {
    BloomFilter* current = head;
    unsigned char* id = (unsigned char*)citizenID;
    unsigned long hash;
    unsigned int set = 1; // All 0s and leftmost bit=1
    int x;

    // Add data to structure
    addToStats(stats, virus, countryTo, date);

    while (current) {
        // printf("Checking BLoom Filter %s\n", current->virus);
        if (!strcmp(current->virus, virus)) {
            // Hash the id to get the bits we need to check
            for (unsigned int i=0; i<(current->k-1); i++) {
                // Hash the ID
                hash = hash_i(id, i);
                // Get the equivalent bit position that may be 1
                x = hash%(current->size*8);
                // Shift left the *set* bit
                set = set << (x%32);

                // Check if bitArray has *1* in the same spot
                if ((current->bitArray[x/32] & set) == 0) {
                    printf("REQUEST REJECTED - YOU ARE NOT VACCINATED\n");
                    (*rejected)++;

                    // Add hit or miss to structure
                    informStats(stats, MISS);

                    // Send increment counter message
                    for (int x=0; x<numMonitors; x++) {
                        for (int y=0; y<childMonitor[x].countryCount; y++) {
                            if ( !strcmp(childMonitor[x].country[y], countryTo) ) {
                                sendMessage('+', "NO", outfd[x], bufSize);
                                return;
                            }
                        }
                    }
                }
                set = 1;
            }

            // It's a "MAYBE" => Ask the Monitor in charge of countryFrom
            for (int i=0; i<numMonitors; i++) {
                for (int j=0; j<childMonitor[i].countryCount; j++) {
                    if ( !strcmp(childMonitor[i].country[j], countryFrom) ) {
                        (*readyMonitors)--;
                        char dateString[10];
                        sprintf(dateString, "%d-%d-%d", date.day, date.month, date.year);
                        char* fullString = malloc((strlen(citizenID) + 1 + strlen(virus) + 1 + strlen(countryTo) + 1 + strlen(dateString) + 1)*sizeof(char));
                        strcpy(fullString, citizenID);
                        strcat(fullString, ";");
                        strcat(fullString, virus);
                        strcat(fullString, ";");
                        strcat(fullString, countryTo);
                        strcat(fullString, ";");
                        strcat(fullString, dateString);

                        // Send citizenID, date to Monitor
                        sendMessage('t', fullString, outfd[i], bufSize);
                        free(fullString);
                        break;
                    }
                }
            }
            return;
        }
        current = current->next;
    }
    printf("There is no Bloom Filter for the virus name you inserted.\n");
    return;
}

// Print request stats
void travelStats (Stats stats, char* virus, Date date1, Date date2, char* country) {
    int total = 0;
    int accepted = 0;
    int rejected = 0;
    // Country parameter passed
    if (country) {
        for (int i=0; i<stats.count; i++) {
            // Get requests for virus, country, between date1-date2
            if ( !strcmp(stats.virus[i], virus) && !strcmp(stats.country[i], country) && isBetweenDates(date1, stats.date[i], date2) ) {
                if (stats.hitAndMiss[i] == HIT) {
                    accepted++;
                }
                else if (stats.hitAndMiss[i] == MISS) {
                    rejected++;
                }
                total++;
            }
        }
    }
    // No country parameter passed
    else {
        for (int i=0; i<stats.count; i++) {
            // Get requests for virus, between date1-date2
            if ( !strcmp(stats.virus[i], virus) && isBetweenDates(date1, stats.date[i], date2) ) {
                if (stats.hitAndMiss[i] == HIT) {
                    accepted++;
                }
                else if (stats.hitAndMiss[i] == MISS) {
                    rejected++;
                }
                total++;
            }
        }
    }
    printf("TOTAL REQUESTS %d\nACCEPTED %d\nREJECTED %d\n", total, accepted, rejected);
}

// Check if cizitenID is vaccinated 6 months prior to date
char* processTravelRequest (SkipList* head, char* citizenID, char* virus, Date date) {
    // Get this virus' SkipList 
    head = virusSkipExists(head, virus);
    // Check if this citizenID exists
    SkipNode* node = searchSkipList(head, citizenID);
    if (node) {
        // Check if vacc date is prior to 6 months
        if (compareSixMonths(node->vaccDate, date) == 0) {
            return ("BUT");
        }
        else if (compareSixMonths(node->vaccDate, date) == 1) {
            return ("YES");
        }
    }
    return ("NO");
}

// Search citizenID in all childs 
void searchVaccinationStatus (SkipList* head, char* citizenID) {
    SkipList* current = head;
    SkipNode* node = NULL;
    int done = 0;
    // Iterate through Skip Lists
    while (current) {
        node = searchSkipList(current, citizenID);
        if(node){
            // Print 1st & 2nd lines only once
            if (!done) {
                // 1st line
                printf("%s %s %s %s\n", node->citizenID, node->record->firstName, node->record->lastName, node->record->country->name);
                // 2nd line
                printf("AGE %d\n", node->record->age);
                done = 1;
            }
            // Next lines
            printf("%s ", current->virus);
            if (node->vaccDate.year != 0) {
                printf("VACCINATED ON %d-%d-%d\n", node->vaccDate.day, node->vaccDate.month, node->vaccDate.year);
            }
            else {
                printf("NOT YET VACCINATED\n");
            }
        }
        current = current->next;
    }
    return;
}

// Check if citizenID belongs to virus' Bloom Filter
void vaccineStatusBloom (BloomFilter* head, char* citizenID, char* virus) {
    BloomFilter* current = head;
    unsigned char* id = (unsigned char*)citizenID;
    unsigned long hash;
    unsigned int set = 1; // All 0s and leftmost bit=1
    int x;

    while (current) {
        // printf("Checking BLoom Filter %s\n", current->virus);
        if (!strcmp(current->virus, virus)) {
            // Hash the id to get the bits we need to check
            for (unsigned int i=0; i<(current->k-1); i++) {
                // Hash the ID
                hash = hash_i(id, i);
                // Get the equivalent bit position that may be 1
                x = hash%(current->size*8);
                // Shift left the *set* bit
                set = set << (x%32);

                // Check if bitArray has *1* in the same spot
                if ((current->bitArray[x/32] & set) == 0) {
                    printf("NOT VACCINATED\n");
                    return;
                }
                set = 1;
            }
            printf("MAYBE\n");
            return;
        }
        current = current->next;
    }
    printf("There is no Bloom Filter for the virus name you inserted.\n");
    return;
}

// Check if citizenID belongs to virus' Skip List 
void vaccineStatus (SkipList* head, char* citizenID, char* virus) {
    head = virusSkipExists(head, virus);
    SkipNode* node = searchSkipList(head, citizenID);
    if (node) {
        printf("VACCINATED ON %d-%d-%d\n", node->vaccDate.day, node->vaccDate.month, node->vaccDate.year);
    }
    else {
        printf("NOT VACCINATED\n");
        return;
    }
}

// Display all vacc and non-vacc occurences for citizenID
void vaccineStatusAll (SkipList* head, char* citizenID) {
    SkipList* current = head;
    SkipNode* node = NULL;

    // Iterate through Skip Lists
    while (current) {
        node = searchSkipList(current, citizenID);
        if(node){
            // Print virus
            printf("%s ", current->virus);
            // Print YES dd-mm-yyyy
            if (node->vaccDate.year != 0) {
                printf("YES %d-%d-%d\n", node->vaccDate.day, node->vaccDate.month, node->vaccDate.year);
            }
            // Print NO
            else {
                printf("NO\n");
            }
        }
        current = current->next;
    }
    return;
}

// Display country's vaccined total and percentage of citizens for virus
void populationStatus (SkipList* skipVaccHead, SkipList* skipNonVaccHead, char* country, State* stateHead, Date date1, Date date2) { 
    int vaccined = 0;
    int vaccTotal = 0;
    int nonVaccTotal = 0;
    float percentage = 0;
    Date dateZero;
    Date dateInf;
    dateZero.day = 0;
    dateZero.month = 0;
    dateZero.year = 0;
    dateInf.day = 99;
    dateInf.month = 99;
    dateInf.year = 9999;
    
    // Country optional argument NOT passed
    if (!country) {
        State* currentState = stateHead;
        while (currentState) {
            country = currentState->name;
            vaccined = searchCountrySkipList(skipVaccHead, country, date1, date2);
            vaccTotal = searchCountrySkipList(skipVaccHead, country, dateZero, dateInf);
            nonVaccTotal = searchCountrySkipList(skipNonVaccHead, country, dateZero, dateInf);
            // Check if denominator is zero
            if (vaccTotal == 0 && nonVaccTotal == 0) {
                percentage = 0;
            }
            else {
                percentage = ( (float)vaccined / (float)(vaccTotal + nonVaccTotal) ) * 100;
            }
            printf("%s %d %.2f%%\n", country, vaccined, percentage);
            currentState = currentState->next;
        }
    }
    // Country argument passed
    else {
        vaccined = searchCountrySkipList(skipVaccHead, country, date1, date2);
        vaccTotal = searchCountrySkipList(skipVaccHead, country, dateZero, dateInf);
        nonVaccTotal = searchCountrySkipList(skipNonVaccHead, country, dateZero, dateInf);
        // Check if denominator is zero
        if (vaccTotal == 0 && nonVaccTotal == 0) {
            percentage = 0;
        }
        else {
            percentage = ( (float)vaccined / (float)(vaccTotal + nonVaccTotal) ) * 100;
        }
        printf("%s %d %.2f%%\n", country, vaccined, percentage);
    }
    return;
}

// Display country's vaccined total and percentage of citizens for virus, per age category
void popStatusByAge (SkipList* skipVaccHead, SkipList* skipNonVaccHead, char* country, State* stateHead, Date date1, Date date2) {
    int* vaccined;
    int* vaccTotal;
    int* nonVaccTotal;
    float* percentage;
    
    Date dateZero;
    Date dateInf;
    dateZero.day = 0;
    dateZero.month = 0;
    dateZero.year = 0;
    dateInf.day = 99;
    dateInf.month = 99;
    dateInf.year = 9999;
    
    // Country optional argument NOT passed
    if (!country) {
        State* currentState = stateHead;
        while (currentState) {
            country = currentState->name;
            vaccined = searchCountryByAge(skipVaccHead, country, date1, date2);
            vaccTotal = searchCountryByAge(skipVaccHead, country, dateZero, dateInf);
            nonVaccTotal = searchCountryByAge(skipNonVaccHead, country, dateZero, dateInf);
            percentage = malloc(4*sizeof(float));

            printf("%s\n", country);
            // Check if denominator is zero
            for (int i=0; i<4; i++) {
                if (vaccTotal[i] == 0 && nonVaccTotal[i] == 0) {
                    percentage[i] = 0.f;
                }
                else {
                    percentage[i] = ( (float)vaccined[i] / (float)(vaccTotal[i] + nonVaccTotal[i]) ) * 100;
                }
                if (i==0) {
                    printf("0-20  ");
                }
                else if (i==1) {
                    printf("20-40 ");
                }
                else if (i==2) {
                    printf("40-60 ");
                }
                else if (i==3) {
                    printf("60+   ");
                }
                printf("%d %.2f%%\n", vaccined[i], percentage[i]);
            }
            printf("\n");
            free(vaccined);
            free(vaccTotal);
            free(nonVaccTotal);
            free(percentage);
            currentState = currentState->next;
        }
    }
    // Country argument passed
    else {
        vaccined = searchCountryByAge(skipVaccHead, country, date1, date2);
        vaccTotal = searchCountryByAge(skipVaccHead, country, dateZero, dateInf);
        nonVaccTotal = searchCountryByAge(skipNonVaccHead, country, dateZero, dateInf);
        percentage = malloc(4*sizeof(float));

        // Check if denominator is zero
        printf("%s\n", country);
        // Check if denominator is zero
        for (int i=0; i<4; i++) {
            if (vaccTotal[i] == 0 && nonVaccTotal[i] == 0) {
                percentage[i] = 0;
            }
            else {
                percentage[i] = ( (float)vaccined[i] / (float)(vaccTotal[i] + nonVaccTotal[i]) ) * 100;
            }
            if (i==0) {
                printf("0-20  ");
            }
            else if (i==1) {
                printf("20-40 ");
            }
            else if (i==2) {
                printf("40-60 ");
            }
            else if (i==3) {
                printf("60+   ");
            }
            printf("%d %.2f%%\n", vaccined[i], percentage[i]);
        }
        printf("\n");

        free(vaccined);
        free(vaccTotal);
        free(nonVaccTotal);
        free(percentage);
    }
    return;
}

// Validate citizenID for Insert command
int insertCitizenCheck (Record* head, char* citizenID, char* fName, char* lName, char* country, int age, char* virus) {
    // First element to be added, no possible duplicates
    if (!head) {
        return 0;
    }

    while (head) {
        // Check if same citizenID exists
        if (!strcmp(head->citizenID, citizenID)) {
            // Check if name, country, age are the same
            if (!strcmp(head->firstName, fName) && !strcmp(head->lastName, lName) && !strcmp(head->country->name, country) && (head->age == age)) {
                // Check if one of the record's viruses is the same
                for (int i=0; i<head->virusCount; i++){
                    // If virus is the same, no need to add it to record's viruses
                    if (!strcmp(head->virus[i], virus)) {
                        return 0;
                    }
                }
                // If new virus, add it to record's virus list
                return 2;
            }
            // CitizenID same, rest is different
            else {
                printf("ERROR IN RECORD %s %s %s %s %d %s \n", citizenID, fName, lName, country, age, virus);
                return 1;
            }
        }
        head = head->next;
    }
    return 0;
}

// Compare dates - Returns : "1", a older than b [a < b] | "0", a newer than b [a > b] | "-1": [a = b]
int compareDate (Date a, Date b) {
    if (a.year != b.year)
        return a.year < b.year;
    else if (a.month != b.month)
        return a.month < b.month;
    else if (a.day != b.day)
        return a.day < b.day;
    else {
        // Dates are the same
        return -1;
    }
}

// Find if Date x lies between Dates a, b
int isBetweenDates (Date a, Date x, Date b) {
    if ( (compareDate(a, x) == 1 || compareDate(a, x) == -1) && (compareDate(x, b) == 1 || compareDate(x, b) == -1) ) {
        return 1;
    }
    else {
        return 0;
    }
}

// Return current system date
Date getTime () {
    Date currentDate;
    time_t t;
    time(&t);
    struct tm *timeInfo = localtime(&t);
    char*buffer = malloc(strlen(ctime(&t) + 1));
    strftime(buffer, strlen(ctime(&t) + 1), "%e-%m-%Y", timeInfo); // %m: month %Y: year %e: day of month without leading zeros
    sscanf(buffer, "%d-%d-%d", &currentDate.day, &currentDate.month, &currentDate.year);
    
    free(buffer);
    return currentDate;
}

// Check if vacc date "a" is within 6 months of travel date "b"
int compareSixMonths (Date a, Date b) {
    if ( (b.year - a.year) > 1 || (b.year - a.year) < 0 ) {
        return 0;
    }
    else if ( (b.year - a.year) <= 1 ) {
        int compMonths = 0;
        // If not same year, add 12 months to 2nd for comparison
        if ( (b.year - a.year) == 1) {
            compMonths = (b.month+12) - a.month;
        }
        else if ( (b.month - a.month) == 0) {
            compMonths = b.month - a.month;
        }
        if (compMonths > 6) {
            return 0;
        }
        else if (compMonths == 6) {
            // Also check days
            if (b.day > a.day) {
                return 0;
            }
            else {
                return 1;
            }
        }
        else if (compMonths < 6) {
            return 1;
        }
    }
    return 0;
}