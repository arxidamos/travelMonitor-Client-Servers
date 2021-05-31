OBJS = main.o mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o stats.o communication.o parentAux.o
SOURCE = main.c mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c stats.c communication.c parentAux.c
CHILD_OBJS = childMain.o cyclicBuffer.o mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o communication.o monitorDirList.o stats.o childAux.o
CHILD_SOURCE = childMain.c cyclicBuffer.c mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c communication.c monitorDirList.c stats.c childAux.c 
HEADER = structs.h functions.h
OUT = travelMonitorClient
CHILD_OUT = monitorServer
DIRS = ./log_files
CC = gcc
FLAGS = -g3 -c -Wall

all:$(OUT) $(CHILD_OUT)

$(OUT): $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT)

$(CHILD_OUT): $(CHILD_OBJS)
	$(CC) -g -pthread $(CHILD_OBJS) -o $(CHILD_OUT)

childMain.o: childMain.c
	$(CC) $(FLAGS) childMain.c

main.o:main.c
	$(CC) $(FLAGS) main.c

# mainFunctions.o:mainFunctions.c
# 	$(CC) $(FLAGS) mainFunctions.c

# stateList.o:stateList.c
# 	$(CC) $(FLAGS) stateList.c	

# recordList.o:recordList.c
# 	$(CC) $(FLAGS) recordList.c

# bloomFilter.o:bloomFilter.c
# 	$(CC) $(FLAGS) bloomFilter.c

# skipList.o:skipList.c
# 	$(CC) $(FLAGS) skipList.c

monitorDirList.o:monitorDirList.c
	$(CC) $(FLAGS) monitorDirList.c

# stats.o:stats.c
# 	$(CC) $(FLAGS) stats.c	

# communication.o:communication.c
# 	$(CC) $(FLAGS) communication.c		

# parentSignals.o:parentSignals.c
# 	$(CC) $(FLAGS) parentSignals.c

# childSignals.o:childSignals.c
# 	$(CC) $(FLAGS) childSignals.c

parentAux.o:parentAux.c
	$(CC) $(FLAGS) parentAux.c

childAux.o:childAux.c
	$(CC) $(FLAGS) childAux.c

clean:
	rm -f $(OBJS) $(OUT) $(CHILD_OBJS) $(CHILD_OUT)
	rm -rf $(DIRS)