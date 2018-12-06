function sqlite_test_bind_typed_wrapped
%
    clear all; close all; clc
    
    %% Create an in-memory database
    sql( 'open', '' );

    % enable stmt re-use
    sql( 'param_wrapping', 1 )
    
    % enable typed BLOBs
    sql( 'TypedBLOBs', 2 );
    
%%
    clear s
    for i = 1:100
        s(i).Name = char( randi(25,1,10) + 'A' );
        s(i).data = randn( 3, 5 );
        s(i).data2 = { struct( 'Color', 'red', 'Value', 10 )
                       struct( 'Color', 'green', 'Value', 20 ) };
    end
    
    % write to SQL all at once
    sql( 'CREATE TABLE test (Name TEXT, data, data2)' );
    sql( 'BEGIN' ); % Begin transaction
    sql( 'INSERT INTO TEST ([#]) VALUES ([:#])', s );
    sql( 'COMMIT' ); % Commit transaction
    
    % Read back values
    test = sql( 'SELECT * FROM test' );
    disp( test );
    disp( test(1) );
    disp( test(1).data );
    disp( test(1).data2 );
    disp( test(1).data2{1} );
    
    sql( 'close' );
end
