fprintf ('compiling mksqlite...\n');

mksqlite_compile_currdir = pwd;
cd (fileparts(mfilename('fullpath')));

mksqlite_compile_subwcrev = [getenv('ProgramFiles') '\TortoiseSVN\bin\SubWCRev.exe'];

if exist(mksqlite_compile_subwcrev, 'file')
    fprintf ('svn revision info:\n');
    system(['"' mksqlite_compile_subwcrev '" ' pwd ' svn_revision.tmpl svn_revision.h']);
else
    if ~exist('svn_revision.h','file')
        copyfile('svn_revision.dummy','svn_revision.h');
    end
end

if ispc
  mex -output mksqlite -DNDEBUG#1 -DSQLITE_ENABLE_RTREE=1 -O mksqlite.cpp sqlite3.c user32.lib advapi32.lib
else
  mex -output mksqlite -DNDEBUG#1 -DSQLITE_ENABLE_RTREE=1 -O mksqlite.cpp sqlite3.c -ldl
end

cd (mksqlite_compile_currdir);

clear mksqlite_compile_currdir;
clear mksqlite_compile_subwcrev;

fprintf ('completed.\n');
