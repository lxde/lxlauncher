## Process this file with automake to produce Makefile.in

SUBDIRS = src po

themedir=$(datadir)/lxlauncher
theme_DATA=gtkrc

menufiledir=/etc/xdg/menus
menufile_DATA=\
	data/lxlauncher.menu

directorydir=$(datadir)/desktop-directories
directory_DATA=\
	data/WebMail.directory \
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
