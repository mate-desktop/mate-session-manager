/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#ifndef __GSM_DBUS_CLIENT_H__
#define __GSM_DBUS_CLIENT_H__

#include "gsm-client.h"

G_BEGIN_DECLS

#define GSM_TYPE_DBUS_CLIENT            (gsm_dbus_client_get_type ())
G_DECLARE_FINAL_TYPE (GsmDBusClient, gsm_dbus_client, GSM, DBUS_CLIENT, GsmClient)

typedef enum
{
        GSM_DBUS_CLIENT_ERROR_GENERAL = 0,
        GSM_DBUS_CLIENT_ERROR_NOT_CLIENT,
        GSM_DBUS_CLIENT_NUM_ERRORS
} GsmDBusClientError;

#define GSM_DBUS_CLIENT_ERROR gsm_dbus_client_error_quark ()

GType          gsm_dbus_client_error_get_type     (void);
#define GSM_DBUS_CLIENT_TYPE_ERROR (gsm_dbus_client_error_get_type ())

GQuark         gsm_dbus_client_error_quark        (void);

GsmClient *    gsm_dbus_client_new                (const char     *startup_id,
                                                   const char     *bus_name);
const char *   gsm_dbus_client_get_bus_name       (GsmDBusClient  *client);

G_END_DECLS

#endif /* __GSM_DBUS_CLIENT_H__ */
