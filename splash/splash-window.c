/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * splash-window.c
 * Copyright (C) 1999, 2002, 2007 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include "splash-window.h"

G_DEFINE_TYPE (GsmSplashWindow, gsm_splash_window, GTK_TYPE_WINDOW)

#define TRANS_TIMEOUT        50
#define TRANS_TIMEOUT_PERIOD 30

/* width / height if we have no image */
#define SPLASH_BASE_WIDTH 480
#define SPLASH_BASE_HEIGHT 220

/* offset from bottom of label & font */
#define SPLASH_LABEL_V_OFFSET 18
#define SPLASH_LABEL_FONT_SIZE 8

/* icon border, spacing, offset from bottom and initial size */
#define SPLASH_ICON_BORDER  26
#define SPLASH_ICON_SPACING 4
#define SPLASH_ICON_V_OFFSET 28
#define SPLASH_BASE_ICON_SIZE 22
#define SPLASH_BASE_ICON_ROWS 1

static gboolean update_trans_effect (gpointer);

typedef struct {
        GdkRectangle  position;
        GdkPixbuf    *unscaled;
        GdkPixbuf    *scaled;
        GdkPixbuf    *scaled_copy;
        gint          trans_count;
} SplashIcon;

typedef struct {
        GsmSplashWindow *splash;
        SplashIcon   *si;
        int width;
        int height;
        int n_channels;
        int rowstride_trans;
        int rowstride_orig;
        guchar *pixels_trans;
        guchar *pixels_orig;
} TransParam;

static gboolean
re_scale (GsmSplashWindow *splash)
{
        int i;

        static struct {
                int icon_size;
                int icon_spacing;
                int icon_rows;
        } scales[] = {
                { SPLASH_BASE_ICON_SIZE,
                  SPLASH_ICON_SPACING,
                  SPLASH_BASE_ICON_ROWS },
                { 24, 3, 1 },
                { 24, 3, 2 },
                { 16, 2, 2 },
                { 16, 2, 3 },
                { 12, 1, 3 },
                { 8, 1, 4 },
                { 4, 1, 5 },
                { 4, 1, 4 }
        };

        for (i = 0; i < G_N_ELEMENTS (scales); i++)
                {
                        if (scales [i].icon_size < splash->icon_size ||
                            scales [i].icon_rows > splash->icon_rows) {
                                splash->icon_size    = scales [i].icon_size;
                                splash->icon_spacing = scales [i].icon_spacing;
                                splash->icon_rows    = scales [i].icon_rows;
                                break;
                        }
                }

        if (i == G_N_ELEMENTS (scales)) {
                g_warning ("Too many inits - overflowing");
                return FALSE;
        } else {
                return TRUE;
        }
}

static void
calc_text_box (GsmSplashWindow *splash)
{
        GtkAllocation *allocation = &GTK_WIDGET (splash)->allocation;
        PangoRectangle pixel_rect;

        pango_layout_get_pixel_extents (splash->layout, NULL, &pixel_rect);

        splash->text_box.x = (allocation->x + allocation->width / 2 -
                              pixel_rect.width / 2);
        splash->text_box.y = (allocation->y + allocation->height -
                              pixel_rect.height - SPLASH_LABEL_V_OFFSET);
        splash->text_box.width = pixel_rect.width + 1;
        splash->text_box.height = pixel_rect.height + 1;
}

static gboolean
splash_window_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
        GList *l;
        GdkRectangle exposed;
        GtkStyle *style = gtk_widget_get_style (widget);
        GsmSplashWindow *splash = GSM_SPLASH_WINDOW (widget);

        if (!GTK_WIDGET_DRAWABLE (widget)) {
                return FALSE;
        }

        for (l = splash->icons; l; l = l->next) {
                SplashIcon *si = l->data;

                if (gdk_rectangle_intersect (&event->area,
                                             &si->position,
                                             &exposed)) {
                        gdk_draw_pixbuf (
                                gtk_widget_get_window (widget),
                                style->black_gc,
                                si->scaled,
                                exposed.x - si->position.x,
                                exposed.y - si->position.y,
                                exposed.x, exposed.y,
                                exposed.width, exposed.height,
                                GDK_RGB_DITHER_MAX,
                                exposed.x, exposed.y);
                }
        }

        if (splash->layout) {
                calc_text_box (splash);
                if (gdk_rectangle_intersect (&event->area, &splash->text_box, &exposed)) {
                        /* drop shadow */
                        gdk_draw_layout (gtk_widget_get_window (widget),
                                         style->black_gc,
                                         splash->text_box.x + 1, splash->text_box.y + 1,
                                         splash->layout);

                        /* text */
                        gdk_draw_layout (gtk_widget_get_window (widget),
                                         style->white_gc,
                                         splash->text_box.x, splash->text_box.y,
                                         splash->layout);
                }
        }

        return FALSE;
}

static void
splash_window_realize (GtkWidget *widget)
{
        GsmSplashWindow *splash = (GsmSplashWindow *) widget;

        GTK_WIDGET_CLASS (gsm_splash_window_parent_class)->realize (widget);

        if (splash->background && gtk_widget_get_window (widget)) {
                GdkPixmap   *pixmap = NULL;
                GdkBitmap   *mask = NULL;
                GdkColormap *colormap;

                colormap = gtk_widget_get_colormap (widget);
                pixmap = gdk_pixmap_new (gtk_widget_get_window (widget),
                                         gdk_pixbuf_get_width (splash->background),
                                         gdk_pixbuf_get_height (splash->background),
                                         -1);

                if (pixmap) {
                        GtkStyle *style;

                        /* we want dither_max for 16-bits people */
                        gdk_draw_pixbuf (pixmap, NULL, splash->background,
                                         0, 0, 0, 0, -1, -1,
                                         GDK_RGB_DITHER_MAX, 0, 0);

                        style = gtk_widget_get_style (widget);
                        style->bg_pixmap[GTK_STATE_NORMAL] = pixmap;

                        gtk_widget_set_style (widget, style);
                        g_object_unref (style);

                        gdk_pixbuf_render_pixmap_and_mask_for_colormap (splash->background,
                                                                        colormap,
                                                                        NULL,
                                                                        &mask,
                                                                        125);

                        if (mask) {
                                gdk_window_shape_combine_mask (gtk_widget_get_window (widget),
                                                               mask, 0, 0);
                                g_object_unref (mask);
                        }

                        gtk_style_set_background (gtk_widget_get_style (widget),
                                                  gtk_widget_get_window (widget),
                                                  GTK_STATE_NORMAL);
                }
        }
}

static void
splash_icon_destroy (SplashIcon *si)
{
        g_object_unref (si->unscaled);
        if (si->scaled) {
                g_object_unref (si->scaled);
        }
        if (si->scaled_copy) {
                g_object_unref (si->scaled_copy);
        }

        g_free (si);
}

static void
splash_window_finalize (GObject *object)
{
        GsmSplashWindow *splash = (GsmSplashWindow *) object;

        /* do not unref it: we don't own it */
        splash->icon_theme = NULL;

        g_list_foreach (splash->icons, (GFunc) splash_icon_destroy, NULL);
        g_list_free (splash->icons);

        if (splash->background != NULL) {
                g_object_unref (splash->background);
        }
        splash->background = NULL;

        g_object_unref (splash->layout);
        splash->layout = NULL;

        G_OBJECT_CLASS (gsm_splash_window_parent_class)->finalize (object);
}

static void
gsm_splash_window_class_init (GsmSplashWindowClass *klass)
{
        GObjectClass   *gobject_class = (GObjectClass *) klass;
        GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

        gobject_class->finalize = splash_window_finalize;

        widget_class->realize = splash_window_realize;
        widget_class->expose_event = splash_window_expose_event;
}

static void
gsm_splash_window_init (GsmSplashWindow *splash)
{
        GtkWindow     *window;
        PangoAttrList *attrs;

        window = &splash->window;

        /* window->type clobbered by default properties on GtkWindow */
        g_object_set (window,
                      "type_hint", GDK_WINDOW_TYPE_HINT_SPLASHSCREEN,
                      "decorated", FALSE,
                      "window_position", GTK_WIN_POS_CENTER,
                      "allow_shrink", FALSE,
                      "allow_grow", FALSE,
                      NULL);

        splash->icon_size = SPLASH_BASE_ICON_SIZE;
        splash->icon_spacing = SPLASH_ICON_SPACING;
        splash->cur_y_row = SPLASH_BASE_ICON_ROWS;

        gtk_widget_add_events (GTK_WIDGET (window),
                               GDK_BUTTON_RELEASE_MASK);

        splash->layout = gtk_widget_create_pango_layout (GTK_WIDGET (splash), "");
        pango_layout_set_alignment (splash->layout, PANGO_ALIGN_CENTER);

        attrs = pango_attr_list_new ();
        splash->font_size_attr = pango_attr_size_new (PANGO_SCALE * SPLASH_LABEL_FONT_SIZE);
        splash->font_size_attr->start_index = 0;
        splash->font_size_attr->end_index = 0;
        pango_attr_list_insert (attrs, splash->font_size_attr);
        pango_layout_set_attributes (splash->layout, attrs);
        pango_attr_list_unref (attrs);

        splash->icon_theme = gtk_icon_theme_get_default ();
}

static void re_laydown (GsmSplashWindow *splash);

static void
layout_icon (GsmSplashWindow *splash,
             SplashIcon      *si,
             GdkRectangle    *area)
{
        GtkWidget     *widget = GTK_WIDGET (splash);
        GtkAllocation *allocation = &widget->allocation;

        g_return_if_fail (si != NULL);

        si->position.x = allocation->x + splash->cur_x_offset + SPLASH_ICON_BORDER;
        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
                si->position.x = allocation->width - si->position.x - splash->icon_size;
        }

        si->position.y = (allocation->y + allocation->height -
                          SPLASH_ICON_V_OFFSET -
                          (splash->icon_size + splash->icon_spacing) * splash->cur_y_row);

        si->position.width = si->position.height = splash->icon_size;

        splash->cur_x_offset += splash->icon_size + splash->icon_spacing;

        if (area) {
                *area = si->position;
        }

        if (!si->scaled) {
                if (gdk_pixbuf_get_width (si->unscaled) == splash->icon_size &&
                    gdk_pixbuf_get_height (si->unscaled) == splash->icon_size) {
                        si->scaled = gdk_pixbuf_copy (si->unscaled);
                } else {
                        si->scaled = gdk_pixbuf_scale_simple (si->unscaled, splash->icon_size,
                                                              splash->icon_size,
                                                              GDK_INTERP_BILINEAR);
                }
        }

        if (splash->cur_x_offset >= (allocation->width - SPLASH_ICON_BORDER * 2 -
                                     splash->icon_size)) {
                if (--splash->cur_y_row > 0) {
                        splash->cur_x_offset = 0;
                } else {
                        if (re_scale (splash)) {
                                re_laydown (splash);
                                gtk_widget_queue_draw (GTK_WIDGET (splash));
                        }
                }
        }
}

static void
re_laydown (GsmSplashWindow *splash)
{
        GList *l;

        splash->cur_x_offset = 0;
        splash->cur_y_row = splash->icon_rows;

        for (l = splash->icons; l; l = l->next) {
                SplashIcon *si = l->data;

                if (si->scaled) {
                        g_object_unref (si->scaled);
                        si->scaled = NULL;
                }
                layout_icon (splash, l->data, NULL);

                /* If si has a TransParam, it's now messed up because it's still
                 * referring to the old icon. We deal with this by just
                 * immediately finishing the effect now.
                 */
                si->trans_count = TRANS_TIMEOUT_PERIOD;
        }
}

static gboolean
update_trans_effect (gpointer trans_param)
{
        guchar     *p_trans;
        guchar     *p_orig;
        gdouble    r_mul, g_mul, b_mul, a_mul;
        gint       x = 0;
        gint       y = 0;

        TransParam *tp = (TransParam *) trans_param;

        if (tp->si->trans_count++ == TRANS_TIMEOUT_PERIOD) {
                gtk_widget_queue_draw_area (GTK_WIDGET (tp->splash),
                                            tp->si->position.x, tp->si->position.y,
                                            tp->si->position.width, tp->si->position.height);
                g_free (tp);
                return FALSE;
        }

        a_mul = (gdouble) tp->si->trans_count / TRANS_TIMEOUT_PERIOD;
        r_mul = 1;
        g_mul = 1;
        b_mul = 1;

        for (y = 0; y < tp->height; y++) {
                for (x = 0; x < tp->width; x++) {
                        p_trans = tp->pixels_trans + y * tp->rowstride_trans + x * tp->n_channels;
                        p_orig  = tp->pixels_orig  + y * tp->rowstride_orig  + x * tp->n_channels;

                        /* we can add more effects here apart from alpha fading */
                        p_trans[0] = r_mul * p_orig[0];
                        p_trans[1] = g_mul * p_orig[1];
                        p_trans[2] = b_mul * p_orig[2];
                        p_trans[3] = a_mul * p_orig[3];
                }
        }

        gtk_widget_queue_draw_area (GTK_WIDGET (tp->splash),
                                    tp->si->position.x, tp->si->position.y,
                                    tp->si->position.width, tp->si->position.height);
        return TRUE;
}


GtkWidget *
gsm_splash_window_new (GdkPixbuf *background)
{
        GsmSplashWindow *splash = g_object_new (GSM_TYPE_SPLASH_WINDOW, NULL);

        /* FIXME: make this a property */
        if (background) {
                splash->background = g_object_ref (background);
                gtk_widget_set_size_request ((GtkWidget *)splash,
                                             gdk_pixbuf_get_width  (background),
                                             gdk_pixbuf_get_height (background));
        } else {
                gtk_widget_set_size_request ((GtkWidget *)splash,
                                             SPLASH_BASE_WIDTH,
                                             SPLASH_BASE_HEIGHT);
        }

        return (GtkWidget *)splash;
}

void
gsm_splash_window_start (GsmSplashWindow *splash,
                         const char      *app_name,
                         const char      *icon_name)
{
        GdkPixbuf    *pb;
        SplashIcon   *si;
        GdkRectangle  area;
        int           length;
        TransParam   *tp;

        g_return_if_fail (GSM_IS_SPLASH_WINDOW (splash));

        if (!icon_name) {
                return;
        }

        pb = gtk_icon_theme_load_icon (splash->icon_theme,
                                       icon_name,
                                       22, /* icon size */
                                       0 /* flags */,
                                       NULL);
        if (!pb) {
                return;
        }

        /* re-draw the old text extents */
        gtk_widget_queue_draw_area (GTK_WIDGET (splash),
                                    splash->text_box.x, splash->text_box.y,
                                    splash->text_box.width, splash->text_box.height);

        g_free (splash->cur_text);
        splash->cur_text = g_strdup (app_name);

        length = strlen (splash->cur_text);
        splash->font_size_attr->end_index = length;
        pango_layout_set_text (splash->layout, splash->cur_text, length);
        calc_text_box (splash);

        /* re-draw the new text extents */
        gtk_widget_queue_draw_area (GTK_WIDGET (splash),
                                    splash->text_box.x, splash->text_box.y,
                                    splash->text_box.width, splash->text_box.height);

        si = g_new0 (SplashIcon, 1);
        si->unscaled = pb;

        splash->icons = g_list_append (splash->icons, si);

        layout_icon (splash, si, &area);

        /* prepare transparency effect */
        if ((gdk_pixbuf_get_colorspace (si->scaled) == GDK_COLORSPACE_RGB) &&
            (gdk_pixbuf_get_bits_per_sample (si->scaled) == 8) &&
            (gdk_pixbuf_get_n_channels (si->scaled))) {
                si->trans_count = 0;
                if (!gdk_pixbuf_get_has_alpha (si->scaled)) {
                        si->scaled_copy = gdk_pixbuf_add_alpha (si->scaled, FALSE, 0, 0, 0);
                        g_object_unref (si->scaled);
                        si->scaled = gdk_pixbuf_copy (si->scaled_copy);
                } else {
                        si->scaled_copy = gdk_pixbuf_copy (si->scaled);
                }

                tp = g_new0(TransParam, 1);
                tp->si = si;
                tp->splash = splash;
                tp->width  = gdk_pixbuf_get_width  (tp->si->scaled);
                tp->height = gdk_pixbuf_get_height (tp->si->scaled);
                tp->rowstride_trans = gdk_pixbuf_get_rowstride (tp->si->scaled);
                tp->rowstride_orig  = gdk_pixbuf_get_rowstride (tp->si->scaled_copy);
                tp->pixels_trans = gdk_pixbuf_get_pixels (tp->si->scaled);
                tp->pixels_orig  = gdk_pixbuf_get_pixels (tp->si->scaled_copy);
                tp->n_channels = gdk_pixbuf_get_n_channels (tp->si->scaled);

                gdk_pixbuf_fill (si->scaled, 0x00000000);
                g_timeout_add (TRANS_TIMEOUT, update_trans_effect, tp);
        } else {
                gtk_widget_queue_draw_area (GTK_WIDGET (splash), area.x, area.y,
                                            area.width, area.height);
        }
}

void
gsm_splash_window_finish (GsmSplashWindow *splash,
                          const char      *app_name)
{
        if (splash->cur_text && !strcmp (splash->cur_text, app_name)) {
                g_free (splash->cur_text);
                splash->cur_text = NULL;

                splash->font_size_attr->end_index = 0;
                pango_layout_set_text (splash->layout, "", 0);

                gtk_widget_queue_draw_area (GTK_WIDGET (splash),
                                            splash->text_box.x, splash->text_box.y,
                                            splash->text_box.width, splash->text_box.height);

                calc_text_box (splash);
        }
}
