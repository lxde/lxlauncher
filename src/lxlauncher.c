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

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include <errno.h>

#include <cairo.h>

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

static GMenuTree* menu_tree = NULL;
static GMenuTreeDirectory* root_dir = NULL;

static int reload_handler = 0;

typedef struct _PageData{
    GMenuTreeDirectory* dir;
	GtkWidget* page_vbox;
	GtkBox* go_up_bar;
	GtkWidget* table;
	GdkPixbuf* background;
}PageData;

static void on_app_btn_clicked( GtkButton* btn, GMenuTreeEntry* app )
{
	lxlauncher_execute_app( gdk_screen_get_default(),
										NULL, gmenu_tree_entry_get_exec(app), 
										gmenu_tree_entry_get_name(app), NULL, 
										gmenu_tree_entry_get_launch_in_terminal(app),
										NULL );
}

static void notebook_page_chdir( PageData* data, GMenuTreeDirectory* dir );

static void on_dir_btn_clicked( GtkButton* btn, PageData* data )
{
    GMenuTreeDirectory* dir = (GMenuTreeDirectory*)g_object_get_data( btn, "dir" );
    notebook_page_chdir( data, dir );
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

#if 0
GdkPixbuf* load_icon( const char* icon_name, int size, gboolean use_fallback )
{
    GtkIconTheme* theme;
    char *_icon_name = NULL, *suffix;
    GdkPixbuf* icon = NULL;

    if( app->icon_name )
    {
        if( g_path_is_absolute( app->icon_name) )
        {
            icon = gdk_pixbuf_new_from_file_at_scale( app->icon_name,
                                                     size, size, TRUE, NULL );
        }
        else
        {
            theme = gtk_icon_theme_get_default();
            suffix = strchr( app->icon_name, '.' );
            if( suffix ) /* has file extension, it's a basename of icon file */
            {
                /* try to find it in pixmaps dirs */
                icon = load_icon_file( app->icon_name, size );
                if( G_UNLIKELY( ! icon ) )  /* unfortunately, not found */
                {
                    /* Let's remove the suffix, and see if this name can match an icon
                         in current icon theme */
                    _icon_name = g_strndup( app->icon_name,
                                           (suffix - app->icon_name) );
                    icon = vfs_load_icon( theme, _icon_name, size );
                    g_free( _icon_name );
                }
            }
            else  /* no file extension, it could be an icon name in the icon theme */
            {
                icon = vfs_load_icon( theme, app->icon_name, size );
            }
        }
    }
    if( G_UNLIKELY( ! icon ) && use_fallback )  /* fallback to generic icon */
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon( theme, "application-x-executable", size );
        if( G_UNLIKELY( ! icon ) )  /* fallback to generic icon */
        {
            icon = vfs_load_icon( theme, "gnome-mime-application-x-executable", size );
        }
    }
    return icon;
}
#endif

static void add_dir_btn( PageData* data, GMenuTreeDirectory* dir )
{
    GdkPixbuf* icon;
    GtkWidget* btn;
    const char* icon_name = gmenu_tree_directory_get_icon( dir );
    if( !icon_name )
        icon_name = "folder";

    icon = lxlauncher_load_icon( icon_name, IMG_SIZE, TRUE );

    btn = add_btn( data->table, gmenu_tree_directory_get_name(dir), icon, gmenu_tree_directory_get_comment(dir) );
    if( icon )
        g_object_unref( icon );

    g_object_set_data( btn, "dir", dir );
    g_signal_connect( btn, "clicked", G_CALLBACK(on_dir_btn_clicked), data );
}

static void add_app_btn( GtkWidget* table, GMenuTreeEntry* app )
{
    GdkPixbuf* icon;
    GtkWidget* btn;
    const char* icon_name = gmenu_tree_entry_get_icon( app );

    if( !icon_name )
        icon_name = "application-x-executable";

    icon = lxlauncher_load_icon( icon_name, IMG_SIZE, TRUE );

    btn = add_btn( table, gmenu_tree_entry_get_name(app), icon, gmenu_tree_entry_get_comment(app) );

    if( icon )
        g_object_unref( icon );
    g_signal_connect( btn, "clicked", G_CALLBACK(on_app_btn_clicked), app );
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

static char* menu_dir_to_path( GMenuTreeDirectory* dir )
{
    if( gmenu_tree_item_get_parent(dir) == root_dir )
        return g_strdup( gmenu_tree_directory_get_name(dir) );
    else
    {
        char* parent = menu_dir_to_path( gmenu_tree_item_get_parent(dir) );
        char* ret;
        ret = g_strconcat( parent, " > ", gmenu_tree_directory_get_name(dir), NULL );
        g_free( parent );
        return ret;
    }
    return NULL;
}

static void create_notebook_pages();

static PageData* notebook_page_from_dir( GMenuTreeDirectory* dir )
{
    int i, n;
    GtkWidget* page;
    GMenuTreeDirectory* top = dir;
    PageData* page_data;

    // get toplevel parent dir
    while( gmenu_tree_item_get_parent(top) != root_dir )
        top = gmenu_tree_item_get_parent(top);

    n = gtk_notebook_get_n_pages( notebook );
    for( i = 0; i < n; ++i )
    {

        GMenuTreeDirectory* top2;
        page = gtk_notebook_get_nth_page( notebook, i );
        page_data = (PageData*)g_object_get_data(page, "page");
        top2 = page_data->dir;
        while( gmenu_tree_item_get_parent(top2) != root_dir )
            top2 = gmenu_tree_item_get_parent(top2);

        if( top == top2 )
            return page_data;
    }
    return NULL;
}

static gboolean reload_apps()
{
    int i;
    PageData* page_data;
    GtkWidget* page;

    int n = gtk_notebook_get_n_pages( notebook );
    char** page_paths = g_new0(char*, n + 1);
    int cur_page = gtk_notebook_get_current_page(notebook);

    for( i = 0; i < n; ++i )
    {
        page = gtk_notebook_get_nth_page( notebook, 0 );
        page_data = (PageData*)g_object_get_data( page, "page" );
        page_paths[i] = gmenu_tree_directory_make_path( page_data->dir, NULL);
        gtk_notebook_remove_page( notebook, 0 );
    }

    // rebuild every pages
    create_notebook_pages();
    root_dir = gmenu_tree_get_root_directory( menu_tree );

    for( i = 0; i < n; ++i )
    {
        GMenuTreeDirectory* dir = gmenu_tree_get_directory_from_path( menu_tree, page_paths[i] );
        if( dir )
        {
            GMenuTreeDirectory* top = dir;
            // get toplevel parent dir

            while( gmenu_tree_item_get_parent(top) != root_dir )
                top = gmenu_tree_item_get_parent(top);

            // find notebook page containing the top dir
            page_data = notebook_page_from_dir( top );

            if( ! page_data )
                continue;

            if( page_data->dir != dir )
                notebook_page_chdir( page_data, dir );

            if( i == cur_page )
                gtk_notebook_set_current_page( notebook, cur_page );
        }
    }
    g_strfreev( page_paths );
    return FALSE;
}

void on_menu_tree_changed( GMenuTree *tree, gpointer  user_data )
{
    // some changes happened in applications dirs
    // reload is needed
    // g_debug( "file changed" );

    if( reload_handler )
        g_source_remove( reload_handler );

    // delay the reload deliberately to prevent frequent massive changes to the menu dirs 
    // due to system upgrade or something.
    reload_handler = g_timeout_add( 5000,(GSourceFunc)reload_apps, NULL );
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

static void notebook_page_chdir( PageData* data, GMenuTreeDirectory* dir )
{
    GSList* l;
    char* dir_path;
    GMenuTreeDirectory* parent_dir;

    data->dir = dir;

    // destroy old buttons
    gtk_container_foreach( data->table, gtk_widget_destroy, NULL );

    gtk_container_forall( data->go_up_bar, gtk_widget_destroy, NULL );

    for( l = gmenu_tree_directory_get_contents(dir); l; l = l->next )
    {
        GMenuTreeItem* item = (GMenuTreeItem*)l->data;
        GMenuTreeItemType type = gmenu_tree_item_get_type(item);
        if( type == GMENU_TREE_ITEM_DIRECTORY )
            add_dir_btn( data, (GMenuTreeDirectory*)item );
        else if( type == GMENU_TREE_ITEM_ENTRY )
        {
            if( gmenu_tree_entry_get_is_nodisplay(item) || gmenu_tree_entry_get_is_excluded(item) )
                continue;
            add_app_btn( data->table, (GMenuTreeEntry*)item );
        }
    }

    parent_dir = gmenu_tree_item_get_parent((GMenuTreeItem*)dir);

    if( parent_dir != root_dir )   // if dir has parent, not top-level group
    {
        GtkWidget* label;
        char* text = g_strdup_printf( _("Go back to \"%s\""), gmenu_tree_directory_get_name(parent_dir) );
        GtkWidget* btn = gtk_button_new_with_label( text );
        g_free( text );

        dir_path = menu_dir_to_path( dir );

        label = gtk_label_new( dir_path );
        g_free( dir_path );

        gtk_misc_set_alignment( label, 0.0, 0.5 );
        g_object_set_data( btn, "dir", parent_dir );
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

static void create_notebook_pages()
{
    GSList* l;

    root_dir = gmenu_tree_get_root_directory( menu_tree );

    // build pages for toplevel groups
	for( l = gmenu_tree_directory_get_contents(root_dir); l; l = l->next )
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
        PageData* page_data;
		GMenuTreeDirectory* dir = (GMenuTreeDirectory*)l->data;

		if( G_UNLIKELY( gmenu_tree_item_get_type((GMenuTreeItem*)dir) != GMENU_TREE_ITEM_DIRECTORY ) )
		    continue;

        page_data = g_new0( PageData, 1 );
        g_object_set_data_full( page_vbox, "page", page_data, page_data_free );

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
        image = gtk_image_new_from_icon_name( gmenu_tree_directory_get_icon(dir), GTK_ICON_SIZE_MENU );

		gtk_box_pack_start( label, image, FALSE, TRUE, 2 );
		gtk_box_pack_start( label, gtk_label_new( gmenu_tree_directory_get_name(dir) ), FALSE, TRUE, 2 );
		gtk_widget_show_all(label);

        // gtk_container_set_border_width( page_vbox, 4 );
        exo_wrap_table_set_col_spacing( table, 8 );

		viewport = gtk_viewport_new( NULL, NULL );
		gtk_container_add( viewport, table );
		gtk_container_add( scroll, viewport );
		gtk_widget_show_all( scroll );

        gtk_box_pack_start( page_vbox, go_up_bar, FALSE, TRUE, 0 );
        gtk_box_pack_start( page_vbox, scroll, TRUE, TRUE, 0 );
		gtk_widget_show_all( page_vbox );

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
        notebook_page_chdir( page_data, dir );
	}
}

int main(int argc, char** argv)
{
	int i;
	GdkRectangle working_area;
    GList* l;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

	gtk_init( &argc, &argv );

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

    menu_tree = gmenu_tree_lookup( DATA_DIR"/launcher.menu", GMENU_TREE_FLAGS_NONE );
    gmenu_tree_add_monitor( menu_tree, on_menu_tree_changed, NULL );

    create_notebook_pages();

    get_working_area( gtk_widget_get_screen(main_window), &working_area );
    // working_area.width = 800;
    // working_area.height = 480;
    gtk_window_move( main_window, working_area.x, working_area.y );
    gtk_window_resize( main_window, working_area.width, working_area.height );

	gtk_widget_show_all( main_window );
	gtk_main();

    gdk_window_remove_filter( gtk_widget_get_root_window(main_window), evt_filter, NULL );

    gmenu_tree_remove_monitor( menu_tree, on_menu_tree_changed, NULL );
    gmenu_tree_unref( menu_tree );

	return 0;
}
