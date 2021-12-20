#!/usr/bin/env python3

import sys

def filter_lines():
    with open( sys.argv[1], 'rt' ) as f:
        iHide = 0
        replacement = '' #'\n'
        
        for line in f:
            if line.find( r'\hide' ) >= 0:
                iHide = iHide - 1
                sys.stdout.write( replacement )
                continue

            if line.find( r'\endhide' ) > 0 and iHide < 0:
                iHide += 1
                sys.stdout.write( replacement )
                continue

            if line.find( r'\stophide' ) > 0:
                iHide = 0
                sys.stdout.write( replacement )
                continue

            if iHide >= 0:
                sys.stdout.write( line )
            else:
                sys.stdout.write( replacement )
            

filter_lines()

