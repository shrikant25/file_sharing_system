
create table psubstrings (
   substrid serial, substring bytea,
   msgid int);
   
create table psubparts(msgid int, cur int, total int);
create table pmerged_msg(msgid int, msg oid);
    

create or replace function usub()
returns trigger as
'
begin
	update psubparts set cur = cur + 1 where msgid = new.msgid;
return NEW;
end;
'
language 'plpgsql';

create trigger tusub after insert into psubstrings for each row execute usub; 


create or replace function msub()
returns trigger as
'
declare
	fcur int;
	ftotal int;
begin
	select cur, total into fcur, ftotal from psubparts where msgid = new.msgid;
	if ftotal > 0 then
		if fcur = ftotal then
			insert into pmerged_msg (msgid, msg)
			select s.msgid,lo_from_bytea(msgid + 1000,string_agg(s.fsubstring, '')) as merged_string
			from psubstrings s
			where s.msgid = new.msgid; 
		end if;
	end if;
return NEW;
end;
'
language 'plpgsql';


create trigger tmsub after update on psubparts for each row execute msub;
	







