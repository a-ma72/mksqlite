function sqlite_test_regex

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );

    %% Create a new in-memory database
    db = mksqlite( 0, 'open', ':memory:' );
    
    % define an email address
    email = 'Guenther.Mayer@domain.de';

    fprintf( 'Some examples on regular expressions (email="%s"):\n\n', email );

    %% Some regex examples (only some ideas, you'll find your specific needs...)
    fprintf( '%s\n', 'Find matching string: mksqlite( ''SELECT REGEX(?,"M...r") as result'', email )' );
    disp( mksqlite( db, 'SELECT REGEX(?,"M...r") as result', email ) )

    fprintf( '%s\n', 'Parse entire email: mksqlite( ''SELECT REGEX(?,"([^@]+@(.*))\.(.*)") as result'', email )' );
    disp( mksqlite( db, 'SELECT REGEX(?,"([^@]+@(.*))\.(.*)") as result', email ) )

    fprintf( '%s\n', 'Omit name only (3rd argument is replacement string): mksqlite( ''SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1") as result'', email )' );
    disp( mksqlite( db, 'SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1") as result', email ) )

    fprintf( '%s\n', 'Replace domain: mksqlite( ''SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1@$2.com") as result'', email )' );
    disp( mksqlite( db, 'SELECT REGEX(?,"([^@]+)@(.*)\.(.*)", "$1@$2.com") as result', email ) )


    mksqlite( db, 'close' );

    if length(dbstack) == 1
        input('Press <return> ' );
    end
    
    % ---------------------------------------------------------------

    %% Small example recursive scanning a path and storing results into a database to easily query some statistics later
    clc
    
    % on-disc database filename to create
    db_name = 'winsys32.db';
    
    % root path which will be scanned
    root = fullfile( getenv('windir'), 'system32' );
    fprintf( 'For the next example the path "%s" will be scanned for files (bat,exe,dll).\n', root );
    fprintf( 'After the scan SQLite will be used to show summaries on catched files...\n' );
    
    if length(dbstack) == 1
        input( 'Press <return> to progress, Ctrl+C otherwise...' );
    end

    if exist( db_name, 'file' )
        fprintf( '\nReopen database file of last scan results...' );
        mksqlite( 'open', db_name ); % open existing dbase
    else
        fprintf( '\nScan in progress...\n' );
        fprintf( 'Create local dbase file of all asked files in %s...\n', root );
        mksqlite( 'open', db_name ); % create new dbase
        mksqlite( ['CREATE TABLE IF NOT EXISTS files '  , ...
                   '(id       PRIMARY KEY, '            , ...
                   ' name     TEXT, '                   , ...
                   ' parent   INTEGER, '                , ...
                   ' date     DATE, '                   , ...
                   ' size     INTEGER)'] );
        mksqlite( 'DELETE FROM files' ); % delete all records
        
        % recursive scan program files directory for EXEs, DLLs, ...
        path_trace( root, {'.exe', '.dll', '.bat'} ); 
    end

    %% Query statistics from database and display
    clc
    fprintf( '\n\n%s\n', 'Analyse path scan: count files and file sizes, grouped by extension' );
    query = mksqlite( ['SELECT SUM(CAST(size AS REAL)) AS sum, ', ...
                       '       COUNT(*) as count, ', ...
                       '       REGEX(lower(name),"^.*\.(.*)$","$1") as ext ', ...
                       'FROM files WHERE size NOT NULL GROUP BY ext'] );

    for i = 1:numel(query)
        disp( query(i) )
    end

    
    %% Rebuild full file names of all found DLLs and display
    fprintf( '\n\n%s\n', 'Display filenames of found DLLs (first 20)' );
    
    if length(dbstack) == 1
        input('Press <return> to continue with last step...' );
    end

    query = mksqlite( ['SELECT * FROM files ', ...
                       'WHERE REGEX(lower(name),"^.*\.(.*)$","$1")="dll" ', ...
                       'ORDER BY date LIMIT 20'] );

    %% Rebuild full path by backtracing and display
    for i = 1:numel( query )
        q = query(i);
        name = q.name;
        while q.parent > 0
            q = mksqlite( 'SELECT * FROM files WHERE id=?', q.parent );
            name = fullfile( q.name, name );
        end
        fprintf( '%s\n', fullfile( root, name ) );
    end

    % That's it...
    mksqlite( 'close' );


%% Path scan tool for recursive search
function path_trace( pathname, extensions, parent_id )
    if nargin < 3
      parent_id = 0;
    end

    % get the parent path by its ID from the database
    query = mksqlite( 'SELECT COUNT(*) AS id FROM files' );
    id = query.id;

    % process all files in this path and store all in one transaction
    mksqlite( 'BEGIN' );
    files = dir( pathname );
    if ~isempty( files )
        for i = 1:numel( files )
            filename = files(i).name;
            if filename(1) ~= '.'  % skip './' and '../'
                fullfilename = fullfile( pathname, filename );
                if ~files(i).isdir  % only files here
                    [trash, name, ext] = fileparts( fullfilename );
                    if any( strcmpi( ext, extensions ) ) % only extensions demanded
                        id = id + 1;
                        mksqlite( ['INSERT INTO files ', ...
                                   '(id,name,parent,date,size) ', ...
                                   'VALUES (?,?,?,?,?)'], ...
                                   id, [name ext], parent_id, ...
                                   datestr( files(i).datenum, 'yyyy-mm-dd HH:MM:SS.FFF' ), ...
                                   files(i).bytes );
                        if length(dbstack) == 1
                            fprintf( 'File: %s\n', fullfilename );
                        end
                    end
                end
            end
        end
    end
    mksqlite( 'COMMIT' );

    % process all subpaths in this path now
    if ~isempty( files )
        for i = 1:numel( files )
            filename = files(i).name;
            if filename(1) ~= '.'  % skip './' and '../'
                fullfilename = fullfile( pathname, filename );
                if files(i).isdir  % only paths now
                    query = mksqlite( 'SELECT COUNT(*) AS id FROM files' );
                    id = query.id + 1;
                    mksqlite( ['INSERT INTO files ', ...
                               '(id,name,parent) ', ...
                               'VALUES (?,?,?)'], ...
                               id, filename, parent_id );
                    % scan this subpath
                    path_trace( fullfilename, extensions, id );
                    if length(dbstack) == 1
                        fprintf( 'Dir: %s\n', fullfilename );
                    end
                end
            end
        end
    end
