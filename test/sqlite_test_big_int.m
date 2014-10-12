function sqlite_test_big_int ()

clc
format long

% Datenbank öffnen (in-memory)
mksqlite('open', '');

% Testtabelle erzeugen
mksqlite('create table bigint (x int)');

% mksqlite schaltet automatisch auf long long, wenn integer Typen nicht 
% Verlustfrei als double zurueckgegeben werden koennen:
for i = 1:2
    if i == 1
        value = uint8( hex2dec( ['CC'; 'CC'; 'CC'; 'CC'; '00'; '00'; '00'; '00'] ) );
        value = typecast( value', 'int64' );
    else
        value = uint8( hex2dec( ['CC'; 'CC'; 'CC'; 'CC'; 'CC'; 'CC'; 'CC'; '7C'] ) );
        value = typecast( value', 'int64' );
    end

    mksqlite('insert into bigint values (?)', value );

    value = mksqlite('select x, printf("%d",x) as x_dec from bigint');

    fprintf( 'Type: %s, Dec (SQL): %s\nMatlab representation:', class( value.x ), value.x_dec );
    cast( value.x, 'int64' )
    
    fprintf( '\n\n' );
    mksqlite( 'delete from bigint' );
end

mksqlite('close');


