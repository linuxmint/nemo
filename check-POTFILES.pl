#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  Nautilus
#
#  Copyright (C) 2000 Eazel, Inc.
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

# check-POTFILES.pl: Checks for files mentioned in POTFILES.in that
# are not present in the Makefile.am files for those directories.

use diagnostics;
use strict;

my @directories = (".");

my %exceptions =
  (
   '$(APPLETS_SUBDIRS)' => 'applets',
   '$(AUTHENTICATE_HELPER_SUBDIRS)' => 'authenticate',
   '$(INSTALL_SERVICE)' => 'install',
   '$(MOZILLA_COMPONENT_SUBDIRS)' => 'mozilla',
   '$(NULL)' => '',
   '$(RPMVIEW_COMPONENT_SUBDIRS)' => 'rpmview',
   '$(SERVICE_SUBDIRS)' => 'services',
   'intl' => '',
   'po' => '',
  );

my %files;

# collect all files mentioned in Makefile.am files
while (@directories)
  {
    my $directory = pop @directories;
    my $prefix = "";
    $prefix = "$directory/" if $directory ne ".";

    my $in_subdirs = 0;

    my $file = $prefix . "Makefile.am";
    open FILE, $file or die "can't open $file\n";
    while (<FILE>)
      {
        if (s/^SUBDIRS\s*=//)
          {
            $in_subdirs = 1;
          }
        if ($in_subdirs)
          {
            while (s/^\s*([^\s\\]+)//)
              {
                if (defined $exceptions{$1})
                  {
                    if ($exceptions{$1})
                      {
                        push @directories, $prefix . $exceptions{$1};
                      }
                  }
                else
                  {
                    push @directories, $prefix . $1;
                  }
              }
            if (/^\s*$/)
              {
                $in_subdirs = 0;
              }
            elsif (!/^\s*\\$/)
              {
                die "can't parse SUBDIRS in $directory\n";
              }
          }
	while (s/([-_a-zA-Z0-9]+\.(c|h|xml|cpp|oaf\.in|desktop\.in))\W//)
	  {
	    $files{$prefix . $1} = $1;
	  }
      }
    close FILE;
  }

open POTFILES, "po/POTFILES.in" or die "can't open POTFILES.in\n";
while (<POTFILES>)
  {
    chomp;
    if (! defined $files{$_})
      {
        print "$_ is in POTFILES.in but not Makefile.am\n";
      }
  }
close POTFILES;
