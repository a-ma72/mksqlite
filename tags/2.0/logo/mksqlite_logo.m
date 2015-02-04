%% Creating the MATLAB Logo
% This example shows how to create and display the MATLAB(R) logo.

% Copyright 2014 The MathWorks, Inc.

%%
% Use the |membrane| command to generate the surface data for the logo.

L = 160*membrane(1,100);

%%
% Create a figure and an axes to display the logo.  Then, create a surface
% for the logo using the points from the |membrane| command. Turn off the
% lines in the surface.

f = figure;
ax = axes;

s = surface(L);
set( s, 'EdgeColor', 'none' );
view(3)

%%
% Adjust the axes limits so that the axes are tight around the logo.

set( ax, 'XLim', [1 201] );
set( ax, 'YLim', [1 201] );
set( ax, 'ZLim', [-53.4 160] );


%%
% Adjust the view of the logo using the camera properties of the axes.
% Camera properties control the view of a three dimensional scene like a
% camera with a zoom lens.
set( ax, 'CameraPosition', [-1440 -315 885] );
set( ax, 'CameraTarget',   [ 100 100 50] );
set( ax, 'CameraUpVector', [0 0 1] );
set( ax, 'CameraViewAngle', 10 );

%%
% Change the position of the axes and the _x_, _y_, and _z_ aspect ratio to
% fill the extra space in the figure window.

set( ax, 'Position', [0 0 1 1] );
set( ax, 'DataAspectRatio', [1 1 .9] );

%%
% Create lights to illuminate the logo.  The light itself is not visible
% but its properties can be set to change the appearance of any patch or
% surface object in the axes.

l1 = light;
set( l1, 'Position', [160 400 80] );
set( l1, 'Style', 'local' );
set( l1, 'Color', [0 0.8 0.8] );
 
l2 = light;
set( l2, 'Position', [.5 -1 .4] );
set( l2, 'Color', [0.8 0.8 0] );

%%
% Change the color of the logo.

set( s, 'FaceColor', [0.9 0.2 0.2] );

%%
% Use the lighting and specular (reflectance) properties of the surface to
% control the lighting effects.

set( s, 'FaceLighting', 'gouraud' );
set( s, 'AmbientStrength', 0.3 );
set( s, 'DiffuseStrength', 0.6 ); 
set( s, 'BackFaceLighting', 'lit' );

set( s, 'SpecularStrength', 1 );
set( s, 'SpecularColorReflectance', 1 );
set( s, 'SpecularExponent', 7 );

%%
% Turn the axis off to see the final result.

axis off
set( f, 'Color', 'black' );
%view( [-75, 30] );
