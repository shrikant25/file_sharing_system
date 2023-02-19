CC = gcc
FS_DIR = $(shell pwd)
LOCAL_INCLUDE_DIR = $(FS_DIR)/include
PSQL_INCLUDE_DIR = /usr/include/postgresql

CFLAGS = -I$(LOCAL_INCLUDE_DIR) -I$(PSQL_INCLUDE_DIR) -lpq -lpthread -std=c99 -g 

processor_target = processor
processor_files := processor.c processor_db.c shared_memory.c partition.c

sender_target = sender
sender_files := sender.c shared_memory.c partition.c

receiver_target = sender
receiver_files := receiver.c shared_memory.c partition.c 

processor: $(processor_files)
		$(CC) $(processor_files) -o $(processor_target) $(CFLAGS) 

sender: $(sender_files)
		$(CC) $(sender_files) -o $(sender_target) $(CFLAGS)

receiver: $(receiver_files)
		$(CC) $(receiver_files) -o $(receiver_target) $(CFLAGS)