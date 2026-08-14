function mksqlite_ci_smoke
% MKSQLITE_CI_SMOKE  Minimal, self-contained smoke test for CI.
%
% Exercises every native module the build links in, so a broken or
% mis-linked binary fails loudly:
%   * SQLite core        - open / create / insert / select round-trip + bind
%   * md5/                - MD5() SQL function against a known reference hash
%   * c-blosc/ + lz4      - typed-BLOB compression round-trip (blosclz/lz4/lz4hc)
%   * deelx (regex)       - REGEX() SQL function
%
% Runs entirely in an in-memory database (no files, no plotting), errors
% on the first failed assertion so run-matlab-command returns non-zero.

    fprintf( 'mksqlite CI smoke test\n' );

    v = mksqlite( 'version mex' );
    assert( ischar( v ) && ~isempty( v ), 'version mex returned nothing' );
    fprintf( '  version mex: %s\n', v );

    mksqlite( 'open', ':memory:' );
    cleanup = onCleanup( @() mksqlite( 'close' ) );

    % --- SQLite core: create / insert / select round-trip ---
    mksqlite( 'CREATE TABLE t (a, b)' );
    mksqlite( 'INSERT INTO t (a, b) VALUES (?, ?)', 42, 'hello' );
    r = mksqlite( 'SELECT a, b FROM t' );
    assert( r(1).a == 42, 'core: numeric round-trip failed' );
    assert( strcmp( r(1).b, 'hello' ), 'core: text round-trip failed' );
    fprintf( '  core round-trip OK\n' );

    % --- md5/ module: MD5() against the canonical reference hash ---
    q = mksqlite( 'SELECT MD5(?) AS h', 'abc' );
    assert( strcmpi( q.h, '900150983cd24fb0d6963f7d28e17f72' ), ...
            'MD5(''abc'') mismatch (got %s)', q.h );
    fprintf( '  MD5 OK\n' );

    % --- c-blosc/ + lz4: typed-BLOB compression round-trip ---
    % Only the codecs actually compiled into the mex are exercised.
    mksqlite( 'typedBLOBs', 2 );        % typed BLOBs with compression
    mksqlite( 'compression_check', 1 ); % verify compressed data on write
    mksqlite( 'CREATE TABLE blob_t (id, data)' );
    payload = cumsum( ones( 1000, 1 ) ); % deterministic, compresses well
    compressors = { 'blosclz', 'lz4', 'lz4hc' };
    for i = 1:numel( compressors )
        mksqlite( 'compression', compressors{i}, 9 );
        mksqlite( 'INSERT INTO blob_t (id, data) VALUES (?, ?)', i, payload );
        got = mksqlite( 'SELECT data FROM blob_t WHERE id = ?', i );
        assert( isequal( got.data, payload ), ...
                'blosc: round-trip failed for compressor %s', compressors{i} );
        fprintf( '  blosc %-8s OK\n', compressors{i} );
    end
    mksqlite( 'compression', 'blosclz', 0 ); % leave compression off again

    % --- deelx: REGEX() SQL function ---
    q = mksqlite( 'SELECT REGEX(?, ?) AS m', 'abc123def', '[0-9]+' );
    assert( strcmp( q.m, '123' ), 'REGEX match mismatch (got %s)', q.m );
    fprintf( '  regex OK\n' );

    fprintf( 'mksqlite CI smoke test PASSED\n' );
end
