function varargout = sql( first_arg, varargin )
% sql() shortens a combination of sprintf() and mksqlite() calls.
% Example:
%   [query, count, colnames] = sql( 'SELECT * FROM %s WHERE rowid=?', 'my_table', desired_row );

  % check if first argument is a database slot id (dbid)
  if ~isnumeric( first_arg )
      dbid = {};
      query = first_arg;
  else
      dbid = {first_arg};
      query = varargin{1};
      varargin(1) = [];   % delete first argument
  end

  % dbid is now the database handle (if any),
  % stmt is the sql statement (or command) and
  % varagin{1:end} are the remaining arguments

  nParams = 0;
  assert( ischar(query) );

  % count sprintf placeholders (i.e. %d)
  % nParams holds the number of placeholders
  i = 1;
  while i < length(query)
      if query(i) == '%'
          nParams = nParams + 1;
          % check for '%%', which is no sprintf placeholder
          if i < length(query) && query(i+1) == '%'
              query(i+1) = [];
              nParams = nParams - 1;
          end
      end
      i = i + 1;
  end

  % if there are placeholders in SQL string, build
  % the SQL query by sprintf() first.
  if nParams > 0
      query = sprintf( query, varargin{1:nParams} );
      varargin(1:nParams) = []; % remove sprintf parameters
  end

  args = [ dbid, {query}, varargin ];

  % kv69 support named binding;
  if  isstruct(args{end})
      binds = regexp( query, ':(\w*)', 'tokens' ); % get bind names starting with ":" (but skipping)
      binds = [binds{:}]; % resolve nested cells
      [~, idx, ~] = unique(binds, 'first'); % Get the indexes of all elements excluding duplicates
      binds = binds( sort(idx) ); % get unique elements preserving order
      args{end} = rmfield( args{end}, setdiff( fieldnames(args{end}), binds ) ); % remove unused fields
      args{end} = orderfields( args{end}, binds ); % order remaining fields to match occurence in sql statement
      args{end} = struct2cell( args{end}(:) ); % retrieve data from structure (column-wise datasets)
  end


  % remaining arguments are for SQL parameter binding
  if ~nargout
    mksqlite( args{:} );
  else
    [varargout{1:nargout}] = mksqlite( args{:} );
  end

end

