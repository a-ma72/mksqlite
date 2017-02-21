%MKSQLITE A MATLAB interface to SQLite
%  SQLite is an embedded SQL Engine, which can be used to access SQL
%  databases without a server.  MKSQLITE offers an interface to this
%  database engine.
%
% General calling sequence:
%  dbid = mksqlite([dbid, ] SQLCommand [, Argument])
%   The parameter dbid is optional and is only necessary if one is
%   working with multiple databases at once.  If dbid is left out, then
%   database number 1 is used.
%
% Function Calls:
%  mksqlite('open', 'dataBaseName')
% or
%  dbid = mksqlite(0, 'open', 'dataBaseName')
% Opens the database with name "dataBaseName".  If the database does
% not exist, an empty one will be created.  If a dbid is given that is
% already open, it will be closed before opening again.  Providing a dbid
% of 0 will return the next free dbid.
%
%  mksqlite('close')
% or
%  mksqlite(dbid, 'close')
% or
%  mksqlite(0, 'close')
% Closes the database file. If a dbid is provided, the corresponding
% database is closed.  For dbid 0, all open databases are closed.
%
%  mksqlite('version mex')                 (1
% or
%  version = mksqlite('version mex')       (2
% Returns the version of mksqlite in the output (1), or as a string (2).
%
%  mksqlite('version sql')                 (1
% or
%  version = mksqlite('version sql')       (2
% Returns the version of the SQLite Engine in the output (1), or as a
% string (2).
%
%  mksqlite('SQL-Command')
% or
%  mksqlite(dbid, 'SQL-Command')
% Carries out the given "SQL-Command"
%
% Example:
%  mksqlite('open', 'testdb.db3');
%  result = mksqlite('select * from testtable');
%  mksqlite('close');
% Reads all fields from table "testtable" from the database "testdb.db3"
% into the variable "result"
%
% Example:
%  mksqlite('open', 'testdb.db3')
%  mksqlite('show tables')
%  mksqlite('close')
% Shows all tables in the database "testdb.db3"
%
% =====================================================================
% Parameter binding:
% The SQL syntax allows the use of parameters, which are identified
% by placeholders, and then filled with contents by trailing arguments.
% Allowed placeholders in SQLlite are: ?, ?NNN, :NNN, $NAME, @NAME
% A placeholder can only stand for one value, not for a command,
% split-name, or table, etc.
%
% Example:
%  mksqlite( 'insert firstName, lastName, city into AddressBook values (?,?,?)', ...
%            'Paul', 'Meyer', 'Munich' );
%
% Instead of a listing of arguments, a cell array can be provided that
% contains the arguments.
% If fewer arguments are given then required, the remaining parameters are
% filled with NULLs.  If more arguments are given than necessary, the
% function reports an error.
% If it is intended, that implicit calls with the same command and the remaining
% arguments shall be done, so called parameter wrapping must be activated:
% mksqlite('param_wrapping', 0|1)
% An argument may be a real value (scalar or array) or a string.
% Non-scalar values are treated as a BLOB (unit8) SQL datatype.
% ( BLOB = (B)inary (L)arge (OB)ject) )
%
% Example:
%  data = rand(10,15);
%  mksqlite( 'insert data into MyTable values (?)', data );
%  query = mksqlite( 'select data from MyTable' );
%  data_sql = typecast( query(1).data, 'double' );
%  data_sql = reshape( data_sql, 10, 15 );
%
% BLOBs are always stored as a vector of uint8 values in the database.
% In order to retrieve the original format (for example, double) and
% dimensions of the matrix, explict typecast() and reshape() functions
% must be used. (Refer to the example "sqlite_test_bind.m")
% Optionally this information (type) can be stored after the BLOB.
% The indicated post-processing is then no longer necessary, but the
% database is then no longer compatible with other software!
% The typecasting conversion can be activated/deactivated with:
%
%   mksqlite( 'typedBLOBs', 1 ); % activate
%   mksqlite( 'typedBLOBs', 0 ); % deactivate
%
% (see also the example "sqlite_test_bind_typed.m")
% Type conversion only works with numeric arrays and vectors.  structs,
% cell arrays and complex data must be converted beforehand.  Matlab
% can do this conversion through undocumented functions:
% getByteStreamFromArray() and getArrayFromByteStream().
% This functionality is activated by following command:
%
%   mksqlite ( 'typedBLOBs', 2); % extended activation
%
% The data in a BLOB is stored either uncompressed (standard) or
% compressed.  Automatic compression of the data is only necessary for
% typed BLOBs, but must be activated:
%
%   mksqlite( 'compression', 'lz4', 9 ); % activate maximal compression (0=off)
%
% (See also examples "sqlite_test_bind_typed_compressed.m" and
% "sqlite_test_md5_and_packaging.m")
%
% The compression uses BLOSC (http://blosc.pytabales.org/trac)
% After compression, the data is unpacked and compared with the original.
% If there is a difference, an error report is given.  If this
% functionality is not desirable, it can be deactivated (data is
% stored without verification).
%
%   mksqlite( 'compression_check', 0 ); % deactive the check (1=activate)
%
%
% Compatibility:
%  Stored compressed blobs cannot be retrieved with older versions of mqslite,
%  this will trigger an error report.  In contrast, uncompressed BLOBS can be
%  retrieved with older versions.  Of course BLOBs stored with older versions
%  can be retrieved with this version.
%
% Remarks on compression rate:
%   The achievable compression rates depend strongly on the contents of the
%   variables.  Although BLOSC is equipped to handle numeric data, its
%   performance on randomized numbers (double) is poor (~95%).  If there are
%   many identical values, for example from quantization, the compression rate
%   is markedly improved.
%
% Further compression methods:
% "QLIN16":
% QLIN16 is a lossy compression method.  The data is linearly discretized
% to 65529 steps and stored as 16-bit values.  Zero, as well as Infinity and Nan
% can also be used, as they are stored as special values (65529...65535).
% Differing compression rates are not supported, so this compressor should
% always be set to 1.
%
% "QLOG16":
% Works like QLIN16, except that the quantization uses logarithmic
% scaling, therefore storage of negative values is not allowed, but
% NULL, Nan, and infinity are still accepted.  Similarly, differing
% compression rates are not supported, so should always be set to 1.
%
% =======================================================================
%
% Control the format of result for queries
%
% Beside the described calling convention, one can retrieve two further
% often needed results:
% 1. The row count (rowcount)
% 2. The original table column names (colnames)
% Both results are given with the common call already:
% [result,rowcount,colnames] = mksqlite(...)
%
% Per default an array of structs will be returned for table queries.
% You can decide between three differet kinds of result types:
% (0) array of structs (default)
% (1) struct of arrays
% (2) cell matrix
% You can change the default setting (n=0) with following call:
% mksqlite( 'result_type', n );
% (see sqlite_test_result_types.m)
%
% =======================================================================
%
% Extra SQL functions:
% mksqlite offers additional SQL functions besides the known "core functions"
% like replace, trim, abs, round, ...
% This version offers 10 additional functions:
%   * pow(x,y):
%     Calculates x raised to exponent y.  If the result is not representable
%     the return value is NULL.
%   * lg(x):
%     Calculates the decadic logarithm of x. If the result is not representable
%     the return value is NULL.
%   * ln(x):
%     Calculates the natural logarithm of x. If the result is not representable
%     the return value is NULL.
%   * exp(x):
%     Calculates the exponential function with e raised x. If the result is not representable
%     the return value is NULL.
%   * regex(str,pattern):
%     Finds the first substring of str that matches the regular expression pattern.
%   * regex(str,pattern,repstr):
%     Finds the first substring of str, that  matches the regular expression pattern.
%     The return value replaces the value with repstr.
%     (mksqlite uses the perl-compatible regex engine "DEELX".
%     Further information can be found at www.regexlab.com or wikipedia)
%   * md5(x):
%     Computes and returns the MD5 hash
%   * bdcpacktime(x):
%     Computes the required time for the actual compression of x.
%   * bdcunpacktime(x):
%     Equivalent to bdcpacktime(x). (but for unpacking?)
%   * bdcratio(x):
%     Computes the compression factor for x, using the currently set
%     compression method.
%
% The use of regex in combination with parameters offers an
% especially efficient possibility for complex queries on text contents.
%
% Example:
%   mksqlite( [ 'SELECT REGEX(field1,"[FMA][XYZ]MR[VH][RL]") AS re_field FROM Table ', ...
%               'WHERE REGEX(?,?,?) NOT NULL' ], 'field2', '(\\d{5})_(.*)', '$1' );
%
% (also see test_regex.m for further examples...)
%
% =======================================================================
%
% Application-defined functions:
% You can register your own MATLAB functions as SQL functions with one of
% the following calls:
%
%   mksqlite( 'create function', <name>, function_handle );
%   mksqlite( 'create aggregation', <name>, step_function_handle, final_function_handle );
%
% So you can access your MATLAB code from within SQL queries.
%
%
% (c) 2008-2017 by Martin Kortmann <mail@kortmann.de>
%                  Andreas Martin  <andimartin@users.sourceforge.net>
%

