function [result, count] = sql( first_arg, varargin )
% sql() shortens a combination of sprintf() and mksqlite() calls.
% Example:
%   [query, count] = sql( 'SELECT * FROM %s WHERE rowid=?', 'my_table', desired_row );

  if ~isnumeric( first_arg )
      db = {};
      s = first_arg;
  else
      db = {first_arg};
      s = varargin{1};
      varargin(1) = [];
  end
  
  nParams = 0;
  assert( ischar(s) );
  
  % count string placeholders (i.e. %d) for sprintf
  % nParams holds the number of placeholders
  for i = 1:length(s)
      if s(i) == '%'
          nParams = nParams + 1;
          if i < length(s) && s(i+1) == '%'
              i = i + 1;
              nParams = nParams - 1;
          end
      end
  end
  
  % if there are placeholders in SQL string, build
  % the SQL query by sprintf() first.
  if nParams > 0
      s = sprintf( s, varargin{1:nParams} );
      varargin(1:nParams) = [];
  end
  
  % remaining arguments are for SQL parameter binding (via ?)
  args = { db{:}, s, varargin{:} };
  
  if ~nargout
      mksqlite( args{:} );
  else
      result = mksqlite( args{:} );
  end
  
  if nargout > 1
      if isstruct( result )
          count = numel( result );
      else
          count = 0;
      end
  end
end

