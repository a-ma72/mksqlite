function sqlite_test_regex
  db_name = 'tracklist.db';
  root = getenv('ProgramFiles');
  if exist( db_name, 'file' )
      mksqlite( 'open', db_name ); % open existing dbase
  else
      fprintf( 'Create dbase file of files in %s...\n', root );
      mksqlite( 'open', db_name ); % create new dbase
      mksqlite( 'CREATE TABLE IF NOT EXISTS files (id PRIMARY KEY, name TEXT, parent INTEGER, date DATE, size INTEGER)' );
      mksqlite( 'DELETE FROM files' ); % delete all records
      path_trace( root, {'.exe', '.dll', '.bat'} ); % scan program files directory for EXEs, DLLs, ...
  end

  clc

  fprintf( 'Some examples on regular expressions...\n\n' );

  % some regex examples (only some ideas, you'll find your specific needs...)
  email = 'Günther.Mayer@domain.de';

  %
  fprintf( '%s\n', 'Find matching string: mksqlite( ''SELECT REGEX(?,"M...r") as result'', email )' );
  mksqlite( 'SELECT REGEX(?,"M...r") as result', email )

  fprintf( '%s\n', 'Parse entire email: mksqlite( ''SELECT REGEX(?,"([^@]+@(.*))\.(.*)") as result'', email )' );
  mksqlite( 'SELECT REGEX(?,"([^@]+@(.*))\.(.*)") as result', email )

  fprintf( '%s\n', 'Omit name only (3rd argument is replacement string): mksqlite( ''SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1") as result'', email )' );
  mksqlite( 'SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1") as result', email )

  fprintf( '%s\n', 'Replace domain: mksqlite( ''SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1@$2.com") as result'', email )' );
  mksqlite( 'SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1@$2.com") as result', email )


  fprintf( '\n\n%s\n', 'Analyse HD scan: count files and file sizes, grouped by extension' );
  query = mksqlite( ['SELECT SUM(size) as sum, COUNT(*) as count, REGEX(lower(name),"^.*\.(.*)$","$1") as ext ', ...
                     'from files where size not null group by ext'] );

  for i = 1:numel(query)
      disp( query(i) )
  end


  fprintf( '\n\n%s\n', 'Display filenames of all found DLLs' );
  input('Press <return> ' );

  query = mksqlite( 'SELECT * from files WHERE REGEX(lower(name),"^.*\.(.*)$","$1")="dll" ORDER BY date' );

  for i = 1:numel( query )
    q = query(i);
    name = q.name;
    while q.parent > 0
        q = mksqlite( 'SELECT * from files where id=?', q.parent );
        name = fullfile( q.name, name );
    end
    fprintf( '%s\n', fullfile( root, name ) );
  end

  % That's it...
  mksqlite( 'close' );
end


% HD scan tool
function path_trace( pathname, extensions, parent_id )
    if nargin < 3
      parent_id = 0;
    end

    query = mksqlite( 'select count(*) as id from files' );
    id = query.id;

    % Trace files
    mksqlite( 'BEGIN' );
    files = dir( pathname );
    if ~isempty( files )
        for i = 1:numel( files )
            filename = files(i).name;
            if filename(1) ~= '.'
                fullfilename = fullfile( pathname, filename );
                if ~files(i).isdir
                    [trash, name, ext] = fileparts( fullfilename );
                    if any( strcmpi( ext, extensions ) )
                        id = id + 1;
                        mksqlite( 'INSERT INTO files (id,name,parent,date,size) VALUES (?,?,?,?,?)', ...
                                   id, [name ext], parent_id, datestr(files(i).datenum, 'yyyy-mm-dd HH:MM:SS.FFF' ), files(i).bytes );
                        fprintf( 'File: %s\n', [name ext] );
                    end
                end
            end
        end
    end
    mksqlite( 'COMMIT' );

    % Trace directories
    if ~isempty( files )
        for i = 1:numel( files )
            filename = files(i).name;
            if filename(1) ~= '.'
                fullfilename = fullfile( pathname, filename );
                if files(i).isdir
                    % path found
                    query = mksqlite( 'select count(*) as id from files' );
                    id = query.id + 1;
                    mksqlite( 'INSERT INTO files (id,name,parent) VALUES (?,?,?)', id, filename, parent_id );
                    path_trace( fullfilename, extensions, id );
                    fprintf( 'Dir: %s\n', fullfilename );
                end
            end
        end
    end
end
