function sqlite_test_bind_typed_compressed ()
  
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Datenbank und Inhalt erzeugen  %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  mksqlite( 'open', 'TBH_data.db' ); % Datenbank als Datei erzeugen
  mksqlite( 'drop table if exists demo' );
  mksqlite( 'vacuum' );
  
  % Tabelle anlegen
  mksqlite( ['create table demo (ID primary key, Type, Data, Size, ', ...
             'Level, PackRatio, PackTime, UnpackTime, MD5)'] );
  
  for n = 1:10000;
    type = randi(3);
    compression_level = randi(10)-1;
    use_typed_blobs = randi(2)-1;
    
    switch type
      case 1
        data = randn( 1 + randi( 10 ) );
      case 2
        data = randn( 10000 + randi(1e4), 1);
      case 3
        data = cumsum( randn( 10000 + randi(1e4), 1) );
    end
    
    nElements = numel( data );
    
    mksqlite( 'typedBLOBs', use_typed_blobs ); % Typisierung der BLOBs
    mksqlite( 'compression', 'blosclz', compression_level ); % Kompression der BLOBs
    
    if ~use_typed_blobs
      compression_level = 0;
    end
    
    mksqlite( 'insert or replace into demo (ID, Type, Data, Size, Level) values (?,?,?,?,?)', ...
              n, type, data, nElements, compression_level );
    clc, fprintf( '%d\n', n );
  end
  
  mksqlite( 'update demo set PackRatio=BDCRatio(Data), PackTime=BDCPackTime(Data), UnpackTime=BDCUnpackTime(Data), MD5=MD5(Data)' );
  
  % Kompressionsrate für "echte" Zufallszahlen zu gering:
  query = mksqlite( 'select Type, Size, Level, PackRatio, PackTime, UnpackTime from demo where type<3' );
  figure, hist( [query.PackRatio]', 50 )
  
  % Kompressionsrate für "echte" Zufallszahlen zu gering:
  min_level = 1;
  query = mksqlite( 'select Type, Size, Level, PackRatio, PackTime, UnpackTime from demo where type=3 and Level>=?', min_level );
  figure, hist( [query.PackRatio]', 50 )
  
  mksqlite( 0, 'close' );

end