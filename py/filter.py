#!/usr/bin/env python

import sys

def filter_lines():
    f = file(sys.argv[1],'r')
    iHide = 0
    replacement = '' #'\n'
    
    for line in f:
        if line.find('\\hide') >= 0:
            iHide = iHide - 1
            sys.stdout.write(replacement)
            continue

        if line.find('\\endhide') > 0 and iHide < 0:
            iHide += 1
            sys.stdout.write(replacement)
            continue

        if line.find('\\stophide') > 0:
            iHide = 0
            sys.stdout.write(replacement)
            continue

        if iHide >= 0:
            sys.stdout.write(line)
        else:
            sys.stdout.write(replacement)
            
    f.close()


filter_lines()

