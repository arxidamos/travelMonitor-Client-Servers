OBJS = main.o parentAux.o # mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o stats.o communication.o parentSignals.o parentAux.o
SOURCE = main.c parentAux.c #  mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c stats.c communication.c parentSignals.c parentAux.c
CHILD_OBJS = childMain.o cyclicBuffer.o monitorDirList.o childAux.o
# mainFunctions.o stateList.o recordList.o bloomFilter.o skipList.o communication.o monitorDirList.o stats.o childSignals.o childAux.o
CHILD_SOURCE = childMain.c cyclicBuffer.c monitorDirList.c childAux.c
#  mainFunctions.c stateList.c recordList.c bloomFilter.c skipList.c communication.c monitorDirList.c stats.c childSignals.c childAux.c 
HEADER = structs.h functions.h
OUT = travelMonitorClient
CHILD_OUT = monitorServer
# DIRS = ./named_pipes ./log_files
CC = gcc
FLAGS = -g3 -c -Wall

all:$(OUT) $(CHILD_OUT)

$(OUT): $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT)

$(CHILD_OUT): $(CHILD_OBJS)
	$(CC) -g $(CHILD_OBJS) -o $(CHILD_OUT)

child.o: childMain.c
	$(CC) -pthread $(FLAGS) childMain.c

travelMonitorClient.o:main.c
	$(CC) -pthread $(FLAGS) main.c

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

# monitorDirList.o:monitorDirList.c
# 	$(CC) $(FLAGS) monitorDirList.c

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

# childAux.o:childAux.c
# 	$(CC) $(FLAGS) childAux.c

clean:
	rm -f $(OBJS) $(OUT) $(CHILD_OBJS) $(CHILD_OUT)
	rm -rf $(DIRS)