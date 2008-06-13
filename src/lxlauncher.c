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

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <errno.h>

#include <cairo.h>

#include "vfs-app-desktop.h"

#include "inotify/linux-inotify.h"
#include "inotify/inotify-syscalls.h"

#include "exo-wrap-table.h"
#include "working-area.h"

#define BUTTON_SIZE	120
#define IMG_SIZE	48

#define DATA_DIR  PACKAGE_DATA_DIR"/lxlauncher"
#define BACKGROUND_DIR  PACKAGE_DATA_DIR"/lxlauncher/background"
#define ICON_DIR        PACKAGE_DATA_DIR"/lxlauncher/icons"

static GtkWidget* main_window;
static GtkWidget* notebook;
static GtkTooltips* tooltips;
static int n_cols;
static GtkIconSize icon_size;

static Atom atom_NET_WORKAREA = NULL;

static int inotify_fd = -1;
static GIOChannel* inotify_io_channel = NULL;
static int* watches = NULL;
static guint inotify_io_watch = 0;
static int reload_handler = 0;

typedef struct _AppDir
{
    char* name;
    char* title;
    char* icon;
    char* desc;
    GList* items;
    GList* children;
    struct _AppDir* parent;
}AppDir;

typedef struct _PageData{
    AppDir* dir;
	GtkWidget* page_vbox;
	GtkBox* go_up_bar;
	GtkWidget* table;
	GdkPixbuf* background;
}PageData;

static AppDir* app_dir_root = NULL;

static void finalize_inotify();

static void load_app_desktops( GHashTable* app_pool_hash, const char* dir_path, const char* parent )
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
				load_app_desktops( app_pool_hash, file, desktop_id );
			}
			else // desktop entry file
			{
				VFSAppDesktop* app_desktop;
				if( ! g_str_has_suffix( name, ".desktop" ) )
					continue;
				app_desktop = vfs_app_desktop_new( file );
				if( ! vfs_app_desktop_is_hidden( app_desktop ) )
				{
					g_hash_table_replace( app_pool_hash, g_strdup( desktop_id ),
												 app_desktop );
				}
				else
				{
					g_hash_table_remove( app_pool_hash, desktop_id );
					vfs_app_desktop_unref( app_desktop );
				}
			}

			g_free( desktop_id );
				g_free( file );

		}
		g_dir_close( dir );
	}
}

static void on_app_btn_clicked( GtkButton* btn, VFSAppDesktop* app )
{
	vfs_app_desktop_open_files( gdk_screen_get_default(),
										NULL, app, NULL, NULL );
}

static void notebook_page_chdir( PageData* data, AppDir* dir );

static void on_dir_btn_clicked( GtkButton* btn, PageData* data )
{
    AppDir* dir = (AppDir*)g_object_get_data( btn, "dir" );
    notebook_page_chdir( data, dir );
}

static gboolean is_app_in_cetegories( VFSAppDesktop* app, char** cats )
{
	char **grp_cat, **app_cat;
	for( grp_cat=cats; *grp_cat; ++grp_cat )
	{
		for( app_cat=app->categories; *app_cat; ++app_cat )
		{
			if( 0 == strcmp( *app_cat, *grp_cat ) )
				return TRUE;
		}
	}
	return FALSE;
}

static GtkWidget* add_btn( GtkWidget* table, const char* text, GdkPixbuf* icon, const char* tip )
{
    GtkWidget *btn, *box, *img, *label;

    // add the app to that page
    btn = gtk_button_new();
    GTK_WIDGET_UNSET_FLAGS(btn, GTK_CAN_FOCUS );
    GTK_WIDGET_UNSET_FLAGS(btn, GTK_CAN_DEFAULT );

    img = gtk_image_new_from_pixbuf( icon );

    box = gtk_vbox_new( FALSE, 2 );
    gtk_box_pack_start( box, img, FALSE, TRUE, 2 );

    label = gtk_label_new( text );
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

    if( tip )
        gtk_tooltips_set_tip( tooltips, btn, tip, NULL );

    gtk_container_add( table, btn );

    gtk_widget_realize( btn );
    gtk_widget_set_app_paintable( btn, TRUE );
    gdk_window_set_back_pixmap( ((GtkWidget*)btn)->window, NULL, TRUE );
    return btn;
}

static void add_dir_btn( PageData* data, AppDir* dir )
{
    GdkPixbuf* icon;
    GtkWidget* btn;
    if( dir->icon[0] == '/' )
        icon = gdk_pixbuf_new_from_file_at_size( dir->icon, IMG_SIZE, IMG_SIZE, NULL );
    else
        icon = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(), dir->icon, IMG_SIZE, 0, NULL );
    btn = add_btn( data->table, dir->title, icon, dir->desc );
    g_object_unref( icon );

    g_object_set_data( btn, "dir", dir );
    g_signal_connect( btn, "clicked", G_CALLBACK(on_dir_btn_clicked), data );
}

static void add_app_btn( GtkWidget* table, VFSAppDesktop* app )
{
    GdkPixbuf* pix = vfs_app_desktop_get_icon( app, IMG_SIZE, TRUE );
    GtkWidget* btn = add_btn( table, vfs_app_desktop_get_disp_name(app),
                              pix, vfs_app_desktop_get_desc(app) );
    g_object_unref( pix );
    vfs_app_desktop_ref( app );
    g_signal_connect( btn, "clicked", G_CALLBACK(on_app_btn_clicked), app );
}

// compare func used to sort apps in lists
static int sort_apps(VFSAppDesktop* app1, VFSAppDesktop* app2)
{
    return g_utf8_collate( app1->disp_name, app2->disp_name );
}

static gboolean grouping_apps( GHashTable* app_pool_hash );

static void load_apps()
{
	char** dirs = (char**)g_get_system_data_dirs(), **dir;
	GHashTable* app_pool_hash = g_hash_table_new_full( g_str_hash, g_str_equal, g_free,
														  vfs_app_desktop_unref );
    gboolean init_watch = FALSE;
    int i;
    static int n = 0;

    if( ! watches )
    {
        n = g_strv_length(dirs) + 1;
        watches = g_new0( int, n );
        init_watch = TRUE;
    }

    // load system-wide apps
    for( dir = dirs; *dir; ++dir )
	{
		char* dir_path = g_build_filename( *dir, "applications",NULL );

        if( init_watch )
        {
            // monitor the dir for changes
            watches[(dir-dirs)] = inotify_add_watch ( inotify_fd, dir_path,
                                        IN_MODIFY|IN_CREATE|IN_DELETE );
        }

		load_app_desktops( app_pool_hash, dir_path, NULL );
		g_free( dir_path );
	}
	// load user-specific apps
	{
		char* dir_path = g_build_filename( g_get_user_data_dir(), "applications",NULL );
        if( init_watch )
        {
            // monitor the dir for changes
            watches[n - 2] = inotify_add_watch ( inotify_fd, dir_path,
                                        IN_MODIFY|IN_CREATE|IN_DELETE );
        }
		load_app_desktops( app_pool_hash, dir_path, NULL );
		g_free( dir_path );
    }
	watches[n-1] = -1;

	grouping_apps( app_pool_hash );  // load the marvelous grouping rules and categorize apps.

	g_hash_table_destroy( app_pool_hash );
}

static gboolean on_viewport_expose( GtkWidget* w, GdkEventExpose* evt, gpointer data )
{
    GObjectClass* oc = G_OBJECT_GET_CLASS(w);
    GtkWidgetClass* wc = (GtkWidgetClass*)g_type_class_peek_parent( oc );
    GdkPixmap* pixmap = (GdkPixmap*)data;

    if( GTK_WIDGET_DRAWABLE(w) && evt->window == ((GtkViewport*)w)->bin_window )
    {
        cairo_t *cr;
        cairo_pattern_t* pat;

        GtkWidget* scroll = gtk_widget_get_parent(w);
        GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);

        cr = gdk_cairo_create (evt->window);
        pat = cairo_pattern_create_linear( 0, gtk_adjustment_get_value(vadj), 0, w->allocation.height + gtk_adjustment_get_value(vadj) );
        cairo_pattern_add_color_stop_rgb( pat, 0, 1, 1, 1);
        cairo_pattern_add_color_stop_rgb( pat, 1.0, ((gdouble)184/256), ((gdouble)215/256), ((gdouble)235/256));
//        cairo_rectangle(cr, 0, 0, w->allocation.width, w->allocation.height );
//        cairo_rectangle(cr, evt->area.x, evt->area.y, evt->area.width, evt->area.height );
        cairo_rectangle(cr, evt->area.x, evt->area.y, evt->area.width, evt->area.height );
        cairo_set_source(cr, pat);
        cairo_fill(cr);
        cairo_pattern_destroy(pat);
        cairo_destroy(cr);

/*
        GdkGC* gc = gdk_gc_new(evt->window);

        gdk_gc_set_tile( gc, pixmap );
        gdk_gc_set_fill( gc, GDK_TILED );
        gdk_gc_set_ts_origin( gc, 0, 0 );

        gdk_draw_rectangle( evt->window, gc, TRUE, evt->area.x, evt->area.y, evt->area.width, evt->area.height );

        gdk_gc_unref( gc );
*/
    }

    // call handler of tha parent GtkContainer class to propagate the event to children
    (* wc->expose_event) (w, evt);
    return TRUE;
}

static gboolean on_scroll( GtkAdjustment* adj, PageData* data )
{
    // Dirty hacks used to force pseudo-transparent background
    gtk_widget_queue_draw( data->table );
}

// Dirty hacks used to reduce unnecessary redrew during scroll
static gboolean on_scroll_change_val( GtkRange* scroll, GtkScrollType type, gdouble value, PageData* data )
{
    GtkAdjustment* adj = gtk_range_get_adjustment(scroll);

    if( type == GTK_SCROLL_JUMP )
    {
        if( ABS( adj->value - value ) < BUTTON_SIZE / 2 )
        {
            if( adj->value > value )    // upward
            {
                if( (adj->value - adj->lower) < BUTTON_SIZE / 2 )
                {
                    gtk_adjustment_set_value( adj, adj->lower );
                }
            }
            else // downward
            {
                if( (adj->upper - adj->value) < BUTTON_SIZE / 2 )
                {
                    gtk_adjustment_set_value( adj, adj->lower );
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean reload_apps()
{
    int i;
#if 0
    for( i = 0; i < G_N_ELEMENTS(groups); ++i )
    {
        // destroy all existing buttons
        gtk_container_foreach( groups[i].page, G_CALLBACK(gtk_widget_destroy), NULL );
        // groups[i].n_btns = 0;
        // gtk_table_resize( groups[i].page, 1, 1 );
        // g_debug("remove all children");
    }
    // load all apps again
    load_apps();
#endif
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

GdkFilterReturn evt_filter(GdkXEvent *xevt, GdkEvent *evt, gpointer data)
{
    XEvent *xev = (XEvent*)xevt;
    // working area is changed
    if(xev->type == PropertyNotify && xev->xproperty.atom == atom_NET_WORKAREA )
    {
        GdkRectangle working_area;
        get_working_area( gdk_drawable_get_screen(evt->any.window), &working_area );
        gtk_window_move( main_window, working_area.x, working_area.y );
        gtk_window_resize( main_window, working_area.width, working_area.height );
    }
    else if( evt->type == GDK_SCROLL )
    {
//        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

static gboolean window_delete(GtkWidget *window, gpointer data)
{
    return TRUE;
}

#define skip_spaces( str ) \
    while( *str == ' ' || *str == '\t' ) ++str;

static char* get_line( char** buf )
{
    char *ret, *pbuf;
    ret = pbuf = *buf;

    while( *pbuf && *pbuf != '\n' )
        ++pbuf;
    if( G_LIKELY(*pbuf == '\n') )
    {
        *pbuf = '\0';
        *buf = pbuf + 1;
    }
    else
        *buf = NULL;

    return ret;
}

AppDir* app_dir_new(const char* name)
{
    static const char desktop_entry_name[] = "Desktop Entry";

    AppDir* dir = g_new0( AppDir, 1 );
    GKeyFile* kf = g_key_file_new();
    char* fn = g_strconcat( "desktop-directories/", name, ".directory", NULL );

    if( g_key_file_load_from_data_dirs( kf, fn, NULL, 0, NULL ) )
    {
        dir->title = g_key_file_get_locale_string( kf, desktop_entry_name, "Name", NULL, NULL );
        dir->icon = g_key_file_get_string( kf, desktop_entry_name, "Icon", NULL );
        dir->desc = g_key_file_get_locale_string( kf, desktop_entry_name, "Comment", NULL, NULL );
    }
    g_free( fn );
    g_key_file_free( kf );

    if( ! dir->title )
        dir->title = g_strdup(name);
    if( ! dir->icon )
        dir->icon = g_strdup( "folder" );

    dir->name = g_strdup( name );
    return dir;
}

AppDir* app_dir_free( AppDir* dir )
{
    g_free(dir->name);
    g_free(dir->title);
    g_free(dir->icon);
    g_free(dir->desc);
    g_free(dir);
}

struct CategorizeData
{
    AppDir* dir;
    char** cats;
};

static void include_app( gpointer key, gpointer val, gpointer data )
{
    struct CategorizeData* cats_data= (struct CategorizeData*)data;
	int i;
	VFSAppDesktop* app = (VFSAppDesktop*)val;
    AppDir* dir = cats_data->dir;

	if( app->categories )
	{
        if( is_app_in_cetegories( app, cats_data->cats ) )
        {
            vfs_app_desktop_ref(app);
            dir->items = g_list_prepend(dir->items, app );
            return;
        }
	}
}

#if 0
static print_dir_structure( AppDir* dir )
{
    GList* l;
    g_debug( "app inside %s", dir->title );
    for( l = dir->children; l; l = l->next )
    {
        print_dir_structure( l->data );
    }
    for( l = dir->items; l; l = l->next )
    {
        g_debug("   %s", vfs_app_desktop_get_name(l->data) );
    }
    g_debug("-----------------");
}
#endif

static gboolean grouping_apps( GHashTable* app_pool_hash )
{
    gchar *buf, *ver;
    char *line, *key, *value, *sep;
    AppDir* dir;

    if( ! g_file_get_contents( DATA_DIR"/launcher.rules", &buf, NULL, NULL ) )
        return FALSE;

    // start parsing
    dir = app_dir_root = app_dir_new( NULL );

    while( buf )
    {
        line = get_line( &buf );

        skip_spaces( line );
        if( line[0] == '#' || line[0] == '\0' )
            continue;

        key = strtok( line, " \t" );

        if( strcmp( key, "dir" ) == 0 ) // start of menu
        {
            AppDir* new_dir;
            char* curly;
            value = strtok( NULL, " \t{" );
            if( ! value )   // no dir name, error!
                return NULL;
/*
            curly = strtok( NULL, " \t" );
            if( ! curly )   // no {, error!
                return NULL;
*/
            new_dir = app_dir_new(value);
            new_dir->parent = dir;
            dir->children = g_list_append( dir->children, new_dir );
            dir = new_dir;
        }
        else if( strcmp( key, "}" ) == 0 )  // close brace
        {
            // sort the apps
            dir->items = g_list_sort( dir->items, (GCompareDataFunc)sort_apps );

            if( G_UNLIKELY( ! dir->parent ) )   // error
                return NULL;

            dir = dir->parent;
        }
        else if( strcmp( key, "include" ) == 0 ) // include apps
        {
            char** cats;
            struct CategorizeData data;
            value = strtok( NULL, "" );

            cats = g_strsplit_set( value, " \t;", -1 );

            data.dir = dir;
            data.cats = cats;

            // g_debug( "dir '%s' include %s", dir->title, value );
            // scan the app pool and add every app meeting the inclusion criteria to dir.
            g_hash_table_foreach( app_pool_hash, include_app, &data );

            g_strfreev( cats );
        }
        else if( strcmp( key, "exclude" ) == 0 ) // exclude apps
        {
            GList* l, *next;
            char** cats;
            value = strtok( NULL, "" );

            cats = g_strsplit_set( value, " \t;", -1 );
            // remove the app from dir if it meets the exclusion criteria
            for( l = dir->items; l; l = next )
            {
                VFSAppDesktop* app = (VFSAppDesktop*)l->data;
                next = l->next;

                if( is_app_in_cetegories( app, cats ) )
                {
                    dir->items = g_list_delete_link( dir->items, l );
                    vfs_app_desktop_unref( app );
                }
            }
            g_strfreev( cats );
        }
    }

    // print_dir_structure(app_dir_root);

    g_free( buf );
    return FALSE;
}

static void notebook_page_chdir( PageData* data, AppDir* dir )
{
    GList* l;

    data->dir = dir;

    // destroy old buttons
    gtk_container_foreach( data->table, gtk_widget_destroy, NULL );

    gtk_container_forall( data->go_up_bar, gtk_widget_destroy, NULL );

    for( l = data->dir->children; l; l = l->next )
    {
        VFSAppDesktop* app = (VFSAppDesktop*)l->data;
        add_dir_btn( data, app );
    }

    for( l = dir->items; l; l = l->next )
    {
        VFSAppDesktop* app = (VFSAppDesktop*)l->data;
        add_app_btn( data->table, app );
    }

    if( dir->parent != app_dir_root )   // if dir has parent, not top-level group
    {
        GtkWidget* label = gtk_label_new( dir->title );
        char* text = g_strdup_printf( _("Go back to \"%s\""), dir->parent->title );
        GtkWidget* btn = gtk_button_new_with_label( text );
        g_free( text );

        gtk_misc_set_alignment( label, 0.0, 0.5 );
        g_object_set_data( btn, "dir", dir->parent );
        g_signal_connect( btn, "clicked", on_dir_btn_clicked, data );
        gtk_button_set_image( btn, gtk_image_new_from_stock( GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON ) );

        gtk_box_pack_start( data->go_up_bar, label, TRUE, TRUE, 4 );
        gtk_box_pack_start( data->go_up_bar, btn, FALSE, TRUE, 2 );
        gtk_widget_show_all( data->go_up_bar );
    }
    else
    {
        gtk_widget_hide( data->go_up_bar );
    }
}

static void page_data_free( PageData* data )
{
    if( data->background )
        g_object_unref( data->background );
    g_free( data );
}

int main(int argc, char** argv)
{
	int i;
	gboolean use_asus_icons;
	GdkRectangle working_area;
    GList* l;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

	gtk_init( &argc, &argv );

    // init inotify
    if( !init_inotify() )
        return 1;

    // set up themes for notebook
    gtk_rc_parse( PACKAGE_DATA_DIR "/lxlauncher/gtkrc" );

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

    g_signal_connect(main_window, "delete-event", G_CALLBACK(window_delete), NULL);

    atom_NET_WORKAREA = XInternAtom( GDK_DISPLAY(), "_NET_WORKAREA", True);;
    XSelectInput(GDK_DISPLAY(), GDK_WINDOW_XID(gtk_widget_get_root_window(main_window)), PropertyChangeMask );
	gdk_window_add_filter( gtk_widget_get_root_window(main_window), evt_filter, NULL );

	notebook = gtk_notebook_new();
	GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS );
	gtk_container_add( (GtkContainer*)main_window, notebook );

	tooltips = gtk_tooltips_new();
	g_object_ref_sink( tooltips );

    use_asus_icons = g_file_test( ICON_DIR, G_FILE_TEST_IS_DIR );

	load_apps();    // find all available apps

    // build pages for toplevel groups
	for( l = app_dir_root->children; l; l = l->next )
	{
	    GtkWidget* *viewport;
		GtkAdjustment* adj;
		GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
		GtkWidget* page_vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget* table = exo_wrap_table_new(TRUE);
		GtkWidget* label;
		GtkWidget* image;
		GtkWidget* go_up_bar = gtk_hbox_new( FALSE, 2 );
		GdkPixbuf* pixbuf=NULL;
		GdkPixmap* pixmap;
		GdkGC *pixmap_gc=NULL;
		char* file;
        AppDir* app_dir = (AppDir*)l->data;

        PageData* page_data = g_new0( PageData, 1 );
        g_object_set_data_full( page_vbox, "data", page_data, page_data_free );

		label = gtk_hbox_new( FALSE, 2 );

		gtk_scrolled_window_set_policy(scroll, GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC );

        // Very bad dirty hacks used to force gtk+ to draw transparent background
		GtkRange* range = gtk_scrolled_window_get_vscrollbar(scroll);
		//gtk_range_set_update_policy( range, GTK_UPDATE_DELAYED );
		g_signal_connect( range, "change-value", G_CALLBACK(on_scroll_change_val), page_data );
		adj = gtk_scrolled_window_get_vadjustment(scroll);
		adj->step_increment = BUTTON_SIZE / 3;
		adj->page_increment = BUTTON_SIZE / 2;
		gtk_adjustment_changed( adj );
        g_signal_connect( adj, "value-changed", G_CALLBACK(on_scroll), page_data );

        // create label
        if( use_asus_icons ) // use the ugly asus icons
        {
            file = g_build_filename( ICON_DIR, app_dir->icon, NULL );
            image = gtk_image_new_from_file(file);
            g_free( file );
        }
        else // use themed icon provided by icon themes
            image = gtk_image_new_from_icon_name( app_dir->icon, GTK_ICON_SIZE_MENU );

		gtk_box_pack_start( label, image, FALSE, TRUE, 2 );
		gtk_box_pack_start( label, gtk_label_new( app_dir->title ), FALSE, TRUE, 2 );
		gtk_widget_show_all(label);

        // gtk_container_set_border_width( page_vbox, 4 );
        exo_wrap_table_set_col_spacing( table, 8 );

		viewport = gtk_viewport_new( NULL, NULL );
		gtk_container_add( viewport, table );
		gtk_container_add( scroll, viewport );
		gtk_widget_show_all( scroll );

        gtk_box_pack_start( page_vbox, go_up_bar, FALSE, TRUE, 0 );
        gtk_box_pack_start( page_vbox, scroll, TRUE, TRUE, 0 );

		gtk_notebook_append_page( notebook, page_vbox, label );

        // set background
        gtk_widget_set_app_paintable( viewport, TRUE );
/*
        file = g_build_filename( BACKGROUND_DIR, groups[i].background, NULL );
        pixbuf = gdk_pixbuf_new_from_file( file, NULL );
        g_free( file );
*/
        if( pixbuf )
        {
            pixmap = gdk_pixmap_new( gdk_get_default_root_window(), gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf), -1 );
            pixmap_gc = gdk_gc_new(pixmap);
            gdk_pixbuf_render_to_drawable(pixbuf, pixmap, pixmap_gc,
                                    0, 0, 0, 0,
                                    gdk_pixbuf_get_width(pixbuf),
                                    gdk_pixbuf_get_height(pixbuf),
                                    GDK_RGB_DITHER_NORMAL, 0, 0 );
            g_object_unref( pixbuf );

            g_object_weak_ref( viewport, (GWeakNotify)g_object_unref, pixmap );
            g_object_unref(pixmap_gc);
        }
        g_signal_connect( viewport, "expose_event", G_CALLBACK(on_viewport_expose), pixmap );

        page_data->page_vbox = page_vbox;
        page_data->go_up_bar = go_up_bar;
        page_data->table = table;
        notebook_page_chdir( page_data, app_dir );
	}

    get_working_area( gtk_widget_get_screen(main_window), &working_area );
    // working_area.height = 200;
    gtk_window_move( main_window, working_area.x, working_area.y );
    gtk_window_resize( main_window, working_area.width, working_area.height );

	gtk_widget_show_all( main_window );
	gtk_main();

    gdk_window_remove_filter( gtk_widget_get_root_window(main_window), evt_filter, NULL );

    finalize_inotify();

	return 0;
}
