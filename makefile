CC = gcc

write_target = write
write_files := write.c shared_memory.c 

read_target = read
read_files := read.c shared_memory.c

destroy_target = destroy
destroy_files := destroy_shmem.c shared_memory.c 

read: $(read_files)
		$(CC) $(read_files) -o $(read_target) 

write: $(write_files)
		$(CC) $(write_files) -o $(write_target)   -I/usr/include/postgresql -lpq -std=c99

destroy: $(destroy_files)
		$(CC) $(destroy_files) -o $(destroy_target) 

