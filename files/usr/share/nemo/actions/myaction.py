#! /usr/bin/python -OOt

import sys

command = sys.argv[0]
print "Running " + command
print "With the following items:"
for arg in sys.argv:
    if command == arg:
        continue
    else:
        print arg

