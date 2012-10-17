## Process this file with automake to produce Makefile.in

## Created by Anjuta - will be overwritten
## If you don't want it to overwrite it,
## 	Please disable it in the Anjuta project configuration

INCLUDES =
libdir = @libdir@/nemo/extensions-3.0

AM_CFLAGS =\
	`pkg-config --cflags libnemo-extension`\
	 -Wall

lib_LTLIBRARIES = libnemo-wallpaper.la

libnemo_wallpaper_la_SOURCES = \
	nemo-nwe.c\
	nemo-nwe.h\
	nemo-wallpaper-command.c

libnemo_wallpaper_la_LDFLAGS = \
	`pkg-config --libs libnemo-extension`\
	-module -avoid-version -no-undefined

libnemo_wallpaper_la_LIBADD = 

