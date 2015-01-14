function sqlite_test

clc

database = 'my_testdb';
table = 'test_table';

NumOfSamples = 100000;

try
    delete (database);
catch
    error ('Unable to delete database');
end

% Open database
mksqlite('open', database);
mksqlite('PRAGMA synchronous = OFF');

% Create table
fprintf ('Create new table\n');
mksqlite(['create table ' table ' (Entry char(32), BigFloat double, SmallFloat float, Value int, Chars tinyint, Boolean bit, ManyChars char(255))']);

disp ('------------------------------------------------------------');

fprintf ('Create %d records in one single transaction\n', NumOfSamples);
% Create datasets
ManyChars = '12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890';
tic;
mksqlite('begin');
try
    for idx=1:NumOfSamples
    	mksqlite(['insert into ' table ' (Entry, BigFloat, ManyChars) values (?,?,?)'], sprintf('Entry_%d', idx), idx, ManyChars );
    end
catch
end
mksqlite('commit');
toc
fprintf ('Done!\n');

fprintf ('Query amount of records\n')
res = mksqlite(['select count(*) as count from ' table]);
fprintf ('select count(*) returned %d\n', res.count);

fprintf ('Cumulate all values between 10 and 75\n');
res = mksqlite(['select sum(BigFloat) as cumsum from ' table ' where BigFloat between 10 and 75']);
fprintf ('Sum is %d\n', res.cumsum);

disp ('------------------------------------------------------------');
mksqlite('close');
mksqlite('open', database);

disp ('Read all records as array of structs');
tic;
res = mksqlite(['SELECT * FROM ' table]);
a = toc;
fprintf ('ready, %f seconds = %d records per second\n\n', a, int32(NumOfSamples/a));

% Close database
mksqlite('close');

% Copy database to an in-memory one

disp (' ');
disp ('-- in-memory test --');

disp ('copy database in-memory');
% Create in-memory database
mksqlite('open', ':memory:');

% Attach original
mksqlite(['ATTACH DATABASE ''' database ''' AS original']);
mksqlite('begin');
% Copy each table
tables = mksqlite('SELECT name FROM original.sqlite_master WHERE type = ''table''');
for idx=1:length(tables)
    mksqlite(['CREATE TABLE ''' tables(idx).name ''' AS SELECT * FROM original.''' tables(idx).name '''']);
end
% Copy all indexing tables
tables = mksqlite('SELECT sql FROM original.sqlite_master WHERE type = ''index''');
for idx=1:length(tables)
    mksqlite(tables(idx).sql);
end
% Detach original
mksqlite('commit');
mksqlite('DETACH original');
disp ('Copying done.');

% Process in-memory test
fprintf ('Query record count\n')
res = mksqlite(['select count(*) as count from ' table]);
fprintf ('select count(*) returned %d\n', res.count);

fprintf ('Cumulate all values between 10 and 75\n');
res = mksqlite(['select sum(BigFloat) as cumsum from ' table ' where BigFloat between 10 and 75']);
fprintf ('Sum is %d\n', res.cumsum);
disp ('Query all records into an array');
tic;
res = mksqlite(['SELECT * FROM ' table]);
a = toc;
fprintf ('ready, %f seconds = %d records per second\n', a, int32(NumOfSamples/a));
disp ('ready.');

% Close database
mksqlite('close');
