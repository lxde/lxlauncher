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

