## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@GTK_CFLAGS@ @SN_CFLAGS@

bin_PROGRAMS = lxlauncher

lxlauncher_SOURCES = \
	lxlauncher.c \
	glib-mem.h \
	vfs-app-desktop.c vfs-app-desktop.h \
	vfs-utils.c vfs-utils.h \
	vfs-execute.c vfs-execute.h \
	working-area.c working-area.h \
	exo-wrap-table.c exo-wrap-table.h \
	inotify/inotify-syscalls.h \
	inotify/linux-inotify.h

lxlauncher_LDADD = @GTK_LIBS@ @SN_LIBS@ $(INTLLIBS)

