/* splash-window.c
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

#ifndef __GSM_SPLASH_WINDOW_H__
#define __GSM_SPLASH_WINDOW_H__

#include <gtk/gtk.h>

#define GSM_TYPE_SPLASH_WINDOW            (gsm_splash_window_get_type ())
#define GSM_SPLASH_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSM_TYPE_SPLASH_WINDOW, GsmSplashWindow))
#define GSM_SPLASH_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSM_TYPE_SPLASH_WINDOW, GsmSplashWindowClass))
#define GSM_IS_SPLASH_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSM_TYPE_SPLASH_WINDOW))
#define GSM_IS_SPLASH_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSM_TYPE_SPLASH_WINDOW))
#define GSM_SPLASH_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSM_TYPE_SPLASH_WINDOW, GsmSplashWindowClass))

typedef struct  {
  GtkWindow       window;

  GtkIconTheme   *icon_theme;

  GdkPixbuf      *background;
  GList          *icons;
  PangoLayout    *layout;
  PangoAttribute *font_size_attr;

  /* current placement offsets */
  int             cur_x_offset;
  int             cur_y_row;

  char           *cur_text;

  /* Layout Measurements */
  int             icon_size;
  int             icon_spacing;
  int             icon_rows;
  GdkRectangle    text_box;
} GsmSplashWindow;

typedef struct {
  GtkWindowClass parent_class;

} GsmSplashWindowClass;

GType      gsm_splash_window_get_type (void);

GtkWidget *gsm_splash_window_new      (GdkPixbuf       *background);
void       gsm_splash_window_start    (GsmSplashWindow *splash,
                                       const char      *app_name,
                                       const char      *icon_name);
void       gsm_splash_window_finish   (GsmSplashWindow *splash,
                                       const char      *app_name);

#endif /* __GSM_SPLASH_WINDOW_H__ */
