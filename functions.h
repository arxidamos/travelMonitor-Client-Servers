#ifndef __FUNCTIONS__
#define __FUNCTIONS__ 
#include "structs.h"

// stateList.c
State* insertState (State** head, char* country);
State* stateExists (State* head, char* country);
void printStateList (State* state);
void freeStateList (State* head);

// recordList.c
Record* createRecord (char* citizenID, char* fName, char* lName, char* country, int age, char* virus, Date vaccDate);
Record* insertSortedRecord (Record** head, char* citizenID, char* fName, char* lName, State* state, int age, char* virus);
Record* insertVirusOnly (Record** head, char* citizenID, char* virus);
int checkDuplicate (Record* head, char* citizenID, char* fName, char* lName, State* state, int age, char* virus);
int checkExistence (Record* head, char* citizenID);
void printRecordsList (Record* record);
void printSingleRecord (Record* record, char* citizenID);
void freeRecordList (Record* head);

// bloomFilter.c
unsigned long djb2 (unsigned char *str);
unsigned long sdbm (unsigned char *str);
unsigned long hash_i (unsigned char *str, unsigned int i);
BloomFilter* createBloom (BloomFilter* bloomsHead, char* virus, int size, int k);
void insertInBloom (BloomFilter* bloomsHead, char* citizenID, char* virus);
int virusBloomExists (BloomFilter* bloomsHead, char* virus);
void printBloomsList (BloomFilter* head);
void freeBlooms (BloomFilter* head);
BloomFilter* insertBloomInParent (BloomFilter** bloomsHead, char* virus, int size, int k);

// skipList.c
SkipList* createList (SkipList* skipListHead, char* virus);
void insertInSkip (SkipList* skipListHead, Record* record, char* virus, Date vaccDate);
SkipNode* searchSkipList (SkipList* skipListHead, char* citizenID);
int searchCountrySkipList (SkipList* skipListHead, char* country, Date date1, Date date2);
int* searchCountryByAge (SkipList* skipListHead, char* country, Date date1, Date date2);
SkipList* virusSkipExists (SkipList* skipListHead, char* virus);
void removeFromSkip (SkipList* skipListHead, SkipNode* node);
int getHeight (int maximum);
void printSkipLists (SkipList* head);
void printSkipNodes (SkipList* skipList);
void freeSkipLists (SkipList* head);
void freeSkipNodes (SkipList* skipList);

// mainFunctions.c
// Command functions
void travelRequest (Stats* stats, int* readyMonitors, BloomFilter* head, ChildMonitor* childMonitor, int numMonitors, int* sockfd, int bufSize, int* accepted, int* rejected, char* citizenID, char* countryFrom, char* countryTo, char* virus, Date date);
void travelStats (Stats stats, char* virus, Date date1, Date date2, char* country);
char* processTravelRequest (SkipList* head, char* citizenID, char* virus, Date date);
void searchVaccinationStatus (SkipList* vaccHead, SkipList* nonVaccHead, char* citizenID);
void vaccineStatusBloom (BloomFilter* head, char* citizenID, char* virus);
void vaccineStatus (SkipList* head, char* citizenID, char* virus);
void vaccineStatusAll (SkipList* head, char* citizenID);
void populationStatus (SkipList* skipVaccHead, SkipList* skipNonVaccHead, char* country, State* stateHead, Date date1, Date date2);
void popStatusByAge (SkipList* skipVaccHead, SkipList* skipNonVaccHead, char* country, State* stateHead, Date date1, Date date2);
int insertCitizenCheck (Record* head, char* citizenID, char* fName, char* lName, char* country, int age, char* virus);
// Auxiliary functions
int compareDate (Date a, Date b);
int isBetweenDates (Date a, Date x, Date b);
Date getTime ();
int compareSixMonths (Date a, Date b);

// communication.c
int getMessage (Message* incMessage, int incfd, int bufSize);
char* readMessage(char* msg, int length, int fd, int bufSize);
void sendMessage (char code, char* body, int fd, int bufSize);

// childAux.c
void readDirs (MonitorDir** monitorDir, char** dirPath, int dirPathsNumber);
void analyseMessage (MonitorDir** monitorDir, Message* message, int sockfd, int outfd, int* bufSize, int* bloomSize, BloomFilter** bloomsHead, State** stateHead, Record** recordsHead, SkipList** skipVaccHead, SkipList** skipNonVaccHead, int* accepted, int* rejected, int numThreads, pthread_t* threads);
void processAddCommand(MonitorDir** monitorDir, int sockfd, int bufSize, char* country, int numThreads);
void updateParentBlooms(BloomFilter* bloomsHead, int outfd, int bufSize);
int compare (const void * a, const void * b);
void createLogFileChild (MonitorDir** monitorDir, int* accepted, int* rejected);

// parentAux.c
void mapCountryDirs (char* dir_path, int numMonitors, ChildMonitor childMonitor[]);
void insertExecvArgs(char* argsArray[], int port, char* numThreadsString, char* socketBufferSizeString, char* cyclicBufferSizeString, char* bloomSizeString, char* dir_path, ChildMonitor* childMonitor, int index, int length);
void analyseChildMessage(int* sockfd, Message* message, ChildMonitor* childMonitor, int numMonitors, int *readyMonitors, int bufSize, BloomFilter** bloomsHead, int bloomSize, int* accepted, int* rejected, Stats* stats);
void resendCountryDirs (char* dir_path, int numMonitors, int outfd, ChildMonitor childMonitor, int bufSize);
int getUserCommand(Stats* stats, int* readyMonitors, int numMonitors, ChildMonitor* childMonitor, BloomFilter* bloomsHead, char* dir_path, DIR* input_dir, int* sockfd, int bufSize, int bloomSize, int* accepted, int* rejected);
void createLogFileParent (int numMonitors, ChildMonitor* childMonitor, int* accepted, int* rejected);
void updateBitArray (BloomFilter* bloomFilter, char* bitArray);
void exitApp(Stats* stats, DIR* input_dir, char* dir_path, int bufSize, int bloomSize, int* readyMonitors, int numMonitors, int* sockfd, ChildMonitor* childMonitor, int* accepted, int* rejected, BloomFilter* bloomsHead);

// stats.c
void initStats(Stats* stats);
void addToStats(Stats* stats, char* virus, char* countryTo, Date date);
void informStats (Stats* stats, int hitMiss);
void freeStats(Stats* stats);

// monitorDirList.c
MonitorDir* insertDir (MonitorDir** head, DIR* dir, char* country, char* files[], int fileCount);
int fileInDir (MonitorDir* monitorDir, char* newFile);
void insertFile(MonitorDir** monitorDir, char* newFile);
void printMonitorDirList (MonitorDir* monitorDir);
void freeMonitorDirList (MonitorDir* head);

// cyclicBuffer.c
void initCyclicBuffer (CyclicBuffer* cBuf, int cyclicBufferSize);
void insertToCyclicBuffer (CyclicBuffer* cBuf, char* path);
char* extractFromCyclicBuffer (CyclicBuffer* cBuf, char** path);
void freeCyclicBuffer (CyclicBuffer* cBuf);

// threadAux.c
void sendPathsToThreads(MonitorDir* monitorDir, int numThreads);
void sendNewPathToThreads(char* file, int numThreads);
void* threadConsumer (void* ptr);
void processFile (char* filePath);

// Extern common vars and structures
extern CyclicBuffer cBuf;
extern pthread_mutex_t mtx;
extern pthread_cond_t condNonEmpty;
extern pthread_cond_t condNonFull;
extern BloomFilter* bloomsHead;
extern State* stateHead;
extern Record* recordsHead;
extern SkipList* skipVaccHead;
extern SkipList* skipNonVaccHead;
extern int bloomSize;

#endif