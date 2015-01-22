function sqlite_real_example

%{
      % The logfile has folowing columns:
      1:  Code
      2:  Timestamp
      3:  Mileage at start (km)
      4:  Speed at start (km/h)
      5:  Engine speed at start (rpm)
      6:  Torque at start (Nm)
      7:  Duration (in tenths of a second)
      8:  Distance (m)
      9:  Speed at end (km/h)
      10: Engine speed at end
      11: Torque at end

      % Optional additional columns depending on "Code":
      A:  Optional parameter
      B:  Optional parameter
      C:  Optional parameter
      D:  Optional parameter
%}
    
    %% Reading content of the logging file in one cell array
    clear all, close all, clc
    
    fid = fopen ( 'logfile.asc', 'r' );
    assert( fid > 0 );
    %                          1  2  3  4  5  6  7  8  9  10 11 (ABCD)
    content = textscan( fid, '%s %s %s %s %s %s %s %s %s %s %s %[^\r\n]', ...
                        'CollectOutput', 1 );
    content = content{1};
    fclose( fid );
    
    %% Create default SQL table, feeded by cell array
    sql( 'open', '' );
    sql( ['CREATE TABLE mantab (' , ...
               '  Code, '              , ...
               '  Timestamp, '         , ...
               '  MileageStart  REAL, ', ...
               '  SpeedStart    REAL, ', ...
               '  EngSpeedStart REAL, ', ...
               '  TorqueStart   REAL, ', ...
               '  Duration      REAL, ', ...
               '  Distance      REAL, ', ...
               '  SpeedEnd      REAL, ', ...
               '  EngSpeedEnd   REAL, ', ...
               '  TorqueEnd     REAL, ', ...
               '  Optional      TEXT )'] );

    sql( 'param_wrapping', 1 );
    
    sql( ['INSERT INTO mantab '  , ...
          ' VALUES (?,?,?,?,?,?,?,?,?,?,?,?)'], ...
          content' );
           
    %% Identifying optional parameters
    % Codes B, P and V carrying acceleration values in optional columns
    % (A,B). Adding new column and translate parameter A and B as
    % acceleration.
    % Code O carries acceleration value in optional column A.
    
    optional =  { ...
                    { '"B","P","V"', 'BPV_AccMean',    'BPV_AccRng' },
                    { '"O"',         'O_LongAdj' },
                };

    % Creating new data column(s) and extract conditional parameters into them
    for i = 1:numel( optional )
        % Build regex pattern matching column count
        re = cell( 2, numel( optional{i} ) - 1 );
        re(1,:)     = {'([^\t*]*)'};
        re(2,:)     = {'\t'};
        re(end,end) = {''};
        re          = ['^', re{:}, '$'];
        for j = 2:numel( optional{i} )
            % Create column and update
            sql( 'ALTER TABLE mantab ADD COLUMN %s REAL', optional{i}{j} );
            sql( 'UPDATE mantab SET %s = REGEX( Optional, "%s", "$%d" ) WHERE Code IN (%s)', ...
                 optional{i}{j}, re, j-1, optional{i}{1} );
        end
    end
    
    sql( 'result_type', 2 );  % Result type set to "cell matrix"
    
    [result, count, names] = sql( 'SELECT * FROM mantab WHERE Code="B" AND BPV_AccMean>30 ORDER BY BPV_AccRng' );
    
    plot( [result{:,14}], 'k-', 'linewidth', 3 )
    
    sql( 'close' );
