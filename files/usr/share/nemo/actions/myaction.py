#! /usr/bin/python2 -OOt

import sys

command = sys.argv[0]
print "Running " + command
print "With the following arguments:"
for arg in sys.argv:
    if command == arg:
        continue
    else:
        print arg

