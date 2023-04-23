CC = gcc
FS_DIR = $(shell pwd)
LOCAL_INCLUDE_DIR = $(FS_DIR)/include
PSQL_INCLUDE_DIR = /usr/include/postgresql

CFLAGS = -I$(LOCAL_INCLUDE_DIR) -I$(PSQL_INCLUDE_DIR) -lpq -g -lpthread -std=c99 -mcmodel=large

processor_r_target = processor_r
processor_r_files := processor_r.c shared_memory.c partition.c 

processor_s_target = processor_s
processor_s_files := processor_s.c shared_memory.c partition.c 

sender_target = sender
sender_files := sender.c shared_memory.c partition.c 

receiver_target = receiver
receiver_files := receiver.c shared_memory.c partition.c 

processor_r: $(processor_r_files)
		$(CC) $(processor_r_files) -o $(processor_r_target) $(CFLAGS)

processor_s: $(processor_s_files)
		$(CC) $(processor_s_files) -o $(processor_s_target) $(CFLAGS) 

sender: $(sender_files)
		$(CC) $(sender_files) -o $(sender_target) $(CFLAGS)

receiver: $(receiver_files)
		$(CC) $(receiver_files) -o $(receiver_target) $(CFLAGS)