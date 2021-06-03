OBJS = main.o mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o stats.o communication.o parentAux.o
SOURCE = main.c mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c stats.c communication.c parentAux.c
CHILD_OBJS = childMain.o cyclicBuffer.o mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o communication.o monitorDirList.o stats.o childAux.o threadAux.o
CHILD_SOURCE = childMain.c cyclicBuffer.c mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c communication.c monitorDirList.c stats.c childAux.c threadAux.c
HEADER = structs.h functions.h
OUT = travelMonitorClient
CHILD_OUT = monitorServer
DIRS = ./log_files
CC = gcc
FLAGS = -g3 -Wall -pthread

all:$(OUT) $(CHILD_OUT)

$(OUT): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -o $(OUT)

$(CHILD_OUT): $(CHILD_OBJS)
	$(CC) $(FLAGS) $(CHILD_OBJS) -o $(CHILD_OUT)

clean:
	rm -f $(OBJS) $(OUT) $(CHILD_OBJS) $(CHILD_OUT)
	rm -rf $(DIRS)