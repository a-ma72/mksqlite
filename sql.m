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

  % Assert query to be a vector of char type
  assert( ischar(query) && min( size( query ) ) == 1 );
  nParams = 0;

  % count sprintf placeholders (i.e. %d)
  % nParams holds the number of placeholders
  i = 1;
  while i < length(query)
      if query(i) == '%'
          nParams = nParams + 1;
          % check for '%%', which is no sprintf placeholder
          if query(i+1) == '%'
              query(i+1) = [];
              nParams = nParams - 1;
          end
      end
      i = i + 1;
  end

  % if there are placeholders in SQL string, build
  % the SQL query by sprintf() first.
  % First nParams parameters are taken as sprintf parameter list.
  if nParams > 0
      query = sprintf( query, varargin{1:nParams} );
      varargin(1:nParams) = []; % remove sprintf parameters
  end

  args = [ dbid, {query}, varargin ];

  % kv69 support named binding (only non-extended typedBLOBs)
  if isstruct(args{end}) && mksqlite( 'typedBLOBs' ) < 2
      % Replace special tokens [#], [:#], [=#], [+#] and [-#] referencing struct argument
      [match, tokens] = regexp( query, '\[(.?)#\]', 'match', 'tokens' );
      for i = 1:numel( match )
          query = strrep( query, match{i}, field_list( args{end}, tokens{i}{1} ) );
      end

      args = [ dbid, {query}, varargin ];

      % Get bind names starting with ":", "$" or "@" as cell array. (Character is not part of the names taken)
      binds = regexp( query, '[\$\:\@](\w*)', 'tokens' );
      binds = [binds{:}]; % resolve nested cells
      if isempty( binds )
          % No named bind names, discard struct argument!
          args(end) = [];
      else
        mex_ver = mksqlite( 'version mex' );
        mex_ver_dot = strfind( mex_ver, '.' );
        mex_ver_major = int16( str2double( mex_ver(1:mex_ver_dot-1) ) );
        mex_ver_minor = int16( str2double( mex_ver(mex_ver_dot+1:end) ) );
        % Since version 2.1 mksqlite handles named bindings with a struct
        % argument. For versions prior a cell argument have to be built for
        % compatibility reasons:
        if mex_ver_major < 2 || ( mex_ver_major == 2 && mex_ver_minor <= 1 )
            [~, idx, ~] = unique(binds, 'first'); % Get the indexes of all elements excluding duplicates
            binds = binds( sort(idx) ); % get unique elements, preserving order
            dataset = rmfield( args{end}, setdiff( fieldnames(args{end}), binds ) ); % remove unused fields
            dataset = orderfields( dataset, binds ); % order remaining fields to match occurence in sql statement
            dataset = struct2cell( dataset(:) ); % retrieve data from structure (column-wise datasets)
            args = [args(1:end-1), dataset(:)'];
        end
      end
  end


  % remaining arguments are for SQL parameter binding
  if ~nargout
    mksqlite( args{:} );
  else
    [varargout{1:nargout}] = mksqlite( args{:} );
  end

end


% Create a comma separated list of fields depending on "mode"
function list = field_list( struct_var, mode )
  assert( isstruct( struct_var ), '<struct_var must> be a structure type variable' );

  if ~exist( 'mode', 'var' )
    mode = '';
  else
    assert( ischar( mode ) && numel( mode ) < 2, '<mode> must be a char type variable' );
  end

  fnames = fieldnames( struct_var );
  switch mode
      case ''
        % Comma separated field names
        list = sprintf( '%s,', fnames{:} );
      case ':'
        % Comma separated field names, preceded by a colon
        % Example: sql( 'INSERT INTO tbl ([#]) VALUES ([:#]) WHERE ...', struct( 'a', 3.14, 'b', 'String', 'd', 1:5 ) )
        list = sprintf( ':%s,', fnames{:} );
      case '='
        % Comma separated list of assignments
        % Example: sql( 'UPDATE tbl SET [=#] WHERE ...', struct( 'a', 3.14, 'b', 'String', 'd', 1:5 ) )
        fnames = [fnames(:),fnames(:)]';
        list = sprintf( '%s=:%s,', fnames{:} );
      case '+'
        % 'AND' joined list of comparisations for SQL WHERE statement i.e.
        % Example: sql( 'SELECT ... WHERE [+#]', struct( 'a', 3.14, 'b', 'String' ) )
        fnames = [fnames(:),fnames(:)]';
        list = sprintf( '%s=:%s AND ', fnames{:} );
        list(end-3:end) = []; % Remaining character deleted later
      case '*'
        % For SQL CREATE statement
        % Example: sql( 'CREATE TABLE tbl ([*#])', struct( 'a', 'REAL', 'b', 'TEXT', 'ID', 'INTEGER PRIMARY KEY' ) )
        defs = struct2cell( struct_var );
        defs = [fnames(:),defs(:)]';
        list = sprintf( '%s %s,', defs{:} );
      otherwise
        error( 'MKSQLITE:SQL:UNKMODE', 'Unknown parameter <mode>' );
  end
  list(end) = [];
end