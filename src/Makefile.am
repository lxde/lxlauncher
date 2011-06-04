## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(GTK_CFLAGS) $(SN_CFLAGS) $(GMENU_CFLAGS) $(X11_CFLAGS) $(ADDITIONAL_FLAGS)

bin_PROGRAMS = lxlauncher

lxlauncher_SOURCES = \
	lxlauncher.c \
	misc.c misc.h \
	glib-mem.h \
	vfs-utils.c vfs-utils.h \
	vfs-execute.c vfs-execute.h \
	working-area.c working-area.h \
	exo-wrap-table.c exo-wrap-table.h

lxlauncher_LDADD = $(GTK_LIBS) $(SN_LIBS) $(GMENU_LIBS) $(X11_LIBS) $(INTLLIBS)

