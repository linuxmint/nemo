#!/usr/bin/python3 -OOt

import argparse
from itertools import chain
from subprocess import call

parser = argparse.ArgumentParser()
parser.add_argument('filelist', help='list of absolute paths separated by /#/')
args = parser.parse_args()

command = ['xdg-email'] + list(chain.from_iterable(('--attach', arg) for arg in args.filelist.split('/#/')))
call(command)
