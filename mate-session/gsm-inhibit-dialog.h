/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <mccann@jhu.edu>
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

#ifndef __GSM_INHIBIT_DIALOG_H
#define __GSM_INHIBIT_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gsm-store.h"

G_BEGIN_DECLS

#define GSM_TYPE_INHIBIT_DIALOG         (gsm_inhibit_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GsmInhibitDialog, gsm_inhibit_dialog, GSM, INHIBIT_DIALOG, GtkDialog)

typedef enum
{
        GSM_LOGOUT_ACTION_LOGOUT,
        GSM_LOGOUT_ACTION_SWITCH_USER,
        GSM_LOGOUT_ACTION_SHUTDOWN,
        GSM_LOGOUT_ACTION_REBOOT,
        GSM_LOGOUT_ACTION_HIBERNATE,
        GSM_LOGOUT_ACTION_SLEEP
} GsmLogoutAction;

GtkWidget            * gsm_inhibit_dialog_new                (GsmStore         *inhibitors,
                                                              GsmStore         *clients,
                                                              int               action);
GtkTreeModel         * gsm_inhibit_dialog_get_model          (GsmInhibitDialog *dialog);

G_END_DECLS

#endif /* __GSM_INHIBIT_DIALOG_H */
