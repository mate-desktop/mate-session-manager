/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GSM_PROPERTIES_DIALOG_H
#define __GSM_PROPERTIES_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSM_TYPE_PROPERTIES_DIALOG         (gsm_properties_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GsmPropertiesDialog, gsm_properties_dialog, GSM, PROPERTIES_DIALOG, GtkDialog)

GtkWidget            * gsm_properties_dialog_new                (void);

#define GSM_PROPERTIES_ICON_SIZE GTK_ICON_SIZE_LARGE_TOOLBAR

G_END_DECLS

#endif /* __GSM_PROPERTIES_DIALOG_H */
