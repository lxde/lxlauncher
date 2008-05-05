/*
 *      lxlauncher.c - Open source replace for EeePC Asus Launcher
 *      
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <errno.h>

#include "vfs-app-desktop.h"

#include "inotify/linux-inotify.h"
#include "inotify/inotify-syscalls.h"

#define WIN_WIDTH	800
#define WIN_HEIGHT	480
#define BUTTON_SIZE	120
#define IMG_SIZE	48
#define N_COLS	(WIN_WIDTH / BUTTON_SIZE)

#define BACKGROUND_DIR  PACKAGE_DATA_DIR"/lxlauncher/background"

static GtkWidget* main_window;
static GtkWidget* notebook;
static GtkTooltips* tooltips;
static int n_cols;
static GtkIconSize icon_size;

static int inotify_fd = -1;
static GIOChannel* inotify_io_channel = NULL;
static int* watches = NULL;
static guint inotify_io_watch = 0;
static int reload_handler = 0;

typedef struct {
	char* title;
	char* background;
	char* icon;
	GtkWidget* page;
	char** categories;
	int n_btns;
}Group;

static char* net_cats[] = { "Network", NULL };
static char* work_cats[] = { "Office", "Utility", "Graphics", "TextEditor", "TextTools", "Viewer", "Core", "Development", NULL };
static char* learn_cats[] = { "Education", "Dictionary", NULL };
static char* play_cats[] = { "AudioVideo", "Game", NULL };
static char* settings_cats[] = { "Settings", "System", NULL };

static Group groups[]={
	{N_("Internet"), "accessibility_internet_wallpaper.jpg", "applications-internet", NULL, net_cats, 0 },
	{N_("Work"), "accessibility_work_wallpaper.jpg", "applications-office", NULL, work_cats, 0 },
	{N_("Learn"), "accessibility_learn_wallpaper.jpg", "applications-accessories", NULL, learn_cats, 0 },
	{N_("Play"), "accessibility_play_wallpaper.jpg", "applications-multimedia", NULL, play_cats, 0 },
	{N_("Settings"), "accessibility_settings_wallpaper.jpg", "gtk-preferences", NULL, settings_cats, 0 },
};

static void finalize_inotify();

static void load_app_desktops( GHashTable* hash, const char* dir_path, const char* parent )
{
	GDir* dir = g_dir_open( dir_path, 0, NULL );
	if( dir )
	{
		char* name;
		while( name = g_dir_read_name(dir) )
		{
			char* desktop_id = NULL, *file;
			if( parent )
				desktop_id = g_strconcat( parent, "-", name, NULL );
			else
				desktop_id = g_strdup( name );

			file = g_build_filename( dir_path, name, NULL );

			if( g_file_test( file, G_FILE_TEST_IS_DIR ) ) // sub dir
			{
				load_app_desktops( hash, file, desktop_id );
			}
			else // desktop entry file
			{
				VFSAppDesktop* app_desktop;
				if( ! g_str_has_suffix( name, ".desktop" ) )
					continue;
				app_desktop = vfs_app_desktop_new( file );
				if( ! vfs_app_desktop_is_hidden( app_desktop ) )
				{
					g_hash_table_replace( hash, g_strdup( desktop_id ),
												 app_desktop );
				}
				else
					vfs_app_desktop_unref( app_desktop );
			}

			g_free( desktop_id );
				g_free( file );
			
		}
		g_dir_close( dir );
	}
}

static void on_btn_clicked( GtkButton* btn, VFSAppDesktop* app )
{
	vfs_app_desktop_open_files( gdk_screen_get_default(),
										NULL, app, NULL, NULL );
}

static gboolean is_app_in_group( VFSAppDesktop* app, Group* grp )
{
	char **grp_cat, **app_cat;
	for( grp_cat=grp->categories; *grp_cat; ++grp_cat )
	{
		for( app_cat=app->categories; *app_cat; ++app_cat )
		{
			if( 0 == strcmp( *app_cat, *grp_cat ) )
				return TRUE;
		}
	}
	return FALSE;
}

static void add_app( gpointer key, gpointer val, gpointer data )
{
	int i;
	VFSAppDesktop* app = val;
	if( app->categories )
	{
		for( i = 0; i < G_N_ELEMENTS(groups); ++i )
		{
			if( is_app_in_group( app, &groups[i] ) )
			{
				GtkButton *btn, *box, *img, *label;
				int x, y;
				GtkTable* table = (GtkTable*)groups[i].page;
				// add the app to that page

				btn = gtk_button_new();
				GTK_WIDGET_UNSET_FLAGS(btn, GTK_CAN_FOCUS );
				GTK_WIDGET_UNSET_FLAGS(btn, GTK_CAN_DEFAULT );
//				if( g_path_is_absolute( vfs_app_desktop_get_icon_name(app) ) )
//				{
					GdkPixbuf* pix = vfs_app_desktop_get_icon( app, IMG_SIZE, TRUE );
					img = gtk_image_new_from_pixbuf( pix );
//				}
//				else
//					img = gtk_image_new_from_icon_name( vfs_app_desktop_get_icon_name(app), icon_size );
				box = gtk_vbox_new( FALSE, 2 );
				gtk_box_pack_start( box, img, FALSE, TRUE, 2 );

				label = gtk_label_new( vfs_app_desktop_get_disp_name(app) );
				gtk_widget_show( label );
				gtk_widget_set_size_request( label, BUTTON_SIZE - 10, -1 );
				gtk_label_set_line_wrap_mode( label, PANGO_WRAP_WORD_CHAR );
				gtk_label_set_line_wrap( label, TRUE );
				gtk_label_set_justify( label, GTK_JUSTIFY_CENTER );
				gtk_box_pack_start( box, label, FALSE, TRUE, 2 );
				gtk_container_add( btn, box );

				gtk_button_set_relief( btn, GTK_RELIEF_NONE );
				gtk_widget_set_size_request( btn, BUTTON_SIZE, BUTTON_SIZE );
				gtk_widget_show_all( btn );

				gtk_tooltips_set_tip( tooltips, btn, vfs_app_desktop_get_desc(app), NULL );

				vfs_app_desktop_ref( app );
				g_signal_connect( btn, "clicked", G_CALLBACK(on_btn_clicked), app );

				y = groups[i].n_btns / N_COLS;
				x = groups[i].n_btns % N_COLS;
				//g_debug("x = %d, y = %d", x, y);
				gtk_table_resize( table, y + 1, N_COLS );
				gtk_table_attach( table, btn, x, x+1, y, y+1, 0, 0, 2, 2 );
				++groups[i].n_btns;

                gtk_widget_realize( btn );
                gtk_widget_set_app_paintable( btn, TRUE );
                gdk_window_set_back_pixmap( ((GtkWidget*)btn)->window, NULL, TRUE );
				return;
			}
		}
	}
}

static void load_apps()
{
	char** dirs = (char**)g_get_system_data_dirs(), **dir;
	GHashTable* hash = g_hash_table_new_full( g_str_hash, g_str_equal, g_free,
														  vfs_app_desktop_unref );
    gboolean init_watch = FALSE;
    if( ! watches )
    {
        watches = g_new0( int, g_strv_length(dirs) + 1 );
        init_watch = TRUE;
    }

    for( dir = dirs; *dir; ++dir )
	{
		char* dir_path = g_build_filename( *dir, "applications",NULL );

        if( init_watch )
        {
            // monitor the dir for changes
            watches[(dir-dirs)] = inotify_add_watch ( inotify_fd, dir_path,
                                        IN_MODIFY|IN_CREATE|IN_DELETE );
        }

		load_app_desktops( hash, dir_path, NULL );
		g_free( dir_path );
	}
	watches[(dir-dirs)] = -1;

	g_hash_table_foreach( hash, add_app, NULL );

	g_hash_table_destroy( hash );
}

static gboolean on_viewport_expose( GtkWidget* w, GdkEventExpose* evt, gpointer data )
{
    GObjectClass* oc = G_OBJECT_GET_CLASS(w);
    GtkWidgetClass* wc = (GtkWidgetClass*)g_type_class_peek_parent( oc );
    GdkPixmap* pixmap = (GdkPixmap*)data;

    if( GTK_WIDGET_DRAWABLE(w) )
    {
        GdkGC* gc = gdk_gc_new(evt->window);

        gdk_gc_set_tile( gc, pixmap );
        gdk_gc_set_fill( gc, GDK_TILED );
        gdk_gc_set_ts_origin( gc, 0, 0 );

        gdk_draw_rectangle( evt->window, gc, TRUE, evt->area.x, evt->area.y, evt->area.width, evt->area.height );

        gdk_gc_unref( gc );
    }

    // call handler of tha parent GtkContainer class to propagate the event to children
    (* wc->expose_event) (w, evt);
    return TRUE;
}

static gboolean reload_apps()
{
    int i;;
    for( i = 0; i < G_N_ELEMENTS(groups); ++i )
    {
        // destroy all existing buttons
        gtk_container_foreach( groups[i].page, G_CALLBACK(gtk_widget_destroy), NULL );
        groups[i].n_btns = 0;
        gtk_table_resize( groups[i].page, 1, 1 );
        g_debug("remove all children");
    }
    // load all apps again
    load_apps();
    reload_handler = 0;
    return FALSE;
}

/* event handler of all inotify events */
static gboolean on_inotify_event( GIOChannel * channel,
                                  GIOCondition cond,
                                  gpointer user_data )
{
    #define BUF_LEN (1024 * (sizeof (struct inotify_event) + 16))
    char buf[ BUF_LEN ];
    int i, len;

    if ( cond & (G_IO_HUP | G_IO_ERR) )
    {
        finalize_inotify();
        return TRUE; /* don't need to remove the event source since
                        it has been removed by finalize_inotify(). */
    }

    // keep reading if error happens
    while( ( len = read ( inotify_fd, buf, BUF_LEN ) ) < 0
            && errno == EINTR );

    if ( len < 0 )
        return FALSE;   // error

    if ( len == 0 )
        return FALSE;   // error

    // some changes happened in applications dirs
    // reload is needed

    if( reload_handler )
        g_source_remove( reload_handler );

    reload_handler = g_timeout_add( 1000,(GSourceFunc)reload_apps, NULL );

#if 0
    i = 0;
    while ( i < len )
    {
        struct inotify_event * ievent = ( struct inotify_event * ) & buf [ i ];
        if( ievent->len > 0 )
        {
            // g_debug( "file: %s was modified", ievent->name );
        }
        i += sizeof ( struct inotify_event ) + ievent->len;
    }
#endif
    return TRUE;
}

static gboolean init_inotify()
{
    inotify_fd = inotify_init();
    if ( inotify_fd < 0 )
    {
        g_warning( "failed to initialize inotify." );
        return FALSE;
    }
    inotify_io_channel = g_io_channel_unix_new( inotify_fd );
    g_io_channel_set_encoding( inotify_io_channel, NULL, NULL );
    g_io_channel_set_buffered( inotify_io_channel, FALSE );
    g_io_channel_set_flags( inotify_io_channel, G_IO_FLAG_NONBLOCK, NULL );

    inotify_io_watch = g_io_add_watch( inotify_io_channel,
                                   G_IO_IN | G_IO_PRI | G_IO_HUP|G_IO_ERR,
                                   on_inotify_event,
                                   NULL );
    return TRUE;
}

static void finalize_inotify()
{
    int *watch;

    for( watch = watches; *watch != -1; ++watch )
        inotify_rm_watch ( inotify_fd, *watch );
    g_free( watches );
    watches = NULL;

    g_io_channel_unref( inotify_io_channel );
    inotify_io_channel = NULL;
    g_source_remove( inotify_io_watch );

    close( inotify_fd );
    inotify_fd = -1;
}

int main(int argc, char** argv)
{
	int i;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

	gtk_init( &argc, &argv );

    // init inotify
    if( !init_inotify() )
        return 1;

	icon_size = gtk_icon_size_register( "ALIcon", IMG_SIZE, IMG_SIZE );

	main_window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_move( main_window, 0, 0 );
	gtk_window_set_skip_pager_hint( main_window, TRUE );
	gtk_window_set_skip_taskbar_hint( main_window, TRUE );

    gtk_widget_realize( main_window );
	gdk_window_set_keep_below( main_window->window, TRUE );
	//gdk_window_set_decorations( main_window->window );
	gdk_window_set_type_hint( main_window->window, GDK_WINDOW_TYPE_HINT_DESKTOP );
	gtk_window_set_position( main_window, GTK_WIN_POS_NONE );
	//gtk_window_set_gravity(GDK_GRAVITY_STATIC );

	notebook = gtk_notebook_new();
	gtk_container_add( (GtkContainer*)main_window, notebook );

	tooltips = gtk_tooltips_new();
	g_object_ref_sink( tooltips );

	for( i = 0; i < G_N_ELEMENTS(groups); ++i )
	{
	    GtkWidget* *viewport;
		GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
		GtkWidget* page = gtk_table_new( 1, 1, FALSE );
		GtkWidget* label;
		GtkWidget* image;
		GdkPixbuf* pixbuf;
		GdkPixmap* pixmap;
		char* file;

		label = gtk_hbox_new( FALSE, 2 );

		gtk_scrolled_window_set_policy(scroll, GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

        // create label
		gtk_box_pack_start( label, gtk_image_new_from_icon_name(groups[i].icon, GTK_ICON_SIZE_MENU), FALSE, TRUE, 0 );
		gtk_box_pack_start( label, gtk_label_new(_(groups[i].title)), FALSE, TRUE, 0 );
		gtk_widget_show_all(label);

		viewport = gtk_viewport_new( NULL, NULL );
		gtk_container_add( viewport, page );
		gtk_container_add( scroll, viewport );
		gtk_widget_show_all( scroll );

		gtk_notebook_append_page( notebook, scroll, label );
		groups[i].page = page;

        // set background
        gtk_widget_set_app_paintable( viewport, TRUE );

        file = g_build_filename( BACKGROUND_DIR, groups[i].background, NULL );
        pixbuf = gdk_pixbuf_new_from_file( file, NULL );
        g_free( file );

        if( pixbuf )
        {
            pixmap = gdk_pixmap_new( gdk_get_default_root_window(), gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf), -1 );
            gdk_pixbuf_render_to_drawable(pixbuf, pixmap, scroll->style->black_gc,
                                    0, 0, 0, 0,
                                    gdk_pixbuf_get_width(pixbuf),
                                    gdk_pixbuf_get_height(pixbuf),
                                    GDK_RGB_DITHER_NORMAL, 0, 0 );
            g_object_unref( pixbuf );

            g_signal_connect( viewport, "expose_event", G_CALLBACK(on_viewport_expose), pixmap );
            g_object_weak_ref( viewport, (GWeakNotify)g_object_unref, pixmap );
        }
	}

	load_apps();
	gtk_window_set_default_size( (GtkWindow*)main_window, WIN_WIDTH, WIN_HEIGHT );
	gtk_widget_show_all( main_window );
	gtk_main();

    finalize_inotify();

	return 0;
}
