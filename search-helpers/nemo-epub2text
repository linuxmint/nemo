#!/usr/bin/env bash

# Requires unzip package
# ==================================
# 1. Get a list of all xhtml/html/htm files, exclude titlepage.xhtml (if present)
#    It appears that the zipped files _never_ contain problem characters such as spaces...
# 2. Extract the html files and convert to text (UTF-8 output is available).
# ==================================

# 1. Get a list of xhtml/html/htm files [using unzip's weird regular expression] - and exclude any named titlepage/toc/copyright
files=$(unzip -Z1 "$1" \*.*htm* | egrep -v 'titlepage.*|toc.*|copyright.*')

# 2. Uncompress each of the files and process with html2text.
unzip -cqq "$1" $files | html2text -o -
