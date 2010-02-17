## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@GTK_CFLAGS@ @SN_CFLAGS@ @GMENU_CFLAGS@

bin_PROGRAMS = lxlauncher

lxlauncher_SOURCES = \
	lxlauncher.c \
        app-view.c app-view.h \
	misc.c misc.h \
	vfs-utils.c vfs-utils.h \
	working-area.c working-area.h

lxlauncher_LDADD = @GTK_LIBS@ @SN_LIBS@ @GMENU_LIBS@ $(INTLLIBS)

