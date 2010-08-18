//      theme.h
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.


#ifndef __THEME_H__
#define __THEME_H__

#include <glib.h>
#include <cairo/cairo.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _LXLauncherTheme             LXLauncherTheme;
typedef struct _LXLauncherBackground        LXLauncherBackground;
typedef enum _LXLauncherBackgroundMode      LXLauncherBackgroundMode;
typedef enum _LXLauncherGradientMode        LXLauncherGradientMode;

enum _LXLauncherBackgroundMode
{
    LXLB_STRETH,
    LXLB_FIT,
    LXLB_TILE,
};

enum _LXLauncherGradientMode
{
    LXLG_NONE, /* no gradient */
    LXLG_HORZ, /* horizontal */
    LXLG_VERT, /* vertical */
    LXLG_LEFT_TOP_TO_RIGHT_BOTTOM, /* left top to right bottom */
    LXLG_RIGHT_TOP_TO_LEFT_BOTTOM, /* right top to left bottom */
};

struct _LXLauncherBackground
{
    LXLauncherTheme* theme;
    char* name;

    /* gradient colors */
    LXLauncherGradientMode gradient_mode;
    GdkColor color1;
    GdkColor color2;

    /* background image */
    LXLauncherBackgroundMode image_mode;
    char* image; /* path of background image */
    double alpha; /* alpha blending */
};

struct _LXLauncherTheme
{
    char* theme_dir;
    char* name;
    char* disp_name;
    GHashTable* pages;
    GHashTable* backgrounds;
    LXLauncherBackground* default_background;
    GHashTable* rendered_cache;
};


LXLauncherTheme* lxlauncher_theme_new(const char* name);
char* lxlauncher_theme_get_gtkrc(LXLauncherTheme* theme);
void lxlauncher_theme_free(LXLauncherTheme* theme);

LXLauncherBackground* lxlauncher_theme_get_background(LXLauncherTheme* theme, const char* page_id);

/* render the theme to a pixbuf */
GdkPixbuf* lxlauncher_background_render(LXLauncherBackground* bg, int w, int h);

G_END_DECLS

#endif /* __THEME_H__ */
