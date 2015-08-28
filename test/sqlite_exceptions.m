function sqlite_exceptions

    clear all
    close all
    clc
    dummy = mksqlite('version mex');
    fprintf( '\n\n' );

    commands = { ...
      'mksqlite(''open'',''filenotfound.db'', ''ro'');', ...  % opens non-existent file
      'mksqlite(''open'','''');', ...                         % open in-memory file
      'mksqlite(''select column_doesnt_exist'');', ...        % select a column that not exist
      'mksqlite(''syntax error'');', ...                      % Unknown statements
      'mksqlite( -10, ''open'', '''' );', ...                 % Illegal dbid
      'mksqlite( ''open'', '''' );', ...                      % MATLAB syntax error (not catched!)
    };

    for i = 1:numel( commands )
        if i == numel( commands )
            fprintf( '\nNext error will not be catched and thus terminates this script:' );
        end
        
        fprintf( '\nCalling %s\n', commands{i} );

        try
            eval( commands{i} );
            fprintf( 'succeeded\n' );

        % Exception handling
        catch ex

            % Get the error identifier
            switch ex.identifier
                case 'MKSQLITE:ANY'
                    % All errors from mksqlite
                    fprintf( 'mksqlite omitted an error: "%s"\n', ex.message );

                case 'SQLITE:ERROR'
                    % Explicit handler for SQLite syntax errors
                    fprintf( 'SQLite syntax error: "%s"\n', ex.message );

                otherwise
                    % Split "SQLITE:NAME" into its fields --> C={'SQLITE','NAME'}
                    C = regexp( ex.identifier, ':', 'split' );

                    if numel(C) == 2 && strcmpi( C{1}, 'SQLITE' )
                        % Handler for all remaining SQLite errors
                        switch( C{2} )
                            case {'AUTH', 'CANTOPEN' }
                                % Handle SQLITE:AUTH and SQLITE:CANTOPEN
                                fprintf( 'SQLite error while open database: "%s"\n', ex.message );

                            otherwise
                                % Any other SQLite error
                                fprintf( 'Any other SQLite error: "%s"\n', ex.message );
                        end
                    else
                        % Any other unknown exception, rethrow to outer scope
                        rethrow(ex);
                    end
            end

        end
    end
    