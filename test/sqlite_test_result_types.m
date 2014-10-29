function sqlite_test_result_types ()

clc

database = 'my_testdb';
table = 'test_table';

NumOfSamples = 10;

try
    delete (database);
catch
    error ('Unable to delete database');
end

% Open database
mksqlite('open', database);
mksqlite('PRAGMA synchronous = OFF');
mksqlite('result_type', 0);

% Create table
fprintf ('Create new table\n');
mksqlite(['create table ' table ' (Entry char(32), BigFloat double, ManyChars char(255))']);

disp ('------------------------------------------------------------');

fprintf ('Create %d records in one single transaction\n', NumOfSamples);
% Create datasets
ManyChars = '12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890';
tic;
mksqlite('begin');

for idx=1:NumOfSamples
    mksqlite(['insert into ' table ' (Entry, BigFloat, ManyChars) values (?,?,?)'], sprintf('Entry_%d', idx), idx, ManyChars );
end
    
mksqlite('commit');
toc
fprintf ('Done!\n');

fprintf ('Query amount of records\n')
res = mksqlite(['select count(*) as count from ' table]);
fprintf ('select count(*) returned %d\n', res.count);


disp ('------------------------------------------------------------');
mksqlite('close');
mksqlite('open', database);

disp ('Read all records as array of structs');
tic;
mksqlite('result_type', 0);
[res, res_count, col_names] = mksqlite(['SELECT * FROM ' table])
a = toc;
fprintf ('ready, %f seconds = %d records per second\n\n', a, int32(NumOfSamples/a));

disp ('Read all records as struct of arrays');
tic;
mksqlite('result_type', 1);
[res, res_count, col_names] = mksqlite(['SELECT * FROM ' table])
a = toc;
fprintf ('ready, %f seconds = %d records per second\n\n', a, int32(NumOfSamples/a));

disp ('Read all records as cell array/matrix');
tic;
mksqlite('result_type', 2);
[res, res_count, col_names] = mksqlite(['SELECT * FROM ' table])
a = toc;
fprintf ('ready, %f seconds = %d records per second\n', a, int32(NumOfSamples/a));

disp ('ready.');

% Close database
mksqlite('close');
