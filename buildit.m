% BUILDIT - builds mksqlite
%
% possible commands:
% - buildit                     : builds the release of mksqlite
% - buildit release             : builds the release of mksqlite
% - buildit debug               : builds the debug version of mksqlite
% - buildit packrelease 1.x     : build a release and src package of version 1.x

%%
function buildit(varargin)
clc

switch nargin

    case 0
        buildmksqlite('release');

    case 1

        switch varargin{1}

            case {'release' 'debug'}
                buildmksqlite(varargin{1});

            otherwise
                help(mfilename);
        end

    case 2

         switch varargin{1}

            case {'packrelease'}
%                 buildmksqlite('release');
                packmksqlite(varargin{2});

            otherwise
                help(mfilename);
        end

    otherwise
        help(mfilename);
        return;
end

%% do the build...
function buildmksqlite (buildtype)

% check the argument
if ~exist('buildtype', 'var') || isempty(buildtype) || ~ischar(buildtype)
    error ('wrong or empty argument');
end

switch buildtype

    case 'release'
        buildrelease = true;

    case 'debug'
        buildrelease = false;

    otherwise
    error ('wrong or empty argument');
end

fprintf ('compiling %s version of mksqlite...\n', buildtype);

sqlite = [' sqlite/sqlite3.c '];

blosc  = [' blosc/blosc.c', ...
          ' blosc/blosclz.c', ...
          ' blosc/shuffle.c', ...
          ' blosc/lz4.c', ...
          ' blosc/lz4hc.c'];

md5    = [' md5/md5.c '];

uuid   = [];

modules = [sqlite, blosc, md5, uuid];


% get the mex arguments
if buildrelease
    buildargs = ['-DNDEBUG#1 -DSQLITE_ENABLE_RTREE=1 -DSQLITE_THREADSAFE=2 -DHAVE_LZ4 -O '];
else
    buildargs = ['-UNDEBUG -DSQLITE_ENABLE_RTREE=1 -DSQLITE_THREADSAFE=2 -DHAVE_LZ4 -g -v '];
end

% additional libraries
if ispc
    buildargs = [buildargs ' user32.lib advapi32.lib'];
else
    buildargs = [buildargs ' -ldl'];
end

if ~exist( 'arch', 'var' )
    buildargs = [ buildargs, ' -', computer('arch') ];
else
    buildargs = [ buildargs, ' -', arch ];
end

switch computer('arch')
  case {'glnx86', 'glnxa32', 'glnxa64'}
    % Enable C++11 standard (gcc 4.4.7)
    buildargs = [ buildargs, ' CFLAGS="\$CFLAGS" CXXFLAGS="\$CXXFLAGS -std=gnu++0x"';];
  case {'win32', 'win64'}
    buildargs = [ buildargs, ' LINKFLAGS="$LINKFLAGS" COMPFLAGS="$COMPFLAGS"';];
  case {'maci64'}
    % todo: which settings for macintosh (I'm not able to test...)?
    % (see also: http://libcxx.llvm.org/ )

    buildargs = [ buildargs, ' LINKFLAGS="$LINKFLAGS -stdlib=libc++" ', ...
                             ' CXXFLAGS="$CXXFLAGS -std=c++11 -fno-common -fexceptions"';];
end


% save the current directory and get the version information
mksqlite_compile_currdir = pwd;
cd (fileparts(mfilename('fullpath')));

if 0  % set to 1 if you prefer using Windows and TortoiseSVN only...
    mksqlite_compile_subwcrev = [getenv('ProgramFiles') '\TortoiseSVN\bin\SubWCRev.exe'];

    if exist(mksqlite_compile_subwcrev, 'file')
        fprintf ('svn revision info:\n');
        system(['"' mksqlite_compile_subwcrev '" "' pwd '" svn_revision.tmpl svn_revision.h']);
    else
        if ~exist('svn_revision.h','file')
            copyfile('svn_revision.dummy','svn_revision.h');
        end
    end
else
    if ispc
        svnversion_cmd = ['"', getenv('ProgramFiles') '\TortoiseSVN\bin\svnversion.exe"'];
        if ~exist( svnversion_cmd, 'file' )
            svnversion_cmd = 'svnversion.exe'; % try if svnversion in in PATH
        end
    else
        svnversion_cmd = 'svnversion';
    end

    [status, str_revision] = system( [svnversion_cmd, ' -n'] );
    if status ~= 0  % || 1
        copyfile('svn_revision.dummy','svn_revision.h');
    else
        fid = fopen( 'svn_revision.h', 'w' );
        if str_revision(end) == 'M'
            str_modified = ' (modified)';
            str_revision(end) = [];
        else
            str_modified = '';
        end

        try
          fprintf( fid, ['#ifndef __SVN_REVISION_H\n', ...
                         '#define __SVN_REVISION_H\n\n', ...
                         '#define SVNREV "build: %s%s"\n\n', ...
                         '#endif // __SVN_REVISION_H'], ...
                         str_revision, str_modified );
        catch ex
            fclose( fid );
            throw ex
        end

        fclose( fid );
    end
end

% do the compile via mex
switch computer('arch')
  case {'maci64'}
    % Pass precompiled modules to mex
    % (clang -c -DNDEBUG#1 -DSQLITE_ENABLE_RTREE=1 -DSQLITE_THREADSAFE=2 -DHAVE_LZ4 *.c -ldl -arch x86_64)
    [status,result] = system( ['clang -c -ldl -arch x86_64 ', buildargs, modules], '-echo' );
    assert( status == 0 );
    eval (['mex -output mksqlite ', buildargs, ' mksqlite.cpp ', strrep( modules, '.c', '.o' )]);

  otherwise
    eval (['mex -output mksqlite ', buildargs, ' mksqlite.cpp ', modules]);
end

% back to the start directory
cd (mksqlite_compile_currdir);

fprintf ('completed.\n');

%%
function packmksqlite(versionnumber)

relmaindir = 'releases';

% check the argument
if ~exist('versionnumber', 'var') || isempty(versionnumber) || ~ischar(versionnumber)
    error ('wrong or empty argument');
end

% create the directories
if ~exist(relmaindir, 'dir')
    mkdir(relmaindir);
    if ~exist(relmaindir, 'dir')
       error (['cannot create directory ' relmaindir]);
    end
end

reldir = [relmaindir filesep 'mksqlite-' versionnumber];
srcdir = [relmaindir filesep 'mksqlite-' versionnumber '-src'];

if exist (reldir, 'file')
    error(['there is already a directory or file named ' reldir]);
end
if exist (srcdir, 'file')
    error(['there is already a directory or file named ' srcdir]);
end

if ~exist(reldir, 'dir')
    mkdir(reldir);
    if ~exist(reldir, 'dir')
       error (['cannot create directory ' reldir]);
    end
end
if ~exist(srcdir, 'dir')
    mkdir(srcdir);
    if ~exist(srcdir, 'dir')
       error (['cannot create directory ' srcdir]);
    end
end


fprintf ('packing mksqlite release files\n');

% copy files
% release
copyfile('README.TXT',              reldir);
copyfile('Changelog.txt',           reldir);
copyfile('mksqlite.m',              reldir);
copyfile('mksqlite_en.m',           reldir);
copyfile('sql.m',                   reldir);
copyfile('doxy/chm/mksqlite*.chm',  reldir);

% x86 32-bit version (MSVC 2010 / Win7) / MATLAB Version 7.7.0.471 (R2008b)
if exist( 'mksqlite.mexw32', 'file' )
  copyfile('mksqlite.mexw32', reldir);
end

% x86 64-bit version (MSVC 2010 / Win7) / MATLAB Version 7.13.0.564 (R2011b)
if exist( 'mksqlite.mexw64', 'file' )
  copyfile('mksqlite.mexw64', reldir);
end

% x86 64-bit version (gcc 4.1.2 20080704 / Red Hat 4.1.2-52)
% MATLAB Version 7.13.0.564 (R2011b)
if exist( 'mksqlite.mexa64', 'file' )
  copyfile('mksqlite.mexa64', reldir);
end

% Mac OSX 10.9.2, 64 bit
% MATLAB Version R214a, compiled by Stefan Balke
if exist( 'mksqlite.mexmaci64', 'file' )
  copyfile('mksqlite.mexmaci64', reldir);
end

copyfile('docs/',                  [reldir '/docs']);
copyfile('test/',                  [reldir '/test']);
copyfile('doxy/chm/mksqlite*.chm', [reldir '/docs']);


% source
copyfile('README.TXT',              srcdir);
copyfile('Doxyfile',                srcdir);
copyfile('mksqlite.dox',            srcdir);
copyfile('Changelog.txt',           srcdir);
copyfile('buildit.m',               srcdir);
copyfile('mksqlite.m',              srcdir);
copyfile('mksqlite_en.m',           srcdir);
copyfile('sql.m',                   srcdir);
copyfile('mksqlite.cpp',            srcdir);
copyfile('config.h',                srcdir);
copyfile('global.hpp',              srcdir);
copyfile('heap_check.hpp',          srcdir);
copyfile('locale.hpp',              srcdir);
copyfile('number_compressor.hpp',   srcdir);
copyfile('serialize.hpp',           srcdir);
copyfile('sql_interface.hpp',       srcdir);
copyfile('sql_user_functions.hpp',  srcdir);
copyfile('typed_blobs.hpp',         srcdir);
copyfile('utils.hpp',               srcdir);
copyfile('value.hpp',               srcdir);
copyfile('sqlite/',                [srcdir '/sqlite']);
copyfile('blosc/',                 [srcdir '/blosc']);
copyfile('deelx/',                 [srcdir '/deelx']);
copyfile('md5/',                   [srcdir '/md5']);
copyfile('docs/',                  [srcdir '/docs']);
copyfile('test/',                  [srcdir '/test']);
copyfile('py/',                    [srcdir '/py']);
copyfile('logo/*.png',             [srcdir '/logo']);
copyfile('doxy/*.html',            [srcdir '/doxy/']);
copyfile('doxy/*.dox',             [srcdir '/doxy/']);
copyfile('svn_revision.dummy',      srcdir);
copyfile('svn_revision.h',          srcdir);
copyfile('svn_revision.tmpl',       srcdir);

% x86 32-bit version (MSVC 2010 / Win7) / MATLAB Version 7.7.0.471 (R2008b)
if exist( 'mksqlite.mexw32', 'file' )
  copyfile('mksqlite.mexw32', srcdir);
end

% x86 64-bit version (MSVC 2010 / Win7) / MATLAB Version 7.13.0.564 (R2011b)
if exist( 'mksqlite.mexw64', 'file' )
  copyfile('mksqlite.mexw64', srcdir);
end

% x86 64-bit version (gcc 4.1.2 20080704 / Red Hat 4.1.2-52)
% MATLAB Version 7.13.0.564 (R2011b)
if exist( 'mksqlite.mexa64', 'file' )
  copyfile('mksqlite.mexa64', srcdir);
end

% Mac OSX 10.9.2, 64 bit
% MATLAB Version R214a, compiled by Stefan Balke
if exist( 'mksqlite.mexmaci64', 'file' )
  copyfile('mksqlite.mexmaci64', srcdir);
end

% save the current directory
mksqlite_compile_currdir = pwd;

% create archives
cd (relmaindir);
zip (['mksqlite-' versionnumber], '*.*', ['mksqlite-' versionnumber filesep]);
zip (['mksqlite-' versionnumber '-src'], '*.*', ['mksqlite-' versionnumber '-src' filesep]);

% back to the start directory
cd (mksqlite_compile_currdir);

fprintf ('completed\n');
