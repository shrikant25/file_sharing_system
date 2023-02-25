drop table raw_data;
drop table send_data;
drop table  open_connections;
drop table  senders_comm;
drop table  for_receiver;
drop table  for_sender;


create table raw_data(rdid serial primary key, fd int NOT NULL, data bytea NOT NULL, data_size int NOT NULL);
create table send_data(sdid serial primary key, fd int NOT NULL, data bytea NOT NULL, data_size int NOT NULL);
create table open_connections_receiving(orid serial primary key, fd int NOT NULL, ipaddr int NOT NULL, status int NOT NULL);
create table open_connections_sending(osid serial primary key, fd int NOT NULL, ipaddr int NOT NULL, status int NOT NULL);
create table senders_comm(scid serial primary key, msgid int NOT NULL, status int NULL);
create table for_receiver(frid serial primary key, fd int NOT NULL, status int NOT NULL);
create table for_sender(fsid serial primary key, cid int NOT NULL, ipaddr int NOT NULL, status int NOT NULL);
