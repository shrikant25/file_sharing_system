drop table test2;
drop table test1;

create table test1 (msgid int primary key, parts_cnt int);
create table test2 (seq_num int, msgid int references test1(msgid) on update cascade);
create table test3 (msgid int primary key, msg text);
create table test4 (seq_num int, msgid int references test3(msgid) on update cascade, msg text);

create or replace function splitval()
returns trigger as
'
begin
insert into test2 select generate_series(1, NEW.parts_cnt),new.msgid;
RETURN NULL;
end;
'
language 'plpgsql';

create trigger splittrg after insert on test1 for each row execute function splitval();
create trigger splitmsgtrg after insert on test2 for each row execute function splitmsg();


create or replace function splitmsg()
returns trigger as
'
begin
insert into test4 select new.seq_num, new.msgid, substr();
RETURN NULL;
end;
'
language 'plpgsql';




insert into test1 values(1, 5);
insert into test1 values(2, 6);



**************************************************************************************************************************


insert into mystring values(1, pg_read_binary_file('/home/shrikant/f2.mp4'));


drop table substrings;
drop table mystring;
drop table subparts;
drop table merged_msg;

create table mystring(msgid int primary key, msg oid);
create table substrings (
   substrid serial, substring bytea,
   msgid int references mystring(msgid) on update cascade
);
create table subparts(msgid int, partsize int);
create table merged_msg(msgid int, msg oid);
create table merged_msg(msgid int, msg bytea);


create or replace function splitstr()
returns trigger as
'
declare
	subpart_size int;
	endval int;
begin

select partsize into subpart_size from subparts where subparts.msgid = new.msgid;

endval := CEIL(pg_column_size(lo_get(new.msg))/(subpart_size*1024*1024));


insert into substrings (substring, msgid) 
select lo_get(new.msg, (row_number - 1) * subpart_size + 1, subpart_size), new.msgid
from mystring, generate_series(1, endval) row_number
where msgid = new.msgid;


return new;
end;
'
language 'plpgsql';


create trigger t1 after update on mystring for each row execute function splitstr();

insert into subparts values(1, 7);
INSERT INTO mystring (msgid, msg)
VALUES (1, lo_import('/home/shrikant/f2.mp4'));
update mystring set msgid = 1 where msgid = 1;


insert into merged_msg (msgid, msg)
select m.msgid, lo_from_bytea(11, string_agg(s.substring, '')) as merged_string
from mystring m
join substrings s on m.msgid = s.msgid
where m.msgid = 1 
group by m.msgid;


insert into merged_msg (msgid, msg)
select m.msgid,string_agg(s.substring, '') as merged_string
from mystring m
join substrings s on m.msgid = s.msgid
where m.msgid = 1 
group by m.msgid;



insert into substrings (substring, msgid) 
select substring(select msg from mystring where msgid = 1, (row_number - 1) * subpart_size + 1, subpart_size), 1
from mystring, generate_series(1, 70) row_number
where msgid = 1;














intervalsize  start  end     total msgid
  5				1	  10      20	1
  5				11    20      20    1
_________________________________________________________  
  
  1.
  
	  insert into subparts
	  insert into strings
	  then trigger will be fired
	  some work will be done
	  subparts table will be updated
	  increment counter
	  check counter
	  if counter
			unset counter
			update m
			set msgid = msgid 
			from mystring m
			join subparts s
			on m.msgid = s.msgid 
			where s.endval != -1 
			and s.startval != -1;   
  2.

	  insert into subparts
	  insert into strings
      then trigger will be some work will be done
      subparts table will be updated
  
  
  
  
  
  
  
  
  
  
  
  
drop table substrings;
drop table mystring;
drop table subparts;
drop table countval;
drop table merged_msg;

create table mystring(msgid int primary key, msg oid);
create table substrings (
   substrid serial, substring bytea,
   msgid int references mystring(msgid) on update cascade
);
create table subparts(msgid int, intervalsize int, startv int, endv int, total int);
create table merged_msg(msgid int, msg oid);
create table countval(curcount int, maxcount int);
    
create or replace function splitstr()
returns trigger as
'
declare
	fintervalsize  int;
	fstartv int;
	fendv  int;  
	fcurcount int;
	fmaxcount int;
	ftotal int;
	flag int;
begin

select s.intervalsize, s.startv, s.endv, s.total 
into fintervalsize, fstartv, fendv, ftotal 
from subparts s 
where s.msgid = new.msgid;

raise notice'' msgid %'', new.msgid;
raise notice ''fstartv %'', fstartv;
raise notice ''fendv %'', fendv;
raise notice ''ftotal %'',ftotal;

flag := 0;


if fendv >= ftotal then
	update subparts set startv = -1, endv = -1 where msgid = new.msgid;
	update countval set maxcount = maxcount -1;
else
	insert into substrings (substring, msgid) 
	select lo_get(new.msg, (row_number - 1) * fintervalsize, fintervalsize), new.msgid
	from mystring, generate_series(fstartv, fendv) row_number
	where msgid = new.msgid;
	
	
    flag := 1;
	
	if (fendv + (fendv- fstartv)) > ftotal then
		update subparts set startv = fendv + 1, endv = ftotal where msgid = new.msgid;
	else 
		update subparts set startv = fendv + 1, endv = fendv + (fendv- fstartv) + 1 where msgid = new.msgid;
	end if;
end if;

select c.curcount, c.maxcount into fcurcount, fmaxcount from countval c;

fcurcount := fcurcount + flag;

raise notice ''fcurcount %'', fcurcount;
raise notice ''fmaxcount %'', fmaxcount;

if fcurcount >= fmaxcount then
	update countval set curcount = 0;
	update mystring 
			set msgid = msgid + 0 
			where msgid in(select msgid from subparts 
			where endv <> -1 
			and startv <> -1);   
else
	update countval set curcount = fcurcount;
end if;

return new;
end;
'
language 'plpgsql';

create trigger t1 after update on mystring for each row execute function splitstr();

insert into countval values(0, 9);

insert into subparts values(1, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (1, lo_import('/home/shrikant/f1.mkv'));
update mystring set msgid = 1 where msgid = 1;

insert into subparts values(2, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (2, lo_import('/home/shrikant/f2.mkv'));
update mystring set msgid = 2 where msgid = 2;

insert into subparts values(3, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (3, lo_import('/home/shrikant/f3.mkv'));
update mystring set msgid = 3 where msgid = 3;

insert into subparts values(4, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (4, lo_import('/home/shrikant/f4.mkv'));
update mystring set msgid = 4 where msgid = 4;

insert into subparts values(5, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (5, lo_import('/home/shrikant/f5.mkv'));
update mystring set msgid = 5 where msgid = 5;

insert into subparts values(6, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (6, lo_import('/home/shrikant/f6.mkv'));
update mystring set msgid = 6 where msgid = 6;

insert into subparts values(7, 1024*1024, 1, 10, 148);
insert into mystring (msgid, msg)
values (7, lo_import('/home/shrikant/f7.mkv'));
update mystring set msgid = 7 where msgid = 7;

insert into subparts values(8, 1024*1024, 1, 10, 87);
insert into mystring (msgid, msg)
values (8, lo_import('/home/shrikant/f8.mkv'));
update mystring set msgid = 8 where msgid = 8;

insert into subparts values(9, 1024*1024, 1, 10, 104);
insert into mystring (msgid, msg)
values (9, lo_import('/home/shrikant/f9.mkv'));
update mystring set msgid = 9 where msgid = 9;
