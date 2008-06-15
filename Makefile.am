## Process this file with automake to produce Makefile.in

SUBDIRS = src po

themedir=$(datadir)/lxlauncher
theme_DATA=gtkrc

datafiledir=$(datadir)/lxlauncher
datafile_DATA=\
	data/launcher.menu

directorydir=$(datadir)/desktop-directories
directory_DATA=\
	data/Work.directory \
	data/Learn.directory \
	data/Play.directory \
	data/Science.directory \
	data/Math.directory

#backgrounddir=$(datadir)/lxlauncher/background
#background_DATA=

EXTRA_DIST = \
	$(theme_DATA) \
	$(datafile_DATA) \
	$(directory_DATA) \
	autogen.sh
