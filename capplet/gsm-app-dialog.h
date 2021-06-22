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

#ifndef __GSM_APP_DIALOG_H
#define __GSM_APP_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSM_TYPE_APP_DIALOG              (gsm_app_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GsmAppDialog, gsm_app_dialog, GSM, APP_DIALOG, GtkDialog)

GtkWidget            * gsm_app_dialog_new                (const char   *name,
                                                          const char   *command,
                                                          const char   *comment,
                                                          guint         delay);

gboolean               gsm_app_dialog_run               (GsmAppDialog  *dialog,
                                                         char         **name_p,
                                                         char         **command_p,
                                                         char         **comment_p,
                                                         guint         *delay);

const char *           gsm_app_dialog_get_name           (GsmAppDialog *dialog);
const char *           gsm_app_dialog_get_command        (GsmAppDialog *dialog);
const char *           gsm_app_dialog_get_comment        (GsmAppDialog *dialog);
guint                  gsm_app_dialog_get_delay          (GsmAppDialog *dialog);

G_END_DECLS

#endif /* __GSM_APP_DIALOG_H */
