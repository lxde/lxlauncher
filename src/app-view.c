/*
 *      app-view.c
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

#include "app-view.h"
#include "misc.h"

enum {
    CLICKED,
    LAST_SIGNAL
};

static void app_view_class_init(AppViewClass *klass);
static void app_view_init(AppView *self);
static void app_view_finalize(GObject *object);

static void app_view_realize(GtkWidget *widget);
static void app_view_unrealize(GtkWidget *widget);
static void app_view_map(GtkWidget *widget);
//static void app_view_unmap(GtkWidget *widget);
static void app_view_size_request(GtkWidget *widget, GtkRequisition *requisition);
static void app_view_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static gint app_view_expose(GtkWidget *widget, GdkEventExpose *evt);
static gboolean app_view_enter_notify(GtkWidget* widget, GdkEventCrossing* evt);
static gboolean app_view_leave_notify(GtkWidget* widget, GdkEventCrossing* evt);
static gboolean app_view_motion_notify(GtkWidget* widget, GdkEventMotion* evt);
static gboolean app_view_button_press(GtkWidget* widget, GdkEventButton* evt);
static gboolean app_view_button_release(GtkWidget* widget, GdkEventButton* evt);
static gboolean app_view_query_tooltip(GtkWidget *widget,gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data);

static void on_scroll(GtkAdjustment* adj, AppView* av);

static void app_view_relayout(AppView* av);

/* Local data */
static GtkWidgetClass *parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

/*
static AppBtn* app_btn_new(MenuCacheItem* item)
{
    AppBtn* btn = g_slice_new(AppBtn);
    btn->icon = NULL;
    btn->item = menu_cache_item_ref(item);
    return item;
}
*/

static void app_btn_free(AppBtn* btn)
{
    if(G_LIKELY(btn->icon))
        g_object_unref(btn->icon);
    if(G_LIKELY(btn->item))
        menu_cache_item_unref(btn->item);
    g_slice_free(AppBtn, btn);
}

G_DEFINE_TYPE (AppView, app_view, GTK_TYPE_WIDGET)

static void app_view_class_init(AppViewClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_object_class->finalize = app_view_finalize;

	parent_class = (GtkWidgetClass*)g_type_class_peek(GTK_TYPE_WIDGET);

    widget_class->realize = app_view_realize;
    widget_class->unrealize = app_view_unrealize;
//    widget_class->map = app_view_map;
//    widget_class->unmap = app_view_unmap;
    widget_class->size_request = app_view_size_request;
    widget_class->size_allocate = app_view_size_allocate;
    widget_class->expose_event = app_view_expose;
    widget_class->motion_notify_event = app_view_motion_notify;
    widget_class->enter_notify_event = app_view_enter_notify;
    widget_class->leave_notify_event = app_view_leave_notify;
    widget_class->button_press_event = app_view_button_press;
    widget_class->button_release_event = app_view_button_release;
    widget_class->query_tooltip = app_view_query_tooltip;

    /* signals */
    signals[CLICKED] = g_signal_new ("clicked",
                              G_OBJECT_CLASS_TYPE (g_object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (AppViewClass, clicked),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__UINT_POINTER,
                              G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
}


static void app_view_init(AppView *self)
{
	self->adj = gtk_adjustment_new(0, 0, 0, 1, 1, 1);
    self->btn_size = 120;
    self->icon_size = 64;
    gtk_widget_set_has_tooltip((GtkWidget*)self, TRUE);
	g_signal_connect(self->adj, "value-changed", G_CALLBACK(on_scroll), self);
}


GtkWidget* app_view_new(void)
{
	return (GtkWidget*)g_object_new(APP_VIEW_TYPE, NULL);
}


static void app_view_finalize(GObject *object)
{
	AppView *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_APP_VIEW(object));

	self = APP_VIEW(object);
	
	if(self->adj)
		g_object_unref(self->adj);

	if (G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


static void
app_view_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED|GTK_APP_PAINTABLE);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
            | GDK_POINTER_MOTION_MASK
            | GDK_BUTTON_MOTION_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK
            | GDK_EXPOSURE_MASK
            | GDK_ENTER_NOTIFY_MASK
            | GDK_LEAVE_NOTIFY_MASK;

    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    attributes.wclass = GDK_INPUT_OUTPUT;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                   &attributes, attributes_mask);
    gdk_window_set_user_data (widget->window, widget);

    widget->style = gtk_style_attach (widget->style, widget->window);
    gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
//    gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
app_view_unrealize (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->unrealize)
        (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
app_view_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
    AppView* av = (AppView*)widget;
    requisition->width = av->btn_size;
    requisition->height = av->btn_size;
}

static void
app_view_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget))
    {
        gdk_window_move_resize (widget->window,
                    allocation->x,
                    allocation->y,
                    allocation->width,
                    allocation->height);
    }
    app_view_relayout((AppView*)widget);
}


static void paint_btn(AppView* av, cairo_t* cr, AppBtn* btn)
{
    GtkWidget* widget = (GtkWidget*)av;
    MenuCacheItem* item = btn->item;
    const char* name = menu_cache_item_get_name(item);

    if( btn == av->cur_btn )
    {
        gtk_paint_box(widget->style, widget->window,
            av->mouse_down ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT,
            GTK_SHADOW_NONE, &btn->box, widget, "button",
            btn->box.x, btn->box.y, btn->box.width, btn->box.height );
    }

    if( G_LIKELY(btn->icon) )
    {
        int x, y;
        int icon_w = gdk_pixbuf_get_width(btn->icon);
        int icon_h = gdk_pixbuf_get_height(btn->icon);
        if( G_UNLIKELY(icon_w > btn->box.width) )
            icon_w = btn->box.width;
        if( G_UNLIKELY(icon_h > av->icon_size) )
            icon_h = av->icon_size;
        x = btn->box.x + (btn->box.width - icon_w)/2;
        y = btn->box.y + (av->icon_size - icon_h)/2;
        gdk_cairo_set_source_pixbuf(cr, btn->icon, x, y);
        cairo_rectangle(cr, x, y, icon_w, icon_h);
        cairo_fill(cr);
    }

    if( G_LIKELY(name) )
    {
        PangoContext* pc = gtk_widget_get_pango_context(widget);
        PangoLayout* pl = pango_layout_new(pc);
        pango_layout_set_width(pl, av->btn_size * PANGO_SCALE);
        pango_layout_set_height(pl, (av->btn_size - av->icon_size) * PANGO_SCALE);
        pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_END );
        pango_layout_set_wrap(pl, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_alignment(pl, PANGO_ALIGN_CENTER);
        pango_layout_set_text(pl, name, -1);
        gdk_cairo_set_source_color(cr, &widget->style->fg[GTK_STATE_NORMAL]);
        cairo_move_to(cr, btn->box.x, btn->box.y + av->icon_size);
        pango_cairo_show_layout(cr, pl);
        g_object_unref(pl);
    }
}

static gint app_view_expose(GtkWidget *widget, GdkEventExpose *evt)
{
    AppView* av = (AppView*)widget;
    if (GTK_WIDGET_DRAWABLE (widget))
    {
        GList* l;
        cairo_t *cr;
        cairo_pattern_t* pat;

        cr = gdk_cairo_create (widget->window);

        pat = cairo_pattern_create_linear( 0, 0, 0, widget->allocation.height );
        cairo_pattern_add_color_stop_rgb( pat, 0, 1.0, 1.0, 1.0);
        cairo_pattern_add_color_stop_rgb( pat, 1.0, ((gdouble)184/256), ((gdouble)215/256), ((gdouble)235/256));

        cairo_set_source(cr, pat);

        cairo_rectangle(cr, evt->area.x, evt->area.y, evt->area.width, evt->area.height);

        cairo_fill(cr);
        cairo_pattern_destroy(pat);

        for( l = av->first_visible; l; l = l->next )
        {
            AppBtn* btn = (AppBtn*)l->data;
            if( gdk_rectangle_intersect( &btn->box, &evt->area, NULL) )
                paint_btn(av, cr, btn);
        }

        cairo_destroy(cr);
    }
    return FALSE;
}

AppBtn* hit_test(AppView* av, long x, long y)
{
    GtkWidget* w = (GtkWidget*)av;
    GList* l;
/*
    x /= av->btn_size;
    if( x >= av->n_cols )
        return NULL;
    y /= av->btn_size;
    l = g_list_nth(av->btns, y * (w->allocation.width / av->btn_size) + x );
    return l ? (AppBtn*)l->data : NULL;
*/
	/* FIXME: iterate all btns is not efficient. */
	for(l=av->first_visible; l; l=l->next)
	{
		AppBtn* btn = (AppBtn*)l->data;
		if( btn->box.x < x && x < (btn->box.x + btn->box.width) )
		{
			if( btn->box.y < y && y < (btn->box.y + btn->box.height) )
				return btn;
		}
	}
	return NULL;
}

gboolean app_view_enter_notify(GtkWidget* widget, GdkEventCrossing* evt)
{
    return FALSE;
}

gboolean app_view_leave_notify(GtkWidget* widget, GdkEventCrossing* evt)
{
    AppView* av = (AppView*)widget;
    if(av->cur_btn)
    {
        AppBtn* prev = av->cur_btn;
        gtk_widget_queue_draw_area(widget, prev->box.x, prev->box.y, prev->box.width, prev->box.height);
        av->cur_btn = NULL;
    }
    return FALSE;
}

gboolean app_view_motion_notify(GtkWidget* widget, GdkEventMotion* evt)
{
    AppView* av = (AppView*)widget;
    AppBtn* btn = hit_test(widget, evt->x, evt->y);
    if( btn )
    {
        if(btn != av->cur_btn)
        {
            AppBtn* prev = av->cur_btn;
            av->cur_btn = btn;
            if(prev)
                gtk_widget_queue_draw_area(widget, prev->box.x, prev->box.y, prev->box.width, prev->box.height);
            gtk_widget_queue_draw_area(widget, btn->box.x, btn->box.y, btn->box.width, btn->box.height);
        }
    }
    else if(av->cur_btn)
    {
        AppBtn* prev = av->cur_btn;
        gtk_widget_queue_draw_area(widget, prev->box.x, prev->box.y, prev->box.width, prev->box.height);
        av->cur_btn = NULL;
    }
    return FALSE;
}

gboolean app_view_button_press(GtkWidget* widget, GdkEventButton* evt)
{
    AppView* av = (AppView*)widget;
    AppBtn* btn = hit_test(widget, evt->x, evt->y);
    av->mouse_down = TRUE;
    if( btn )
    {
        if( evt->button != 1 ) /* mouse buttons other than left button */
        {
            g_signal_emit(av, signals[CLICKED], 0, evt->button, btn);
        }
        else
        {
            av->cur_btn = btn;
            gtk_widget_queue_draw_area(widget, btn->box.x, btn->box.y, btn->box.width, btn->box.height);
        }
    }
    return FALSE;
}

gboolean app_view_button_release(GtkWidget* widget, GdkEventButton* evt)
{
    AppView* av = (AppView*)widget;
    AppBtn* btn = hit_test(widget, evt->x, evt->y);
    av->mouse_down = FALSE;
    if( btn &&  btn == av->cur_btn )
    {
        if( evt->button == 1 ) /* left mouse button */
            g_signal_emit(av, signals[CLICKED], 0, 1, btn);
    }
    return FALSE;
}

gboolean app_view_query_tooltip(GtkWidget *widget,gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data)
{
    AppView* av = (AppView*)widget;
    AppBtn* btn;
    if( keyboard_mode )
        return FALSE;
    btn = hit_test(widget, x, y);
    if( btn )
    {
        gtk_tooltip_set_text(tooltip, menu_cache_item_get_comment(btn->item));
        gtk_tooltip_set_tip_area(tooltip, &btn->box);
    }
    return btn ? TRUE : FALSE;
}


/* public methods */

AppBtn* app_view_add_button(AppView* av, MenuCacheItem* item)
{
    AppBtn* btn = g_slice_new(AppBtn);
//    btn->icon = NULL;
    // btn->icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), menu_cache_item_get_icon(item), av->icon_size, 0, NULL);
    btn->icon = lxlauncher_load_icon(menu_cache_item_get_icon(item), av->icon_size, TRUE);
    btn->item = menu_cache_item_ref(item);

    av->btns = g_list_append(av->btns, btn);

    return btn;
}

void app_view_remove_button(AppView* av, AppBtn* btn)
{
    app_btn_free(btn);
    av->btns = g_list_remove(av->btns, btn);
}

void app_view_remove_nth_button(AppView* av, int n)
{
    GList* l = g_list_nth(av->btns, n);
    if( G_LIKELY(l) )
    {
        app_btn_free((AppBtn*)l->data);
        av->btns = g_list_delete_link(av->btns, l);
    }

}

void app_view_remove_all(AppView* av)
{
    g_list_foreach(av->btns, (GFunc)app_btn_free, NULL);
    g_list_free(av->btns);
    av->btns = NULL;
}

GtkAdjustment* app_view_get_adjustment(AppView* av)
{
	return av->adj;
}

void app_view_set_button_size(AppView* av, int size)
{
    if( av->btn_size != size )
    {
        av->btn_size = size;
        app_view_relayout(av);
    }
}

void app_view_set_icon_size(AppView* av, int size)
{
    if( av->icon_size != size )
    {
        av->icon_size = size;
        app_view_relayout(av);
    }
}

static void app_view_update_y_pos(AppView* av)
{
    GList* l;
	int col = 0;
	int y = -av->y_off;
    for( l = av->btns; l; l = l->next )
    {
        AppBtn* btn = (AppBtn*)l->data;
		btn->box.y = y;
		++col;
		if( col >= av->n_cols )
		{
			col = 0;
			y += av->btn_size;
		}
    }
}

/* FIXME: handle RTL layout */
void app_view_relayout(AppView* av)
{
    GList* l;
    GtkWidget* w = (GtkWidget*)av;
    int x = 0, y = 0;
	guint page_size;

    av->n_cols = w->allocation.width / av->btn_size;
	av->n_rows = 1;

    for( l = av->btns; l; l = l->next )
    {
        AppBtn* btn = (AppBtn*)l->data;
        MenuCacheItem* item = btn->item;
        menu_cache_item_get_name(item);

        btn->box.width = btn->box.height = av->btn_size;

        if( G_LIKELY(x + av->btn_size <= w->allocation.width) )
            btn->box.x = x;
        else
        {
            x = btn->box.x = 0;
            y += av->btn_size;
            ++av->n_rows;
        }
        btn->box.y = y;
        x = x + av->btn_size;
    }
	page_size = w->allocation.height / av->btn_size;
	g_debug("page_size = %d, n_rows = %d", page_size, av->n_rows);
	if( page_size < av->n_rows )
		gtk_adjustment_configure(av->adj, 0, 0, av->n_rows/* - page_size*/, 1, page_size, page_size);
	else
		gtk_adjustment_configure(av->adj, 0, 0, 0, 0, 0, 0);
	on_scroll(av->adj, av);
}

void on_scroll(GtkAdjustment* adj, AppView* av)
{
	av->first_row = (guint)gtk_adjustment_get_value(adj);
	av->first_visible = g_list_nth(av->btns, av->first_row * av->n_cols);
	av->y_off = av->first_row * av->btn_size;
	app_view_update_y_pos(av);
	gtk_widget_queue_draw((GtkWidget*)av);
}

