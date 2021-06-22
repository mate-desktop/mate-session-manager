/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Vincent Untz
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef __GSM_LOGOUT_DIALOG_H__
#define __GSM_LOGOUT_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSM_TYPE_LOGOUT_DIALOG         (gsm_logout_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GsmLogoutDialog, gsm_logout_dialog, GSM, LOGOUT_DIALOG, GtkMessageDialog)

enum
{
        GSM_LOGOUT_RESPONSE_LOGOUT,
        GSM_LOGOUT_RESPONSE_SWITCH_USER,
        GSM_LOGOUT_RESPONSE_SHUTDOWN,
        GSM_LOGOUT_RESPONSE_REBOOT,
        GSM_LOGOUT_RESPONSE_HIBERNATE,
        GSM_LOGOUT_RESPONSE_SLEEP
};

GtkWidget   *gsm_get_logout_dialog        (GdkScreen           *screen,
                                           guint32              activate_time);
GtkWidget   *gsm_get_shutdown_dialog      (GdkScreen           *screen,
                                           guint32              activate_time);

G_END_DECLS

#endif /* __GSM_LOGOUT_DIALOG_H__ */
