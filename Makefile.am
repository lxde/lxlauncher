## Process this file with automake to produce Makefile.in

SUBDIRS = src po

confdir=$(sysconfdir)/xdg/lxlauncher
conf_in_files=settings.conf.in
conf_DATA=$(conf_in_files:.conf.in=.conf)

themedir=$(confdir)
theme_DATA=\
	gtkrc \
	gtk.css

menufiledir=$(sysconfdir)/xdg/menus
menufile_DATA=\
	data/lxlauncher-applications.menu

man_MANS = data/lxlauncher.1

directorydir=$(datadir)/desktop-directories
directory_in_files=\
	data/lxde-webmail.directory.in \
	data/lxde-work.directory.in \
	data/lxde-learn.directory.in \
	data/lxde-play.directory.in \
	data/lxde-science.directory.in \
	data/lxde-math.directory.in
directory_files = $(directory_in_files:.directory.in=.directory)
directory_DATA = $(directory_files)

@INTLTOOL_DIRECTORY_RULE@

#backgrounddir=$(datadir)/lxlauncher/background
#background_DATA=

EXTRA_DIST = \
	$(menufile_DATA) \
	$(conf_in_files) \
	$(theme_DATA) \
	$(datafile_DATA) \
	$(directory_in_files) \
	$(directory_DATA) \
	autogen.sh
