CC = gcc
FS_DIR = $(shell pwd)
LOCAL_INCLUDE_DIR = $(FS_DIR)/include
PSQL_INCLUDE_DIR = /usr/include/postgresql

CFLAGS = -I$(LOCAL_INCLUDE_DIR) -I$(PSQL_INCLUDE_DIR) -lpq -lpthread -std=c99 -g 

program_target = program
program_files := processor.c processor_db.c sender.c receiver.c shared_memory.c partition.c

program: $(program_files)
		$(CC) $(program_files) -o $(program_target) $(CFLAGS) 

