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

# check-headers-in-Makefile.pl: Checks the contents of the source
# directories against the contents of the Makefile.am files.

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

while (@directories)
  {
    my $directory = pop @directories;
    my $prefix = "";
    $prefix = "$directory/" if $directory ne ".";

    my $in_subdirs = 0;

    my $file = $prefix . "Makefile.am";
    open FILE, $file or die "can't open $file";
    my %headers;
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
	while (s/([-_a-zA-Z0-9]+\.[ch])\W//)
	  {
	    $headers{$1} = $1;
	  }
      }
    close FILE;

    if ($directory eq ".")
      {
	$headers{"config.h"} = "config.h";
      }

    opendir DIRECTORY, $directory or die "can't open $directory";
    foreach my $header (grep /.*\.[ch]$/, readdir DIRECTORY)
      {
	if (defined $headers{$header})
	  {
	    delete $headers{$header};
	  }
	else
	  {
	    print "$directory/$header in directory but not Makefile.am\n";
	  }
      }
    closedir DIRECTORY;

    foreach my $header (keys %headers)
      {
	print "$directory/$header in Makefile.am but not directory\n";
      }
  }
