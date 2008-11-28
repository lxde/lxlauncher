/*
 *      misc.c
 *      
 *      Copyright 2008 PCMan <pcman@debian>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "misc.h"
#include "vfs-utils.h"
#include "vfs-execute.h"

static GdkPixbuf* load_icon_file( const char* file_name, int size )
{
    GdkPixbuf* icon = NULL;
    char* file_path;
    const gchar** dirs = (const gchar**) g_get_system_data_dirs();
    const gchar** dir;
    for( dir = dirs; *dir; ++dir )
    {
        file_path = g_build_filename( *dir, "pixmaps", file_name, NULL );
        icon = gdk_pixbuf_new_from_file_at_scale( file_path, size, size, TRUE, NULL );
        g_free( file_path );
        if( icon )
            break;
    }
    return icon;
}

GdkPixbuf* lxlauncher_load_icon( const char* name, int size, gboolean use_fallback )
{
    GtkIconTheme* theme;
    char *icon_name = NULL, *suffix;
    GdkPixbuf* icon = NULL;

    if( name )
    {
        if( g_path_is_absolute( name) )
        {
            icon = gdk_pixbuf_new_from_file_at_scale( name,
                                                     size, size, TRUE, NULL );
        }
        else
        {
            theme = gtk_icon_theme_get_default();
            suffix = strchr( name, '.' );
            if( suffix
                && (0 == g_strcasecmp(++suffix, "png")
                || 0 == g_strcasecmp(suffix, "svg")
                || 0 == g_strcasecmp(suffix, "xpm")) ) /* has file extension, it's a basename of icon file */
            {
                /* try to find it in pixmaps dirs */
                icon = load_icon_file( name, size );
                if( G_UNLIKELY( ! icon ) )  /* unfortunately, not found */
                {
                    /* Let's remove the suffix, and see if this name can match an icon
                         in current icon theme */
                    icon_name = g_strndup( name,
                                           (suffix - name - 1) );
                    icon = vfs_load_icon( theme, icon_name, size );
                    g_free( icon_name );
                }
            }
            else  /* no file extension, it could be an icon name in the icon theme */
            {
                icon = vfs_load_icon( theme, name, size );
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


static gboolean can_desktop_entry_open_multiple_files( const char* exec )
{
    char* p;
    if( exec )
    {
        for( p = exec; *p; ++p )
        {
            if( *p == '%' )
            {
                ++p;
                switch( *p )
                {
                case 'U':
                case 'F':
                case 'N':
                case 'D':
                    return TRUE;
                case '\0':
                    return FALSE;
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

/*
* Parse Exec command line of app desktop file, and translate
* it into a real command which can be passed to g_spawn_command_line_async().
* file_list is a null-terminated file list containing full
* paths of the files passed to app.
* returned char* should be freed when no longer needed.
*/
static char* translate_app_exec_to_command_line( const char* pexec,
                                                 GList* file_list )
{
    char* file;
    GList* l;
    gchar *tmp;
    GString* cmd = g_string_new("");
    gboolean add_files = FALSE;

    for( ; *pexec; ++pexec )
    {
        if( *pexec == '%' )
        {
            ++pexec;
            switch( *pexec )
            {
            case 'U':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_filename_to_uri( (char*)l->data, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'u':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_filename_to_uri( file, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( file );
                    add_files = TRUE;
                }
                break;
            case 'F':
            case 'N':
                for( l = file_list; l; l = l->next )
                {
                    file = (char*)l->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_string_append_c( cmd, ' ' );
                    g_free( tmp );
                }
                add_files = TRUE;
                break;
            case 'f':
            case 'n':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'D':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_path_get_dirname( (char*)l->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'd':
                if( file_list && file_list->data )
                {
                    tmp = g_path_get_dirname( (char*)file_list->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'c':
                #if 0
                g_string_append( cmd, vfs_app_desktop_get_disp_name( app ) );
                #endif
                break;
            case 'i':
                /* Add icon name */
                #if 0
                if( vfs_app_desktop_get_icon_name( app ) )
                {
                    g_string_append( cmd, "--icon " );
                    g_string_append( cmd, vfs_app_desktop_get_icon_name( app ) );
                }
                #endif
                break;
            case 'k':
                /* Location of the desktop file */
                break;
            case 'v':
                /* Device name */
                break;
            case '%':
                g_string_append_c ( cmd, '%' );
                break;
            case '\0':
                goto _finish;
                break;
            }
        }
        else  /* not % escaped part */
        {
            g_string_append_c ( cmd, *pexec );
        }
    }
_finish:
    if( ! add_files )
    {
        g_string_append_c ( cmd, ' ' );
        for( l = file_list; l; l = l->next )
        {
            file = (char*)l->data;
            tmp = g_shell_quote( file );
            g_string_append( cmd, tmp );
            g_string_append_c( cmd, ' ' );
            g_free( tmp );
        }
    }

    return g_string_free( cmd, FALSE );
}

gboolean lxlauncher_execute_app( GdkScreen* screen,
                                     const char* working_dir,
                                     const char* desktop_entry_exec,
                                     const char* app_disp_name,
                                     GList* file_paths,
                                     gboolean in_terminal, /* not supported now */
                                     GError** err )
{
    char* exec = NULL;
    char* cmd;
    GList* l;
    gchar** argv = NULL;
    gint argc = 0;
    const char* sn_desc;

    if( desktop_entry_exec )
    {
        if ( ! strchr( desktop_entry_exec, '%' ) )
        { /* No filename parameters */
            exec = g_strconcat( desktop_entry_exec, " %f", NULL );
        }
        else
        {
            exec = g_strdup( desktop_entry_exec );
        }
    }

    if ( exec )
    {
        if( !screen )
            screen = gdk_screen_get_default();

        sn_desc = app_disp_name;
        if( !sn_desc )
            sn_desc = exec;
        if( can_desktop_entry_open_multiple_files( desktop_entry_exec ) )
        {
            cmd = translate_app_exec_to_command_line( desktop_entry_exec, file_paths );
            if ( cmd )
            {
                /* g_debug( "Execute %s\n", cmd ); */
                if( g_shell_parse_argv( cmd, &argc, &argv, NULL ) )
                {
                    vfs_exec_on_screen( screen, NULL, argv, NULL,
                                        sn_desc, VFS_EXEC_DEFAULT_FLAGS, err );
                    g_strfreev( argv );
                }
                g_free( cmd );
            }
            else
            {
                for( l = file_paths; l; l = l->next )
                {
                    cmd = translate_app_exec_to_command_line( desktop_entry_exec, l );
                    if ( cmd )
                    {
                        /* g_debug( "Execute %s\n", cmd ); */
                        if( g_shell_parse_argv( cmd, &argc, &argv, NULL ) )
                        {
                            vfs_exec_on_screen( screen, NULL,argv, NULL, sn_desc,
                                                G_SPAWN_SEARCH_PATH|
                                                G_SPAWN_STDOUT_TO_DEV_NULL|
                                                G_SPAWN_STDERR_TO_DEV_NULL,
                                    err );
                            g_strfreev( argv );
                        }
                        g_free( cmd );
                    }
                }
            }
            g_free( exec );
        }
        return TRUE;
    }

    g_set_error( err, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, _("Command not found") );
    return FALSE;
}
