files = dir( '*.m' );

if exist( 'html', 'dir' )
    rmdir( 'html', 's' );
end

for i = 1:numel(files)
    close all
    if ~isempty( strfind( files(i).name, 'sqlite_' ) ) ...
       && ~strcmpi( files(i).name, 'sqlite_test_load_extension' )
        publish( files(i).name, 'imageFormat', 'jpg' )
    end
end

delete( 'html/*.png' );
