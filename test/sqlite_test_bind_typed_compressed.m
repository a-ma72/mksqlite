function sqlite_test_bind_typed_compressed

  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Create database with some records  %
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  clear all
  close all
  clc

  mksqlite( 'open', 'TBH_data.db' ); % Create one-file database
  mksqlite( 'drop table if exists demo' );
  mksqlite( 'vacuum' );

  % Create table
  mksqlite( ['create table demo (ID primary key, Type, Data, Size, ', ...
             'Level, PackRatio, PackTime, UnpackTime, MD5)'] );

  fprintf( 'Please wait, while generating 10000 entries...\n' );
  for n = 1:10000
    compression_level = randi(10)-1;
    use_typed_blobs   = randi(2)-1;
    
    if ~use_typed_blobs
      compression_level = 0;
    end

    data = [];
    
    while isempty( data )
      type = randi(3);
      switch type
        case 1
          if use_typed_blobs % Arrays assume typed blobs
            data = randn( 1 + randi( 10 ) );
          end
        case 2
          data = randn( 10000 + randi(1e4), 1);
        case 3
          data = cumsum( randn( 10000 + randi(1e4), 1) );
      end
    end

    nElements = numel( data );
    
    % You're not limited in mixing compressed and uncompressed data in the data base!
    mksqlite( 'typedBLOBs', use_typed_blobs ); % Using typed BLOBs
    mksqlite( 'compression', 'blosclz', compression_level ); % Set compression for typed BLOBs

    mksqlite( 'insert or replace into demo (ID, Type, Data, Size, Level) values (?,?,?,?,?)', ...
              n, type, data, nElements, compression_level );
          
    clc, fprintf( '%d\n', n );
  end

  fprintf( 'Please wait, while updating database...\n' );
  mksqlite( 'update demo set PackRatio=BDCRatio(Data), PackTime=BDCPackTime(Data), UnpackTime=BDCUnpackTime(Data), MD5=MD5(Data)' );

  % Compression of "real" random numbers is poor:
  query = mksqlite( 'select Type, Size, Level, PackRatio, PackTime, UnpackTime from demo where type<3' );
  figure, hist( [query.PackRatio]', 50 )

  % Compression of "real" random numbers is poor:
  min_level = 1;
  query = mksqlite( 'select Type, Size, Level, PackRatio, PackTime, UnpackTime from demo where type=3 and Level>=?', min_level );
  figure, hist( [query.PackRatio]', 50 )
  
  % Close all databases
  mksqlite( 0, 'close' );

end