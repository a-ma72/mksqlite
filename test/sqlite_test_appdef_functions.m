function sqlite_test_appdef_functions
  addpath( '..' ),
  mksqlite( 0, 'close' );
  mksqlite( 'result_type', 0 );
  mksqlite( 'open', '' );
  
  % Create a SQL function
  mksqlite( 'create function', 'norminv', @norminv );
  
  mksqlite( 'CREATE TABLE tbl (id, text)' );
  
  mksqlite( 'create function', 'ontrigger', @displ );
  mksqlite( 'create trigger mytrigger before insert on tbl begin SELECT ontrigger(NEW.text); END' );
  mksqlite( 'INSERT INTO tbl (id,text) VALUES( 1,"Test" )' );
  
  % Function test
  q = mksqlite( 'SELECT norminv( 0.9, 0.0, 1.0 ) AS x' );
  assert( q.x == norminv(0.9, 0, 1) );
  
  % Remove function
  mksqlite( 'create function', 'norminv', [] );
  try
    q = mksqlite( 'SELECT norminv( 0.5, 0.0, 1.0 ) AS x' );
    assert( false);
  catch
  end
  
  mksqlite( 'create function', 'test', @test );
  q = mksqlite( 'SELECT test(?) as x', 2 );
  
end


function y = displ( x )
  y = 1;
  disp(xx)
end


function result = test( value )
  q = mksqlite( 'SELECT test(1) AS x' );
  %q.x = 3.5;
  result = q.x .* value;
  fprintf( '%g x %g = %g\n', q.x, value, result );
end