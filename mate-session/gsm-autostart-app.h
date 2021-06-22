/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Novell, Inc.
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2012-2021 MATE Developers
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __GSM_AUTOSTART_APP_H__
#define __GSM_AUTOSTART_APP_H__

#include "gsm-app.h"

#include <X11/SM/SMlib.h>

G_BEGIN_DECLS

#define GSM_TYPE_AUTOSTART_APP            (gsm_autostart_app_get_type ())
G_DECLARE_DERIVABLE_TYPE (GsmAutostartApp, gsm_autostart_app, GSM, AUTOSTART_APP, GsmApp)

struct _GsmAutostartAppClass
{
        GsmAppClass parent_class;

        /* signals */
        void     (*condition_changed)  (GsmApp  *app,
                                        gboolean condition);
};

GsmApp *gsm_autostart_app_new                (const char *desktop_file);

#define GSM_AUTOSTART_APP_PHASE_KEY       "X-MATE-Autostart-Phase"
#define GSM_AUTOSTART_APP_PROVIDES_KEY    "X-MATE-Provides"
#define GSM_AUTOSTART_APP_STARTUP_ID_KEY  "X-MATE-Autostart-startup-id"
#define GSM_AUTOSTART_APP_AUTORESTART_KEY "X-MATE-AutoRestart"
#define GSM_AUTOSTART_APP_DBUS_NAME_KEY   "X-MATE-DBus-Name"
#define GSM_AUTOSTART_APP_DBUS_PATH_KEY   "X-MATE-DBus-Path"
#define GSM_AUTOSTART_APP_DBUS_ARGS_KEY   "X-MATE-DBus-Start-Arguments"
#define GSM_AUTOSTART_APP_DISCARD_KEY     "X-MATE-Autostart-discard-exec"
#define GSM_AUTOSTART_APP_DELAY_KEY       "X-MATE-Autostart-Delay"

G_END_DECLS

#endif /* __GSM_AUTOSTART_APP_H__ */
