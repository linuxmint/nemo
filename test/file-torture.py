#!/usr/bin/env python
#
# file-torture.py - Simple torture test for file notificatins in Nautilus
# Copyright (C) 2006 Federico Mena-Quintero
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Author: Federico Mena-Quintero <federico@novell.com>

import random
import os
import sys
import optparse
import time

output_dir = ""
random_gen = None
verbose = False

extensions = (".doc", ".gif", ".jpg", ".png", ".xls", ".odt", ".odp", ".ods", ".txt", ".zip", ".gz")

files = []
directories = []

def get_random_file_index ():
    n = len (files)
    if n == 0:
        return -1
    else:
        return random_gen.randrange (n)

def get_random_directory_index ():
    n = len (directories)
    if n == 0:
        return -1
    else:
        return random_gen.randrange (n)

def get_random_filename ():
    chars = []
    for i in range (20):
        chars.append ("abcdefghijklmnopqrstuvwxyz"[random_gen.randrange (26)])

    extension = extensions[random_gen.randrange (len (extensions))]
    filename = "".join (chars) + extension
    return filename

def get_random_path ():
    return os.path.join (output_dir, get_random_filename ())

def op_create_file ():
    filename = get_random_path ()
    files.append (filename)
    f = open (filename, "w")
    f.close ()

    if verbose:
        print 'create file %s' % filename

    return True

def op_move_file ():
    idx = get_random_file_index ()
    if idx == -1:
        return False

    new_name = get_random_path ()
    old_name = files[idx]
    os.rename (old_name, new_name)
    files[idx] = new_name

    if verbose:
        print 'rename file %s to %s' % (old_name, new_name)

    return True

def op_delete_file ():
    idx = get_random_file_index ()
    if idx == -1:
        return False

    filename = files[idx]

    os.unlink (filename)
    files.pop (idx)

    if verbose:
        print 'delete file %s' % filename

    return True

def op_write_file ():
    idx = get_random_file_index ()
    if idx == -1:
        return False

    name = files[idx]
    f = open (name, "a")
    f.write ("blah blah blah blah blah blah blah\n")
    f.close ()

    if verbose:
        print 'write to file %s' % name

    return True

def op_create_dir ():
    name = get_random_path ()
    os.mkdir (name)
    directories.append (name)

    if verbose:
        print 'create directory %s' % name

    return True

def op_move_dir ():
    idx = get_random_directory_index ()
    if idx == -1:
        return False

    new_name = get_random_path ()
    old_name = directories[idx]
    os.rename (old_name, new_name)
    directories[idx] = new_name

    if verbose:
        print 'move directory %s to %s' % (old_name, new_name)

    return True

def op_delete_dir ():
    idx = get_random_directory_index ()
    if idx == -1:
        return False

    name = directories[idx]
    os.rmdir (name)
    directories.pop (idx)

    if verbose:
        print 'delete directory %s' % name

    return True

def op_file_to_dir ():
    idx = get_random_file_index ()
    if idx == -1:
        return False

    name = files[idx]
    os.unlink (name)
    files.pop (idx)
    os.mkdir (name)
    directories.append (name)

    if verbose:
        print 'file to dir %s' % name

    return True

def op_dir_to_file ():
    idx = get_random_directory_index ()
    if idx == -1:
        return False

    name = directories[idx]
    os.rmdir (name)
    directories.pop (idx)
    f = open (name, "w")
    f.close ()
    files.append (name)

    if verbose:
        print 'dir to file %s' % name

    return True

operations = (
    op_create_file,
    op_move_file,
    op_delete_file,
    op_write_file,
    op_create_dir,
    op_move_dir,
    op_delete_dir,
    op_file_to_dir,
    op_dir_to_file,
    )

def main ():
    option_parser = optparse.OptionParser (usage="usage: %prog -o <dirname>")
    option_parser.add_option ("-o",
                              "--output", dest="output",
                              metavar="FILE",
                              help="Name of output directory")
    option_parser.add_option ("-s",
                              "--seed", dest="seed",
                              metavar="NUMBER",
                              help="Random number seed")
    option_parser.add_option ("",
                              "--no-sleep", dest="sleep_enabled", action="store_false", default=True,
                              help="Disable short sleeps between operations.  Will use a lot of CPU!")
    option_parser.add_option ("-v",
                              "--verbose", dest="verbose", action="store_true", default=False,
                              help="Enable verbose output")

    (options, args) = option_parser.parse_args ()

    if not options.output:
        print 'Please specify an output directory with "-o outputdir"'
        return 1

    sleep_enabled = options.sleep_enabled

    if len (args) != 0:
        print 'No extra arguments are supported'
        return 1

    global output_dir
    global random_gen
    global verbose

    verbose = options.verbose

    random_gen = random.Random ()
    if options.seed:
        seed = int (options.seed)
    else:
        seed = int (time.time ())

    print 'Use "--seed=%s" to reproduce this run' % seed
    random_gen.seed (seed)

    if sleep_enabled:
        print 'Using short sleeps between operations (use --no-sleep to disable)'
    else:
        print 'Disabling short sleeps between operations'

    output_dir = options.output
    try:
        os.mkdir (output_dir)
    except:
        1 # nothing

    while True:
        op = operations [random_gen.randrange (len (operations))]
        op ()
        if sleep_enabled:
            time.sleep (random_gen.random () / 100)

    return 0

if __name__ == "__main__":
    sys.exit (main ())
