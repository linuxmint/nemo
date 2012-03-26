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

# check-FIXME.pl: Search for FIXMEs in the sources and correlate them
# with bugs in the bug database.

use diagnostics;
use strict;

# default to all the files starting from the current directory
my %skip_files;
if (!@ARGV)
  {
    @ARGV = `find . \\( \\( -name po -prune -false \\) -or \\( -name CVS -prune -false \\) -or \\( -name macros -prune -false \\) -or \\( -name '*' -and -type f \\) \\) -and ! \\( -name '*~' -or -name '#*' -or -name 'ChangeLog*' -or -name Entries \\) -print`;
    %skip_files =
      (
       "./HACKING" => 1,
       "./TODO" => 1,
       "./aclocal.m4" => 1,
       "./check-FIXME.pl" => 1,
       "./config.sub" => 1,
       "./libtool" => 1,
       "./ltconfig" => 1,
       "./ltmain.sh" => 1,
      );
  }

#locate all of the open FIXMEs in the bug database
my $pwd=`pwd`;
chomp $pwd;

my $repository_file = $pwd."/CVS/Repository";
open FILE, " cat $repository_file | ";

my $product = <FILE>;
chomp $product;

close FILE;

print "Searching the bugzilla database's product $product for open FIXME bugs\n";

if (!grep /$product/, ( "nautilus", "gnome-vfs", "medusa", "oaf")) {
    print "Can't find your product in the bugzilla.gnome.org database\n";
}

my $bugzilla_query_bug_url = "http://bugzilla.gnome.org/buglist.cgi?";

$product =~ s/\-/\+/g;
my @cgi_options = ("bug_status=NEW",
		   "bug_status=ASSIGNED",
		   "bug_status=REOPENED",
		   "long_desc=fixme",
		   "long_desc_type=substring",
		   "product=$product");

my $open_fixmes_url = $bugzilla_query_bug_url.join "&", @cgi_options;

`wget -q -O - "$open_fixmes_url"` =~ /<INPUT TYPE\=HIDDEN NAME\=buglist VALUE\=([0-9:]+)>/;
my $buglist_text = $1;

my %bugs_in_bugzilla;
foreach my $bug (split /:/, $buglist_text) {
    $bugs_in_bugzilla{$bug} = "UNFOUND";
}

print "Locating all of the FIXME's listed in source\n";
# locate all of the target lines
my $no_bug_lines = "";
my %bug_lines;
foreach my $file (@ARGV)
  {
    chomp $file;
    next if $skip_files{$file};
    next unless -T $file;
    open(FILE, $file) || die "can't open $file";
    while (<FILE>)
      {
        next if !/FIXME/;
        if (/FIXME\s*:?\s*bugzilla.gnome.org\s+(\d+)/)
          {
            $bug_lines{$1} .= "$file:$.:$_";
          }
        else
          {
            $no_bug_lines .= "$file:$.:$_";
          }
      }
    close(FILE);
  }

# list database bugs we can't find in nautilus
printf "%d FIXMES in the database still in $product\n", keys %bug_lines;

foreach my $bug_number (keys %bug_lines) {
    $bugs_in_bugzilla{$bug_number} = "FOUND";
}

print "\n";
foreach my $bug_number (keys %bugs_in_bugzilla) {
    if ($bugs_in_bugzilla{$bug_number} eq "UNFOUND") {
        # Also check that the 
        my $bug_url = "http://bugzilla.gnome.org/show_bug.cgi?id=".$bug_number;
        my $bug_page = `wget -q -O - $bug_url`;
        if (!($bug_page =~ /This is not a FIXME bug/i)) {
          $bug_page =~ /<A HREF=\"bug_status.html\#assigned_to\">Assigned To:<\/A><\/B><\/TD>\s+<TD>([^<]+)<\/TD>/s;
          print "Bug $bug_number isn't in the source anymore.  Contact owner $1.\n";
        }

    }
}

# list the ones without bug numbers
if ($no_bug_lines ne "")
  {
    my @no_bug_list = sort split /\n/, $no_bug_lines;
    print "\n", scalar(@no_bug_list), " FIXMEs don't have bug numbers:\n\n";
    foreach my $line (@no_bug_list)
      {
        print $line, "\n";
      }
  }

# list the ones with bugs that are not open
print "\n", scalar(keys %bug_lines), " FIXMEs with bug numbers.\n";
sub numerically { $a <=> $b; }
foreach my $bug (sort numerically keys %bug_lines)
  {
    # Check and see if the bug is open.
    my $page = `wget -q -O - http://bugzilla.gnome.org/show_bug.cgi?id=$bug`;
    $page =~ tr/\n/ /;
    my $status = "unknown";
    $status = $1 if $page =~ m|Status:.*</TD>\s*<TD>([A-Z]+)</TD>|;
    next if $status eq "NEW" || $status eq "ASSIGNED" || $status eq "REOPENED";

    # This a bug that is not open, so report it.
    my @bug_line_list = sort split /\n/, $bug_lines{$bug};
    print "\nBug $bug is $status:\n\n";
    foreach my $line (@bug_line_list)
      {
        print $line, "\n";
      }
  }
print "\n";
