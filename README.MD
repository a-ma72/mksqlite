# *mksqlite* documentation
*mksqlite* is an open source project to implement an interface between MATLAB(R) and SQLite.
You can use the command `mksqlite` to benefit from all features of SQLite.  

Simply enter the command `mksqlite('SELECT * FROM database')` and you get an array of structures with the results of the query.

For a more detailed documentation visit the project home page: http://mksqlite.sourceforge.net.

## Why using *mksqlite*?
Since MATLAB(R) version 2016a the is a `sqlite(...)` function for easy access to SQLite databases.  
*mksqlite* on the other hand serves the facility to...
* Handle BLOB data type (uint8 array)
* Save numerical arrays as (compressed) BLOB (shadowed)
* Save complex data structures (struct, cell, ...) as BLOB (shadowed)
* Define your own SQLite functions in terms of MATLAB(R) function callbacks
Additionally *mksqlite* has some nice features like for example bulk insertion and named parameter binding:
```
clear s
s(1).Name = 'Triangle';
s(1).Type = 'Shape';
s(1).ID   = 145;
s(2).Name = 'Rectangle';
s(2).Type = 'Shape';
s(2).ID   = 344;

mksqlite( 'BEGIN' );
mksqlite( 'INSERT INTO tbl (ID, Type, name) VALUES (:ID, :Type; :Name)', s );
mksqlite( 'COMMIT' );
```
## Prerequisites
*mksqlite* is build on top of SQLite and tested on various MATLAB(R) versions (R13SP1, R14SP2, 2006a, 2008b, 2009a, 2010a..2018b).
You can use *mksqlite* on Windows, Linux and MAC OS-X systems.  
The precompiled windows binaries are available from https://sourceforge.net/projects/mksqlite/files.  
Unzip the contents of the ZIP file into a directory and insert this directory to the MATLAB search path.
```
addpath('path-to-mksqlite');
```
You can create other versions of *mksqlite* via the source package.
## Using *mksqlite* source code
You can find the sourcecode of *mksqlite* also at https://sourceforge.net/projects/mksqlite/files.
The Microsoft Visual C++ Compiler version 2003, 2005, 2010..2017 is needed to compile *mksqlite* on windows systems.
Just use the MATLAB(R) script "buildit.m" to compile them to a MEX-DLL.
## Online help
There is a small help available, just type `help mksqlite` in the MATLAB(R) command window.  
For detailed description and lots of samples watch the compiled help file (.CHM) in the /doc folder.
## Usage
You can use *mksqlite* like any other M-Script command:
```
dbid = mksqlite([dbid, ] SQLQuery [, Argument])
```
The parameter dbid is optional and is needed only if you want
to use several data bases at the same time. If dbid was omitted,
then the data base id 1 is used. 
Function calls:
```
mksqlite('open', 'databasefile' [,'ro'|'rw'|'rwc'] [,'single'|'multi'|'serial'])
```
or
```
dbid = mksqlite(0, 'open', 'databasefile' [,...])
```
Opens the database "databasefile". If there is no such database, then it will
be created. 
If you use the dbid of an already opened database, than this db will be
closed first. If you use the dbid 0, then the next free dbid will be
used and returned by the function call.
```
mksqlite('close')
```
or
```
mksqlite(dbid, 'close')
```
or
```
mksqlite(0, 'close')
```
Closes the databasefile. If you use dbid 0, then all databases will be closed.
```
mksqlite('SQL-Query')
```
or
```
mksqlite(dbid, 'SQL-Query')
```
Executes the query and returns the results..
## FAQ
> My Windows MATLAB(R) doesn't find *mksqlite*.

Be sure that you have placed _mksqlite.mex*_ inside the MATLAB(R) path
or in the current directory.
> MATLAB(R) shows "Attempt to execute SCRIPT *mksqlite* as a function"

It seems that you are using an older version of MATLAB(R).
Try to rename the file "mksqlite.mexw32" to "mksqlite.dll"
or build a new version of *mksqlite* from the source package.
> Does *mksqlite* work on 64Bit MATLAB(R), on Solaris, Linux, OS-X, ...

Yes, there are 64Bit versions inside the binary package.
(mksqlite.mexw64, mksqlite.mexa64)
> How can I rebuild *mksqlite*?

You need MATLAB(R) and a compiler. The compiler must be useable to build
MATLAB(R) MEX-extensions. You can use on of the Microsoft Compiler
on Windows, or a GNU compiler on other Systems.
You will find the MATLAB(R) script "buildit.m" inside the source package. Just
start it from the MATLAB(R) prompt.
> The SQLite database seems to be very slow. Can I speedup my operations?

SQLite uses a journal file for every write operation. This slows down any
change on the database. Try to use transactions for a block of change operations
or change the journal functions with SQLite pragma commands.
You can find more informations inside of the SQLite documentation.
> How can I catch *mksqlite* errors?

You can use the standard MATLAB(R) try ... catch functions.
> What does *mksqlite* costs?

You can use *mksqlite* without any cost.
But if you mean that this software is worth a donation,
Just send me a mail with your offer.
> I need a new functionality, what's about new functions?

If you have implemented a new function or have an idea how something can
made better, just send me an email. Maybe I will implement this function
or I can give you a hint how you can work with the current functionality.
> Can I do something to help develop *mksqlite*?

Sure, documentation, translation, extensions, give me notes about errors, ...
Or just give me a note how do you use *mksqlite*.
## License
*mksqlite* is distributed under the BSD-2 license (see LICENSE file)
## Contact
Andreas Martin, andimartin@users.sourceforge.net  
Martin Kortmann, mail@kortmann.de
