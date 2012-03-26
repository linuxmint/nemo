#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  Nautilus
#
#  Copyright (C) 2000, 2001 Eazel, Inc.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this library; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Author: Darin Adler <darin@bentspoon.com>,
#

# check-strings.pl: Search for .c and .h files where someone forgot
# to put _() around a string.

# Throughout this file you will find extra \ before quote symbols
# that perl does not require. These are to appease emacs perl mode.

use diagnostics;
use strict;

# default to all the files starting from the current directory
if (!@ARGV)
  {
    @ARGV = `find . \\( -name '*.c' -o -name '*.h' \\) -print`;
  }

sub found_string;

# read in file with functions for which no translation is needed
my @no_translation_needed_functions;
open FUNCTIONS, "check-strings-functions" or die "can't open functions file";
while (<FUNCTIONS>)
  {
    chomp;
    s/(([^\\]|\\.)*)\#.*/$1/;
    s/\s*$//;
    next if /^$/;
    push @no_translation_needed_functions, $_;
  }
close FUNCTIONS;
my $no_translation_needed_function_pattern = "^(" . (join "|", @no_translation_needed_functions) . ")\$";

# read in file with patterns for which no translation is needed
my @no_translation_needed_patterns;
open STRINGS, "check-strings-patterns" or die "can't open patterns file";
while (<STRINGS>)
  {
    chomp;
    s/(([^\\]|\\.)*)\#.*/$1/;
    s/\s*$//;
    next if /^$/;
    my ($string_pattern, $file_name_pattern) = /(.+?)\s*\|\|\|\s*(.+)/;
    $string_pattern ||= $_;
    $file_name_pattern ||= ".";
    push @no_translation_needed_patterns, [$string_pattern, $file_name_pattern];
  }
close STRINGS;

FILE: foreach my $file (@ARGV)
  {
    chomp $file;
    open FILE, $file or die "can't open $file";

    my $in_comment = 0;

    my $string = "";

    my $last_word;
    my @stack = ();
    my $paren_level = 0;
    my $in_exception_function = 0;

    LINE: while (<FILE>)
      {
	if ($in_comment)
	  {
	    s/.*?\*\/// or next LINE;
	    $in_comment = 0;
	  }

	# general approach is to just remove things we aren't interested in

	next LINE if /^\s*#\s*(\d|include)/;

	while (s/(((.*?)(\/\*|[\'\"\(\)]|\w+))|.+)//)
	  {
            my $skipped = $3;
            $skipped = $1 unless defined $skipped;
            my $found = $4;
            $found = "" unless defined $found;

            my $function_name = $last_word || "";

	    if ($skipped =~ /\S/ or $found =~ /^[\(\)\w]/)
	      {
		if ($string ne "")
		  {
		    found_string ($string, $file, $.) unless $in_exception_function;
		    $string = "";
		  }
                undef $last_word;
	      }

	    last unless $found ne "";

	    if ($found eq '"')
	      {
		s/^(([^\\\"]|\\.)*)\"// or (print "$file:$.:unclosed quote\n"), next FILE;
		$string .= $1;
	      }
	    elsif ($found eq "'")
	      {
                s/^([^\\\']|\\.)*\'// or (print "$file:$.:unclosed single quote\n"), next FILE;
              }
	    elsif ($found eq "/*")
	      {
		s/^.*?\*\/// or $in_comment = 1, next LINE;
	      }
	    elsif ($found eq "(")
              {
                if ($function_name or $paren_level == 0)
                  {
                    push @stack, [$paren_level, $in_exception_function];
                    $paren_level = 0;
                    $in_exception_function = 1 if $function_name =~ /$no_translation_needed_function_pattern/o;
                  }
                $paren_level++;
              }
	    elsif ($found eq ")")
	      {
                (print "$file:$.:mismatched paren (type 1)\n"), next FILE if $paren_level == 0;
                $paren_level--;
                if ($paren_level == 0)
                  {
                    (print "$file:$.:mismatched paren (type 2)\n"), next FILE if @stack == 0;
                    ($paren_level, $in_exception_function) = @{pop @stack};
                  }
	      }
            else
              {
                $last_word = $found;
              }
	  }
      }
    close FILE;
  }

sub found_string
  {
    my ($string, $file, $line) = @_;

    for my $exception (@no_translation_needed_patterns)
      {
        return if $string =~ /$exception->[0]/ and $file =~ /$exception->[1]/;
      }

    print "$file:$line:\"$string\" is not marked for translation\n";
  }
