//      theme.c
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

#include "theme.h"

#define THEME_DIR       PACKAGE_DATA_DIR"/lxlauncher/themes"
#define THEME_FILE_NAME "theme.rc"

static void lxlauncher_background_free(LXLauncherBackground* bg)
{
    g_free(bg->name);
    g_slice_free(LXLauncherBackground, bg);
}

static LXLauncherBackground* lxlauncher_background_new(LXLauncherTheme* theme, GKeyFile* kf, const char* name)
{
    LXLauncherBackground* background = g_slice_new0(LXLauncherBackground);
    char* group = g_strconcat("Background ", name, NULL);
    char* str;
    g_debug("background name: %s", name);

    background->name = g_strdup(name);
    background->theme = theme;

    background->gradient_mode = g_key_file_get_integer(kf, group, "Gradient", NULL);

    str = g_key_file_get_string(kf, group, "Color1", NULL);
    if(str)
    {
        gdk_color_parse(str, &background->color1);
        g_free(str);
    }

    str = g_key_file_get_string(kf, group, "Color2", NULL);
    if(str)
    {
        gdk_color_parse(str, &background->color2);
        g_free(str);
    }

    background->image_mode = g_key_file_get_integer(kf, group, "ImageMode", NULL);

    str = g_key_file_get_string(kf, group, "Image", NULL);
    if(str)
    {
        if(g_path_is_absolute(str))
            background->image = str;
        else
        {
            background->image = g_build_filename(theme->theme_dir, str, NULL);
            g_free(str);
        }
    }

    str = g_key_file_get_value(kf, group, "Alpha", NULL);
    if(str)
    {
        background->alpha = g_ascii_strtod(str, NULL);
        g_free(str);
    }
    else
        background->alpha = 1.0;

    g_free(group);
    return background;
}

static LXLauncherTheme* lxlauncher_theme_new_form_dir(const char* dir, const char* name)
{
    LXLauncherTheme *theme = NULL;
    GKeyFile* kf = g_key_file_new();
    char* file = g_build_filename(dir, THEME_FILE_NAME, NULL);
    g_debug("load theme file: %s", file);
    if(g_key_file_load_from_file(kf, file, 0, NULL))
    {
        LXLauncherBackground* background;
        char** strs;
        char* default_background_name;
        theme = g_slice_new0(LXLauncherTheme);
        theme->theme_dir = g_strdup(dir);
        theme->name = g_strdup(name);
        theme->disp_name = g_key_file_get_locale_string(kf, "Theme", "Name", NULL, NULL);
        theme->rendered_cache = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
        theme->backgrounds = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, lxlauncher_background_free);
        theme->pages = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        strs = g_key_file_get_string_list(kf, "Theme", "Backgrounds", NULL, NULL);
        default_background_name = g_key_file_get_string(kf, "Theme", "DefaultBackground", NULL);
        if(strs)
        {
            char** pstr;
            for(pstr = strs; *pstr; ++pstr)
            {
                char* background_name = *pstr;
                background = lxlauncher_background_new(theme, kf, background_name);
                if(background)
                    g_hash_table_replace(theme->backgrounds, background->name, background);

                /* check if this is the default background */
                g_debug("back: %s", background_name);
                if(g_strcmp0(default_background_name, background_name) == 0)
                {
                    theme->default_background = background;
                    g_debug("default_background_name=%s", default_background_name);
                }
            }
            g_free(strs); /* strings in the factor are already freed, strfreev is not needed */
        }
        g_free(default_background_name);

        /* associate pages with backgrounds */
        strs = g_key_file_get_keys(kf, "Pages", NULL, NULL);
        if(strs) /* name of pages */
        {
            char** pstr;
            for(pstr = strs; *pstr; ++pstr)
            {
                LXLauncherBackground* background;
                char* page_id = *pstr;
                char* background_name = g_key_file_get_string(kf, "Pages", page_id, NULL);
                background = (LXLauncherBackground*)g_hash_table_lookup(theme->backgrounds, background_name);
                if(background)
                    g_hash_table_replace(theme->pages, page_id, background);
                else
                    g_free(page_id);
                g_free(background_name);
            }
            g_free(strs); /* strings in the factor are already freed, strfreev is not needed */
        }
    }

    g_key_file_free(kf);
    g_free(file);
    return theme;
}

LXLauncherTheme *lxlauncher_theme_new(const char* name)
{
    LXLauncherTheme* theme;
    char* dir;
    dir = g_build_filename(g_get_user_data_dir(), "lxlauncher/themes", name, NULL);
    theme = lxlauncher_theme_new_form_dir(dir, name);
    g_free(dir);
    if(!theme)
    {
        dir = g_build_filename(THEME_DIR, name, NULL);
        theme = lxlauncher_theme_new_form_dir(dir, name);
        g_free(dir);
    }
    return theme;
}

void lxlauncher_theme_free(LXLauncherTheme* theme)
{
    g_free(theme->theme_dir);
    g_free(theme->name);
    g_free(theme->disp_name);

    g_hash_table_destroy(theme->rendered_cache);
    g_hash_table_destroy(theme->backgrounds);
    g_hash_table_destroy(theme->pages);
    g_slice_free(LXLauncherTheme, theme);
}

char* lxlauncher_theme_get_gtkrc(LXLauncherTheme* theme)
{
    return g_build_filename(theme->theme_dir, "gtkrc", NULL);
}

static inline cairo_pattern_t* create_gradient_pattern(LXLauncherGradientMode mode,
                                gdouble x, gdouble y, gdouble w, gdouble h)
{
    cairo_pattern_t* pat;
    gdouble pat_x0, pat_y0, pat_x1, pat_y1;

    switch(mode)
    {
    case LXLG_NONE: /* no gradient */
        return NULL;
    case LXLG_HORZ: /* horizontal */
        pat_x0 = pat_y0 = pat_y1 = 0;
        pat_x1 = w;
        break;
    case LXLG_VERT: /* vertical */
        pat_x0 = pat_y0 = pat_x1 = 0;
        pat_y1 = h;
        break;
    case LXLG_LEFT_TOP_TO_RIGHT_BOTTOM: /* left top to right bottom */
        pat_x0 = pat_y0 = 0;
        pat_x1 = w;
        pat_y1 = h;
        break;
    case LXLG_RIGHT_TOP_TO_LEFT_BOTTOM: /* right top to left bottom */
        pat_x0 = w;
        pat_y0 = 0;
        pat_x1 = 0;
        pat_y1 = h;
        break;
    }
    pat = cairo_pattern_create_linear( pat_x0, pat_y0, pat_x1, pat_y1 );
    return pat;
}

static GdkPixbuf* _cairo_image_surface_to_pixbuf(cairo_surface_t* cs)
{
    int w = cairo_image_surface_get_width(cs);
    int h = cairo_image_surface_get_height(cs);
    guchar* data = cairo_image_surface_get_data(cs);
    GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    guchar* data2 = gdk_pixbuf_get_pixels(pixbuf);
    int len = gdk_pixbuf_get_rowstride(pixbuf) * h;
    int i;

    for(i = 0; i < len; i += 3)
    {
        /* RGB to BGR */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        data2[i] = *(data+2);
        data2[i+1] = *(data+1);
        data2[i+2] = *(data+0);
#else
        data2[i] = *(data+0);
        data2[i+1] = *(data+1);
        data2[i+2] = *(data+2);
#endif
        data += 4;
    }
    return pixbuf;
}

static void on_pixbuf_destroy(gpointer user_data, GObject* pix)
{
    LXLauncherTheme* theme = (LXLauncherTheme*)user_data;
    char* key = (char*)g_object_get_data(pix, "key");
    g_hash_table_remove(theme->rendered_cache, key);
    g_debug("remove form cache: %s", key);
    g_free(key);
}

GdkPixbuf* lxlauncher_background_render(LXLauncherBackground* bg, int w, int h)
{
    char* key = g_strdup_printf("%s-%d:%d", bg->name, w, h);
    GdkPixbuf* pixbuf = (GdkPixbuf*)g_hash_table_lookup(bg->theme->rendered_cache, key);
    if(!pixbuf)
    {
        guchar* data, *data2;
        int i, len;
        cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
        cairo_t* cr = cairo_create(cs);
        /* paint gradient or solid background color */
        cairo_pattern_t* pat = create_gradient_pattern(bg->gradient_mode, 0, 0, w, h);
        if(pat)
        {
            cairo_pattern_add_color_stop_rgb(pat, 0,
                        (gdouble)bg->color1.red / 65535,
                        (gdouble)bg->color1.green / 65535,
                        (gdouble)bg->color1.blue / 65535);

            cairo_pattern_add_color_stop_rgb(pat, 1.0,
                        (gdouble)bg->color2.red / 65535,
                        (gdouble)bg->color2.green / 65535,
                        (gdouble)bg->color2.blue / 65535);

            cairo_set_source(cr, pat);
        }
        else
            gdk_cairo_set_source_color(cr, &bg->color1);

        cairo_paint(cr);

        if(pat)
            cairo_pattern_destroy(pat);

        /* composite the image if available. */
        if(bg->image)
        {
            int x, y;
            /* FIXME: should we cache this image? */
            GdkPixbuf* pixbuf2 = gdk_pixbuf_new_from_file(bg->image, NULL);
            GdkPixbuf* scaled;
            switch(bg->image_mode)
            {
            case LXLB_STRETH:
                scaled = gdk_pixbuf_scale_simple(pixbuf2, w, h, GDK_INTERP_BILINEAR);
                break;
/*
            case LXLB_FIT:
            {
                double ratio;
                if(w > h)
                    ratio = (double)h/w;
                else if(w < h)
                    ratio = (double)w/h;
                else
                    ratio = 1;
                if(ratio != 1)
                    scaled = gdk_pixbuf_scale_simple(pixbuf2, (double)w * ratio, (double)h * ratio, GDK_INTERP_BILINEAR);
                else
                    scaled = g_object_ref(pixbuf2);
                break;
            }
            case LXLB_TILE:
                scaled = g_object_ref(pixbuf2);
                break;
*/
            }
            g_object_unref(pixbuf2);
            x = (gdk_pixbuf_get_width(scaled) - w)/2;
            y = (gdk_pixbuf_get_height(scaled) - h)/2;

            gdk_cairo_set_source_pixbuf(cr, scaled, x, y);
            cairo_paint_with_alpha(cr, bg->alpha);

            g_object_unref(scaled);
        }

        pixbuf = _cairo_image_surface_to_pixbuf(cs);

        cairo_destroy(cr);
        cairo_surface_destroy(cs);

        /* cache the render result */
        if(pixbuf)
        {
            g_hash_table_insert(bg->theme->rendered_cache, key, pixbuf);
            g_object_set_data(pixbuf, "key", key);
            g_object_weak_ref(pixbuf, on_pixbuf_destroy, bg->theme);
        }
        else
            g_free(key);
    }
    else
    {
        g_debug("found in cache: %s", key);
        g_object_ref(pixbuf);
        g_free(key);
    }
    return pixbuf;
}

LXLauncherBackground* lxlauncher_theme_get_background(LXLauncherTheme* theme, const char* page_id)
{
    LXLauncherBackground* background;
    background = (LXLauncherBackground*)g_hash_table_lookup(theme->pages, page_id);
    if(!background)
        background = theme->default_background;
    return background;
}

