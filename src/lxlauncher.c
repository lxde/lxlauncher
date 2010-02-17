/*
 *      lxlauncher.c - Open source replace for EeePC Asus Launcher
 *
 *      Copyright 2008-2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include <errno.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <cairo.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gio/gdesktopappinfo.h>

#include <menu-cache.h>

#include "app-view.h"
#include "working-area.h"
#include "misc.h"

#define CONFIG_FILE "lxlauncher/settings.conf"

#define DATA_DIR  PACKAGE_DATA_DIR"/lxlauncher"
#define BACKGROUND_DIR  PACKAGE_DATA_DIR"/lxlauncher/background"
#define ICON_DIR        PACKAGE_DATA_DIR"/lxlauncher/icons"

#define BUTTON_SIZE_FALLBACK 120
#define IMG_SIZE_FALLBACK 48

static GtkWidget* main_window;
static GtkWidget* notebook;
static int n_cols;
static GtkIconSize icon_size;

static Atom atom_NET_WORKAREA = None;

static MenuCache* menu_tree = NULL;
static MenuCacheDir* root_dir = NULL;

static int reload_handler = 0;
static gpointer reload_notify_id;

static GKeyFile *key_file;

static gint button_size;
static gint img_size;

typedef struct _PageData{
    MenuCacheDir* dir;
    char* dir_path;
    GtkWidget* page_vbox;
    GtkWidget* page_scrollbar;
    GtkBox* go_up_bar;
    GtkWidget* app_view;
    GdkPixbuf* background;
}PageData;

static void on_btn_clicked(AppView* av, int mouse_btn, AppBtn* btn, PageData* data);

static void on_menu_item_properties(GtkMenuItem* item, MenuCacheApp* app)
{
    /* FIXME: if the source desktop is in AppDir other then default
     * applications dirs, where should we store the user-specific file?
    */
    char* ifile = menu_cache_item_get_file_path(app);
    char* ofile = g_build_filename(g_get_user_data_dir(), "applications",
                                  menu_cache_item_get_file_basename(app), NULL);
    char** argv[] = {
        "lxshortcut",
        "-i",
        NULL,
        "-o",
        NULL,
        NULL};
    argv[2] = ifile;
    argv[4] = ofile;
    g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL );
    g_free( ifile );
    g_free( ofile );
}

static void notebook_page_chdir( PageData* data, MenuCacheDir* dir );

static char* menu_dir_to_path( MenuCacheDir* dir )
{
    if( menu_cache_item_get_parent(dir) == root_dir )
        return g_strdup( menu_cache_item_get_name(dir) );
    else
    {
        char* parent = menu_dir_to_path( menu_cache_item_get_parent(dir) );
        char* ret;
        ret = g_strconcat( parent, " > ", menu_cache_item_get_name(dir), NULL );
        g_free( parent );
        return ret;
    }
    return NULL;
}

static void create_notebook_pages();

static PageData* notebook_page_from_dir( MenuCacheDir* dir )
{
    int i, n;
    GtkWidget* page;
    MenuCacheDir* top = dir;
    PageData* page_data;

    // get toplevel parent dir
    while( menu_cache_item_get_parent(top) != root_dir )
        top = menu_cache_item_get_parent(top);

    n = gtk_notebook_get_n_pages( notebook );
    for( i = 0; i < n; ++i )
    {
        MenuCacheDir* top2;
        page = gtk_notebook_get_nth_page( notebook, i );
        page_data = (PageData*)g_object_get_data(page, "page");
        top2 = page_data->dir;
        while( menu_cache_item_get_parent(top2) != root_dir )
            top2 = menu_cache_item_get_parent(top2);

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
        page_paths[i] = g_strdup(page_data->dir_path);
        gtk_notebook_remove_page( notebook, 0 );
    }

    // rebuild every pages
    create_notebook_pages();

    for( i = 0; i < n; ++i )
    {
        MenuCacheDir* dir = menu_cache_get_dir_from_path( menu_tree, page_paths[i] );

        if( dir )
        {
            MenuCacheDir* top = dir;
            // get toplevel parent dir

            while( menu_cache_item_get_parent(top) != root_dir )
                top = menu_cache_item_get_parent(top);

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

void on_menu_tree_changed( MenuCache *tree, gpointer  user_data )
{
    // some changes happened in applications dirs
    // reload is needed
    // g_debug( "file changed" );
    menu_cache_item_unref(root_dir);
    root_dir = menu_cache_ref(menu_cache_get_root_dir(tree));
    reload_apps();
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

static void on_go_up(GtkWidget* go_up_btn, PageData* data )
{
    MenuCacheDir* dir = MENU_CACHE_DIR(g_object_get_data(go_up_btn, "dir"));
    notebook_page_chdir( data, dir );
}

static void notebook_page_chdir( PageData* data, MenuCacheDir* dir )
{
    GSList* l;
    char* dir_path;
    MenuCacheDir* parent_dir;

    if(data->dir)
        menu_cache_item_unref(data->dir);
    data->dir = menu_cache_item_ref(dir);
    g_free(data->dir_path);
    data->dir_path = menu_cache_dir_make_path( dir );

    // destroy old buttons
    app_view_remove_all(data->app_view);

    gtk_container_forall( data->go_up_bar, gtk_widget_destroy, NULL );

    if( G_UNLIKELY( !dir ) )
        return;

    for( l = menu_cache_dir_get_children(dir); l; l = l->next )
    {
        MenuCacheItem* item = (MenuCacheItem*)l->data;
        MenuCacheType type = menu_cache_item_get_type(item);

        if( type == MENU_CACHE_TYPE_APP )
        {
            if( ! menu_cache_app_get_is_visible(item, SHOW_IN_LXDE) )
                continue;
        }
        app_view_add_button(data->app_view, item);
    }

    parent_dir = menu_cache_item_get_parent((MenuCacheItem*)dir);

    if( parent_dir != root_dir )   // if dir has parent, not top-level group
    {
        GtkWidget* label;
        char* text = g_strdup_printf( _("Go back to \"%s\""), menu_cache_item_get_name(parent_dir) );
        GtkWidget* btn = gtk_button_new_with_label( text );
        g_free( text );

        dir_path = menu_dir_to_path( dir );

        label = gtk_label_new( dir_path );
        g_free( dir_path );

        gtk_misc_set_alignment( label, 0.0, 0.5 );
        g_object_set_data( btn, "dir", parent_dir );
        g_signal_connect( btn, "clicked", on_go_up, data );
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
    if(data->dir)
        menu_cache_item_unref( data->dir );
    g_free(data->dir_path);
    g_free( data );
}

void on_btn_clicked(AppView* av, int mouse_btn, AppBtn* btn, PageData* data)
{
    GtkWidget* widget = (GtkWidget*)av;
    if( mouse_btn == 1 ) /* left click */
    {
        if( menu_cache_item_get_type(btn->item) == MENU_CACHE_TYPE_APP )
        {
            GDesktopAppInfo* app;
            GdkAppLaunchContext* ctx;
            char* file = menu_cache_item_get_file_path(btn->item);
            app = g_desktop_app_info_new_from_filename(file);
            g_free(file);

            gtk_widget_queue_draw_area(widget, btn->box.x, btn->box.y, btn->box.width, btn->box.height);

            ctx = gdk_app_launch_context_new();
            gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(widget));
            /* FIXME: should we pass time to "clicked" handler? */
            gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());

            g_app_info_launch(app, NULL, ctx, NULL);
            g_object_unref(app);

            g_object_unref(ctx);
        }
        else /* dir */
        {
            notebook_page_chdir( data, MENU_CACHE_DIR(btn->item) );
        }
    }
    else if( mouse_btn == 3 ) /* right click */
    {
        if( menu_cache_item_get_type(btn->item) == MENU_CACHE_TYPE_APP )
        {
            char* tmp;
            GtkWidget* item;
            GtkMenu* p = gtk_menu_new();
            tmp = g_find_program_in_path("lxshortcut");
            if( tmp )
            {
                /*
                item = gtk_separator_menu_item_new();
                gtk_menu_shell_append(p, item);
                */
                item = gtk_menu_item_new_with_mnemonic(_("_Customize"));
                g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_properties), btn->item);
                gtk_menu_shell_append(p, item);
                g_free(tmp);
            }
            else
            {
                /* FIXME: since currently Properties is the only menu item,
                 *        we don't popup the menu if it's empty */
                gtk_widget_destroy(p);
                return FALSE;
            }
            g_signal_connect(p, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);
g_debug("HERE");
            gtk_widget_show_all(p);
            gtk_menu_popup(p, NULL, NULL, NULL, NULL, NULL, gtk_get_current_event_time());
        }
    }
}

static void on_adjustment_changed(GtkAdjustment* adj, GtkWidget* scroll)
{
	if(adj->upper > adj->page_size)
		gtk_widget_show(scroll);
	else
		gtk_widget_hide(scroll);
}


static void create_notebook_pages()
{
    GSList* l;

    // build pages for toplevel groups
    for( l = menu_cache_dir_get_children(root_dir); l; l = l->next )
    {
        GtkWidget* *viewport;
        GtkAdjustment* adj;
        GtkWidget* page_vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget* page_hbox = gtk_hbox_new(FALSE, 0);
        GtkWidget* app_view;
        GtkWidget* label;
        GtkWidget* image;
		GtkWidget* scrollbar;
        GtkWidget* go_up_bar = gtk_hbox_new( FALSE, 2 );
        GdkPixbuf* pixbuf=NULL;
        GdkPixmap* pixmap;
        GdkGC *pixmap_gc=NULL;
        char* file;
        PageData* page_data;
        MenuCacheDir* dir = (MenuCacheDir*)l->data;

        if( G_UNLIKELY( menu_cache_item_get_type((MenuCacheItem*)dir) != MENU_CACHE_TYPE_DIR ) )
            continue;

        app_view = app_view = app_view_new();

        page_data = g_new0( PageData, 1 );
        g_object_set_data_full( page_vbox, "page", page_data, page_data_free );

        g_signal_connect(app_view, "clicked", G_CALLBACK(on_btn_clicked), page_data );

        label = gtk_hbox_new( FALSE, 2 );

        /* create tab label for notebook page */
        image = gtk_image_new_from_icon_name( menu_cache_item_get_icon(dir), GTK_ICON_SIZE_MENU );
        gtk_box_pack_start( label, image, FALSE, TRUE, 2 );
        gtk_box_pack_start( label, gtk_label_new( menu_cache_item_get_name(dir) ), FALSE, TRUE, 2 );
        gtk_widget_show_all(label);

		/* app view is packed together with a vscrollbar in hbox */
        gtk_box_pack_start( page_hbox, app_view, TRUE, TRUE, 0 );
		scrollbar = gtk_vscrollbar_new(NULL);
		gtk_range_set_adjustment(scrollbar, app_view_get_adjustment(app_view));
		gtk_widget_show(scrollbar);
        gtk_box_pack_start( page_hbox, scrollbar, FALSE, TRUE, 0 );
		g_signal_connect(app_view_get_adjustment(app_view), "changed", G_CALLBACK(on_adjustment_changed), scrollbar);

		/* pack the go up bar and app view into the notebook page */
        gtk_box_pack_start( page_vbox, go_up_bar, FALSE, TRUE, 0 );
        gtk_box_pack_start( page_vbox, page_hbox, TRUE, TRUE, 0 );
        gtk_widget_show_all( page_vbox );

		/* add the newly created page to notebook */
        gtk_notebook_append_page( notebook, page_vbox, label );

        page_data->page_vbox = page_vbox;
		page_data->page_scrollbar = scrollbar;
        page_data->go_up_bar = go_up_bar;
        page_data->app_view = app_view;

		/* chdir to load the apps in this page */
		/* FIXME: do this in a async fashion to speed app startup. */
        notebook_page_chdir( page_data, dir );
    }
}

gchar* get_xdg_config_file(const char *name) {
    const gchar *user_dir = g_get_user_config_dir();
    const gchar **system_dirs = g_get_system_config_dirs();
    const gchar **dir;
    gchar *file;

    file = g_build_filename(user_dir, name, NULL);
    if (g_file_test(file, G_FILE_TEST_EXISTS) == TRUE)
    	return file;
    free(file);

    for (dir = system_dirs; *dir; ++dir ) {
        file = g_build_filename(*dir, name, NULL);
        if (g_file_test(file, G_FILE_TEST_EXISTS) == TRUE)
            return file;
	    free(file);
    }
    return NULL;
}

static void on_menu_cache_reload(MenuCache* mc, gpointer user_data)
{
    GMainLoop* mainloop = (GMainLoop*)user_data;
    g_main_loop_quit(mainloop);
}

MenuCache* _menu_cache_lookup_sync( const char* menu_name )
{
    MenuCache* mc = menu_cache_lookup(menu_name);
    /* ensure that the menu cache is loaded */
    if(! menu_cache_get_root_dir(mc)) /* if it's not yet loaded */
    {
        GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);
        gpointer notify_id = menu_cache_add_reload_notify(mc, on_menu_cache_reload, mainloop);
        g_main_loop_run(mainloop);
        g_main_loop_unref(mainloop);
        menu_cache_remove_reload_notify(mc, notify_id);
    }
    return mc;
}

int main(int argc, char** argv)
{
    int i;
    GdkRectangle working_area;
    GList* l;
    gboolean enable_key=0;
    GtkSettings *settings;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    key_file = g_key_file_new();

    GError *error = NULL;
    gchar *config_file = get_xdg_config_file(CONFIG_FILE);
    if (config_file &&
        g_key_file_load_from_file(key_file,
                      config_file,
                      G_KEY_FILE_NONE,
                      &error)) {
    	printf("Loaded %s\n", config_file);
    }
    else {
    	perror("Error loading " CONFIG_FILE);
    }
    if (config_file)
	free(config_file);

    gtk_init( &argc, &argv );

    // Add application specific properties
    gtk_settings_install_property(g_param_spec_boolean("lxlauncher-enable-key",
                            _("Enable key navigation"),
                            _("Allow users to use up/down/left/right/tab/enter keys to operate the lxlauncher"),
                            FALSE,GTK_ARG_READWRITE));
    // set up themes for notebook
    gchar* gtkrc_file = get_xdg_config_file("lxlauncher/gtkrc");
    gtk_rc_parse(gtkrc_file);
    if (gtkrc_file) {
    	free(gtkrc_file);
    }

    button_size = g_key_file_get_integer(key_file, "Main", "BUTTON_SIZE", NULL);
    img_size = g_key_file_get_integer(key_file, "Main", "IMG_SIZE", NULL);

    // to prevent from going without configure file
    if(!button_size)
        button_size = BUTTON_SIZE_FALLBACK;
    if(!img_size)
        img_size = IMG_SIZE_FALLBACK;

    icon_size = gtk_icon_size_register( "ALIcon", img_size, img_size );

    main_window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_move( main_window, 0, 0 );
    gtk_window_set_skip_pager_hint( main_window, TRUE );
    gtk_window_set_skip_taskbar_hint( main_window, TRUE );

    get_working_area( gtk_widget_get_screen(main_window), &working_area );
	gtk_widget_set_size_request( main_window, working_area.width, working_area.height );
    gtk_window_move( main_window, working_area.x, working_area.y );

    gtk_widget_realize( main_window );
    gdk_window_set_keep_below( main_window->window, TRUE );
    //gdk_window_set_decorations( main_window->window );
    gdk_window_set_type_hint( main_window->window, GDK_WINDOW_TYPE_HINT_DESKTOP );
    gtk_window_set_position( main_window, GTK_WIN_POS_NONE );
    //gtk_window_set_gravity(GDK_GRAVITY_STATIC );

    g_signal_connect(main_window, "delete-event", G_CALLBACK(window_delete), NULL);

    atom_NET_WORKAREA = XInternAtom( GDK_DISPLAY(), "_NET_WORKAREA", True);
    XSelectInput(GDK_DISPLAY(), GDK_WINDOW_XID(gtk_widget_get_root_window(main_window)), PropertyChangeMask );
    gdk_window_add_filter( gtk_widget_get_root_window(main_window), evt_filter, NULL );

    notebook = gtk_notebook_new();
    settings = gtk_widget_get_settings(GTK_WIDGET(main_window));
    g_object_get(settings, "lxlauncher-enable-key", &enable_key,NULL);

    if (!enable_key)
        GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS );
    gtk_container_add( (GtkContainer*)main_window, notebook );

    g_setenv("XDG_MENU_PREFIX", "lxlauncher-", TRUE);
    menu_tree = _menu_cache_lookup_sync( "applications.menu" );
    if(!menu_tree)
    {
        g_print("Unable to load application menu\n");
        return 1;
    }
    root_dir = menu_cache_item_ref(menu_cache_get_root_dir( menu_tree ));
    reload_notify_id = menu_cache_add_reload_notify( menu_tree, on_menu_tree_changed, NULL );

    create_notebook_pages();

    gtk_widget_show_all( main_window );
    gtk_main();

    gdk_window_remove_filter( gtk_widget_get_root_window(main_window), evt_filter, NULL );
    menu_cache_remove_reload_notify( menu_tree, reload_notify_id );
    menu_cache_item_unref( root_dir );
    menu_cache_unref( menu_tree );

    return 0;
}
