## Process this file with automake to produce Makefile.in

SUBDIRS = src po

confdir=$(sysconfdir)/xdg/lxlauncher
conf_in_files=settings.conf.in
conf_DATA=$(conf_in_files:.conf.in=.conf)

themedir=$(confdir)
theme_DATA=gtkrc

menufiledir=/etc/xdg/menus
menufile_DATA=\
	data/lxlauncher-applications.menu

directorydir=$(datadir)/desktop-directories
directory_DATA=\
	data/lxde-webmail.directory \
	data/lxde-work.directory \
	data/lxde-learn.directory \
	data/lxde-play.directory \
	data/lxde-science.directory \
	data/lxde-math.directory

#backgrounddir=$(datadir)/lxlauncher/background
#background_DATA=

EXTRA_DIST = \
	$(menufile_DATA) \
	$(conf_in_files) \
	$(theme_DATA) \
	$(datafile_DATA) \
	$(directory_DATA) \
	autogen.sh
