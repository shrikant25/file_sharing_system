CC = gcc
FS_DIR = $(shell pwd)
LOCAL_INCLUDE_DIR = $(FS_DIR)/include
PSQL_INCLUDE_DIR = /usr/include/postgresql

CFLAGS = -I$(LOCAL_INCLUDE_DIR) -I$(PSQL_INCLUDE_DIR) -lpq -pthread -std=c99 


write_target = write
write_files := write.c shared_memory.c 

read_target = read
read_files := read.c shared_memory.c

destroy_target = destroy
destroy_files := destroy_shmem.c shared_memory.c  


p1_target = p1
p1_files := p1.c partition.c shared_memory.c bitmap.c

p2_target = p2
p2_files := p2.c partition.c shared_memory.c bitmap.c


read: $(read_files)
		$(CC) $(read_files) -o $(read_target) 

write: $(write_files)
		$(CC) $(write_files) -o $(write_target)   -I/usr/include/postgresql -lpq -std=c99

destroy: $(destroy_files)
		$(CC) $(destroy_files) -o $(destroy_target) -I$(LOCAL_INCLUDE_DIR)

p1: $(p1_files)
		$(CC) $(p1_files) -o $(p1_target) $(CFLAGS)

p2: $(p2_files)
		$(CC) $(p2_files) -o $(p2_target) $(CFLAGS)

