function sqlite_test_bind_typed_compressed ()

  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Datenbank und Inhalt erzeugen  %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  close all
  clear all
  clc
  if isempty( mfilename )
    pack
  end
  
  mem_start = memory

  if exist( 'TBH_data.db', 'file' )
    delete( 'TBH_data.db' );
  end
  
  mksqlite( 'open', 'TBH_data.db' ); % Datenbank als Datei erzeugen
  mksqlite( 'drop table if exists demo' );
  mksqlite( 'vacuum' );

  % Tabelle anlegen
  mksqlite( ['create table demo (ID primary key, Type, Data, Size, ', ...
             'Level, PackRatio, PackTime, UnpackTime, MD5)'] );

  use_typed_blobs = 2; % This may be unsupported by erlier MATLAB versions. Use 1 instead then
  compressor = 'QLIN16';
  %compressor = 'lz4hc';
  compression_level = 0;
  
  % You're not limited in mixing compressed and uncompressed data in the data base!
  mksqlite( 'typedBLOBs', use_typed_blobs ); % Typisierung der BLOBs
  mksqlite( 'compression', compressor, compression_level ); % Kompression der BLOBs
  mksqlite( 'compression_check', 1 );
  
  for n = 1:10000;
    
    data = cumsum( randn( 10000, 1) );

    if 0
      mksqlite( 'insert or replace into demo (ID, Type, Data, Size, Level) values (?,?,?,?,?)', ...
                n, 1, data, 1, compression_level );
    else
      q = mksqlite( 'select BDCPackTime(?) as t_pack, BDCUnpackTime(?) as t_unpack, BDCRatio(?) as ratio', data, data, data );
    end
        
    if n > 1
      fprintf( '%c', [8,8,8,8,8,8] );
    end
    
    fprintf( '%06d', n );
  end

  mksqlite( 0, 'close' );
  
  close all
  clear all
  if isempty( mfilename )
    pack
  end
  
  mem_end = memory
end