/*
 *      misc.h
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

#ifndef _MISC_H_
#define _MISC_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean lxlauncher_execute_app( GdkScreen* screen,
                                     const char* working_dir,
                                     const char* desktop_entry_exec,
                                     const char* app_disp_name,
                                     GList* file_paths,
                                     gboolean in_terminal, /* not supported now */
                                     GError** err );

GdkPixbuf* lxlauncher_load_icon( const char* name, int size, gboolean use_fallback );

G_END_DECLS

#endif
