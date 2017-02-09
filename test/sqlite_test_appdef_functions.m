function sqlite_test_appdef_functions
  %% Initialize
  mksqlite( 0, 'close' );
  mksqlite( 'result_type', 0 );
  mksqlite( 'param_wrapping', 1 );
  mksqlite( 'open', '' );
  mksqlite( 'lang', 0 );
  clc
  
  %% Create SQL functions and table
  mksqlite( 'create function', 'norminv', @norminv );
  mksqlite( 'create function', 'anon', @(a,b)a*b );
  mksqlite( 'create function', 'func', @func );
  mksqlite( 'create function', 'errorneous_func', @errorneous_func );
  mksqlite( 'create function', 'recursive_func', @recursive_func );
  mksqlite( 'create function', 'trigger_func', @trigger_func );
  mksqlite( 'create aggregation', 'aggregate_func', @aggregate_step, @aggregate_final );
  
  mksqlite( 'CREATE TABLE tbl (id, number, text)' );
  
  %% Test norminv
  fprintf( 'SELECT norminv( 0.9, 0.0, 1.0 ) AS x\n' );
  q = mksqlite( 'SELECT norminv( 0.9, 0.0, 1.0 ) AS x' );
  assert( q.x == norminv(0.9, 0, 1) );
  fprintf( '...is %g\nOk\n\n', q.x );
  
  %% Test errorneous function
  fprintf( 'Test errorneous function\n' );
  try
    mksqlite( 'SELECT errorneous_func(1)' );
    assert( false );
  catch ME
    fprintf( 'Successfully caught exception "%s":\n%s\nOk\n\n', ME.identifier, ME.getReport('basic') );
  end
  
  %% Test recursive function
  fprintf( 'Test recursive function\n' );
  try
    mksqlite( 'SELECT recursive_func(1)' );
    assert( false );
  catch ME
    fprintf( 'Successfully caught exception "%s":\n%s\nOk\n\n', ME.identifier, ME.getReport('basic') );
  end
  
  %% Test insert trigger
  mksqlite( 'CREATE TRIGGER mytrigger BEFORE INSERT ON tbl BEGIN SELECT trigger_func(NEW.text); END' );
  data = {1, randn(1), 'First value';
          1, randn(1), 'Second value';
          1, randn(1), 'Third value' };
        
  fprintf( 'Now three insertion triggers should follow...\n' );
  mksqlite( 'INSERT INTO tbl (id,number,text) VALUES(?,?,?)', data' );
  fprintf( 'Done\n\n' );
  
  %% Test function
  fprintf( 'Test function func()\n' );
  q = mksqlite( 'SELECT id,func(text,number) FROM tbl' );
  fprintf( 'Ok\n\n' );
  
  %% Test remove function
  fprintf( 'Test removing application-defined function\n' );
  mksqlite( 'create function', 'norminv', [] );
  try
    q = mksqlite( 'SELECT norminv( 0.5, 0.0, 1.0 ) AS x' );
    assert( false);
  catch ME
    fprintf( 'Ok\n\n' );
  end
  
  %% Test aggregate function
  fprintf( 'Test aggregate function\n' );
  q = mksqlite( ['SELECT aggregate_func(number, 1.35) AS x, SUM(1.35*number)/COUNT(number) AS y ', ...
                 'FROM tbl GROUP BY ID'] );
  fprintf( '%g == %g\n', q.x, q.y );
  assert( abs( q.x - q.y ) < 1e-5 );
  fprintf( 'Ok\n' );
  
  %% Close database
  mksqlite( 0, 'close' );
  
end

%% Subfunctions
function result = errorneous_func( value )
  notexistingfunc( value );
end


function result = recursive_func( value )
  mksqlite( 'SELECT recursive_func(?)', value );
end


function result = func( text, value )
  % Subsequent calls to mksqlite are allowed (whilst not recursive!)
  q = mksqlite( 'SELECT id FROM tbl WHERE text=?', text );
  assert( q.id == 1 );
  fprintf( '%s is %g having id %d\n', text, value, q.id );
  result = [];
end


function result = trigger_func( value )
  fprintf( '%s', 'Triggered: ' );
  disp( value );
  result = [];
end


function data = aggregate_step( data, value_1, value_2 )
  if isempty( data )
    data{1} = value_1 * value_2;
    data{2} = 1;
  else
    data{1} = data{1} + value_1 * value_2;
    data{2} = data{2} + 1;
  end
end


function result = aggregate_final( data )
  if isempty( data )
    result = [];
  else
    result = data{1} / data{2};
  end
end
