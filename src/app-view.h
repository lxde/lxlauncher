/*
 *      app-view.h
 *
 *      Copyright 2009 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __APP_VIEW_H__
#define __APP_VIEW_H__

#include <gtk/gtk.h>
#include <menu-cache.h>


G_BEGIN_DECLS

#define APP_VIEW_TYPE				(app_view_get_type())
#define APP_VIEW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			APP_VIEW_TYPE, AppView))
#define APP_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			APP_VIEW_TYPE, AppViewClass))
#define IS_APP_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			APP_VIEW_TYPE))
#define IS_APP_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			APP_VIEW_TYPE))

typedef struct _AppView			AppView;
typedef struct _AppViewClass		AppViewClass;
typedef struct _AppBtn AppBtn;

struct _AppBtn
{
    GdkPixbuf* icon;
    MenuCacheItem* item;
    GdkRectangle icon_rect;
    GdkRectangle text_rect;
    GdkRectangle box;
};

struct _AppView
{
	GtkWidget parent;

    /* private: */
	GList* btns;
    AppBtn* cur_btn;

    guint icon_size;
    guint btn_size;
    guint n_cols;
    guint n_rows;
    gboolean mouse_down;

    PangoFontDescription* font;
    GtkAdjustment* adj;
	
	guint first_row;
	guint y_off;
	GList* first_visible;
};

struct _AppViewClass
{
	GtkWidgetClass parent_class;
    void (*clicked)(int mouse_btn, AppBtn* item);
};

GType		app_view_get_type		(void);
GtkWidget*	app_view_new			(void);

AppBtn* app_view_add_button(AppView* av, MenuCacheItem* item);
void app_view_remove_button(AppView* av, AppBtn* btn);
void app_view_remove_nth_button(AppView* av, int n_btn);
void app_view_remove_all(AppView* av);

GtkAdjustment* app_view_get_adjustment(AppView* av);
void app_view_set_button_size(AppView* av, int size);
void app_view_set_icon_size(AppView* av, int size);


G_END_DECLS

#endif /* __APP_VIEW_H__ */
