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

warning( 'buildit will be deprecated in future, think about using CMake instead! ' )

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

lz_src = 'c-blosc/internal-complibs/lz4-1.8.1.2/';
blosc  = [' c-blosc/blosc/blosc.c', ...
          ' c-blosc/blosc/blosclz.c', ...
          ' c-blosc/blosc/shuffle.c', ...
          ' c-blosc/blosc/shuffle-generic.c', ...
          ' c-blosc/blosc/bitshuffle-generic.c', ...
          ' c-blosc/blosc/fastcopy.c', ...
          [' ', lz_src, 'lz4.c'], ...
          [' ', lz_src, 'lz4hc.c'] ];

md5    = [' md5/md5.c '];

uuid   = [];

modules = [sqlite, blosc, md5, uuid];


% Save the current directory and change to sources
% folder ( = location of this script )
mksqlite_compile_currdir = pwd;
cd (fileparts(mfilename('fullpath')));

% Create object folder for compiled modules
outdir = './obj';
if exist( outdir, 'file' ) ~= 7
  mkdir( outdir );
end

copyfile( [lz_src, 'lz4.h'], 'c-blosc/blosc' );
copyfile( [lz_src, 'lz4hc.h'], 'c-blosc/blosc' );

% set macros
buildargs = ['-DSQLITE_ENABLE_RTREE=1 -DSQLITE_THREADSAFE=2 -DHAVE_LZ4'];
if buildrelease
    buildargs = [buildargs, ' -DNDEBUG -O '];
else
    buildargs = [buildargs, '-UNDEBUG -g -v '];
end

% additional libraries:
% (libut for ctrl-c detection, libdl for dynamic linkage on linux machines)
if ispc
    buildargs = [buildargs ' user32.lib advapi32.lib libut.lib'];
else
    buildargs = [buildargs ' -ldl -lut'];
end

% Get computer architecture
arch = computer('arch');

switch arch
  case {'glnx86', 'glnxa32', 'glnxa64'}
    % Enable C++11 standard (gcc 4.4.7)
    buildargs = [ buildargs, ' -', arch ];
    compvars  = ' CFLAGS="\$CFLAGS" CXXFLAGS="\$CXXFLAGS -std=gnu++0x" ';
  case {'win32', 'win64'}
    buildargs = [ buildargs, ' -', arch ];
    compvars  = ' LINKFLAGS="$LINKFLAGS" COMPFLAGS="$COMPFLAGS" ';
  case {'maci64'}
    if 0
        % Perhaps someone can resolve this to work anytime...
        % (Mac OS 10.11.2 and Xcode 7.1.1)
        buildargs = strrep( buildargs, '-DNDEBUG#1', '-DNDEBUG=1' );
        compvars = ['CXX="/usr/bin/clang++" ',                 ... % Override C++ compiler
                    'CXXFLAGS="-std=c++11 -stdlib=libc++ ',    ... % Override C++ compiler flags
                              '-fno-common -fexceptions ',     ...
                              '-v -Winvalid-source-encoding ', ...
                              '-arch x86_64 " ',               ...
                    'CC="/usr/bin/clang" ',                    ... % Override C compiler
                    'CFLAGS="-arch x86_64 " '];                    % Override C compiler flags
    else
        % Precompile C-modules for mex
        % zznaki proposal, 2016-02-03 ( OSX El Capitan version 10.11.3, Xcode Version 7.2.1 (7C1002) )
        compvars = '';
        buildargs = strrep( buildargs, '-DNDEBUG#1', '-DNDEBUG=1' );
        for srcFile = strsplit( strtrim(modules) )
            clangStr = ['clang -o ', strrep( srcFile{1}, '.c', '.o' ),  ...
                        ' -c -arch x86_64 ', strrep( buildargs, ' -ldl ', ' ' ),' ', srcFile{1}];
            disp( clangStr )
            [status,result] = system( clangStr, '-echo' );
            assert( status == 0 );
        end
        modules = strrep( modules, '.c', '.o' );
    end
end


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

% compile C sources
eval (['mex -c -outdir ./obj ', compvars, buildargs, modules]);
obj = dir( './obj/*.o*' );
% obj = fullfile( './obj/', {obj.name} );  % Only available in newer MATLAB versions
obj = cellfun( @(c) fullfile( './obj/', c ), {obj.name}, 'UniformOutput', 0 );
obj = obj(:)';
obj(2,:) = deal( {' '} );
% compile C++ source and link
eval (['mex -output mksqlite ', compvars, buildargs, ' mksqlite.cpp ', obj{:}]);

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
copyfile('sql_builtin_functions.hpp',  srcdir);
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
