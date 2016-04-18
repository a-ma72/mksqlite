function sqlite_test_bind_named
%
    clear all; close all; clc
    
    % use_sql_m: 
    %  false: use mksqlite only
    %  true:  use sql.m wrapper
    use_sql_m = true;  
    

    if use_sql_m
        sqlfcn = @sql;
    else
        sqlfcn = @mksqlite;
    end
    
    %% Create an in-memory database
    sqlfcn( 'open', '' );

    % enable Foreign key constraint check
    sqlfcn( 'PRAGMA foreign_keys = ON' ); 
    % enable stmt re-use
    sqlfcn( 'param_wrapping', 1 )
    
    %% Create a N-element dataset with activities for some users
    N    = 1000;
    user = { 'user_A', 'user_B', 'user_C' };
    dat  = struct( 'user', { user{ ceil( rand(N,1) * length(user) ) } }, ...
                   'activity', cellstr( char( '@' + ceil( rand(N,10) * 26 ) ) )' )

    fprintf( 'Make a schema with normalized user table...\n' );
    
    if use_sql_m
        tbl_user = struct;
        tbl_user.user_id = 'INTEGER PRIMARY KEY';
        tbl_user.name    = 'TEXT';
        sqlfcn( 'CREATE TABLE user ([*#])', tbl_user );

        tbl_data = struct;
        tbl_data.user_id = 'INTEGER';
        tbl_data.activity    = 'TEXT';
        sqlfcn( 'CREATE TABLE data ([*#], FOREIGN KEY (user_id) REFERENCES user(user_id))', tbl_data );
    else
        sqlfcn( 'CREATE TABLE user(user_id INTEGER PRIMARY KEY, name TEXT)' );
        sqlfcn( 'CREATE TABLE data(user_id INTEGER, activity TEXT, FOREIGN KEY (user_id) REFERENCES user(user_id))' );
    end


    %% Normalize the data
    [user_list, ~, ind] = unique({dat.user});

    %% Fill user table with unique user names and retrieve their user_id
    [~, ~, ~, row_id] = sqlfcn( 'INSERT INTO user (name) VALUES (:name)', ...
                                 struct( 'name', user_list) ) % get the auto increment user_id back
                            
    assert( isequal( row_id, [1,2,3]' ) );

    %% Merge user_id and struct data
    userId        = num2cell( row_id(ind) ); 
    [dat.user_id] = deal( userId{:} ); % add the user_id as new field

    %% Fill data table per struct data
    if use_sql_m
        sqlfcn( 'INSERT INTO data ([#]) VALUES ([:#])', rmfield( dat, 'user' ) );
    else
        sqlfcn( 'INSERT INTO data (user_id,activity) VALUES (:user_id,:activity)', dat );
    end

    %% Deletion of referred data should lead to error
    try
        sqlfcn( 'DELETE FROM user' )
        fprintf( 'Test failed: should not be here!' );
    catch err
        if strcmpi( err.message, 'FOREIGN KEY constraint failed' )
            fprintf( ['Error successfully catched:\n', ...
                      'entries from user table can''t be deleted ', ...
                      'because they are referred by a foreign key of data table\n\n'] );
        else
            rethrow(err)
        end
    end
    
    %% Read back entire data and compare input and output structures
    
    sqlfcn( 'result_type', 0 ); % structs (default)
    test_type_0 = sqlfcn( 'SELECT name, activity, user_id FROM data JOIN user USING(user_id)' );
    
    sqlfcn( 'result_type', 2 ); % cell matrix
    test_type_2 = sqlfcn( 'SELECT name, activity, user_id FROM data JOIN user USING(user_id)' );
    
    lhs_0 = struct2cell(test_type_0(:))';
    lhs_2 = test_type_2;
    rhs   = struct2cell(dat(:))';
    
    assert( isequal(lhs_0, rhs) && isequal(lhs_2, rhs) );
    
    fprintf( 'Check: OK!\n' );
    
    %% Check multiple SELECT statements when parameter wrapping is on
    
    % Get the activities of each user ordered by user_id and activity
    % (still cell result type!)
    lhs = sqlfcn( ['SELECT name, activity FROM data JOIN user USING(user_id) ', ...
                     'ORDER BY user_id, activity'] );
              
    % Split into 3 individual SELECT statements, each ordered by activity,
    % should lead to the same result
    rhs = sqlfcn( ['SELECT name, activity FROM data JOIN user USING(user_id) ', ...
                     'WHERE user_id=? ORDER BY activity'], {1,2,3} );
    
    assert( isequal(lhs, rhs) );
    
    fprintf( 'Check: OK!\n' );

    %% Now delete some user dedicated data, to enable user detaching
    
    % delete some dedicated data
    sqlfcn( 'DELETE FROM data WHERE user_id=:myID', struct('myID', row_id(1) ) )
    % now you can delete also the corresponding user
    sqlfcn( 'DELETE FROM user WHERE user_id=:myID', struct('myID', row_id(1) ) )


    sqlfcn( 'close' )

    fprintf( 'Test finished successful!\n');
    