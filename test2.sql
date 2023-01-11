
	drop table psubstrings;
	drop table psubparts;
	drop table pmerged_msg;

	create table psubstrings (
	substrid serial, substring bytea,
	msgid int);
	
	create table psubparts(msgid int, cur int, total int);
	create table pmerged_msg(msgid int, msg oid);
		

	create or replace function usub()
	returns trigger as
	'
	declare
	fcur int;
	begin
		select cur into fcur from psubparts where msgid = new.msgid;
		raise notice ''cur % soy boy %'', fcur, new.msgid;
		update psubparts set cur = cur + 1 where msgid = new.msgid;
	return NEW;
	end;
	'
	language 'plpgsql';

	create trigger tusub after insert on psubstrings for each row execute function usub(); 


	create or replace function msub()
	returns trigger as
	'
	declare
		fcur int;
		ftotal int;
		fmsgid int;
	begin
		select msgid, cur, total into fmsgid, fcur, ftotal from psubparts where msgid = new.msgid;
		if ftotal > 0 then
			if fcur >= ftotal then
				fmsgid = fmsgid + 1010 + 3;
				insert into pmerged_msg (msgid, msg)
				select s.msgid, lo_from_bytea(fmsgid, string_agg(s.substring, '''')) as merged_string
				from psubstrings s 
				where s.msgid = new.msgid
				group by s.msgid;
			end if;
		end if;
	return NEW;
	end;
	'
	language 'plpgsql';


	create trigger tmsub after update on psubparts for each row execute function msub();
		


