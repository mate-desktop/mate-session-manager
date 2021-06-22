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

#ifndef __GSM_INHIBITOR_H__
#define __GSM_INHIBITOR_H__

#include <glib-object.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GSM_TYPE_INHIBITOR            (gsm_inhibitor_get_type ())
#define GSM_INHIBITOR_ERROR           (gsm_inhibitor_error_quark ())
#define GSM_INHIBITOR_TYPE_ERROR      (gsm_inhibitor_error_get_type ())
G_DECLARE_FINAL_TYPE (GsmInhibitor, gsm_inhibitor, GSM, INHIBITOR, GObject)

typedef enum {
        GSM_INHIBITOR_FLAG_LOGOUT      = 1 << 0,
        GSM_INHIBITOR_FLAG_SWITCH_USER = 1 << 1,
        GSM_INHIBITOR_FLAG_SUSPEND     = 1 << 2,
        GSM_INHIBITOR_FLAG_IDLE        = 1 << 3
} GsmInhibitorFlag;

typedef enum
{
        GSM_INHIBITOR_ERROR_GENERAL = 0,
        GSM_INHIBITOR_ERROR_NOT_SET,
        GSM_INHIBITOR_NUM_ERRORS
} GsmInhibitorError;

GType          gsm_inhibitor_error_get_type       (void);

GQuark         gsm_inhibitor_error_quark          (void);

GsmInhibitor * gsm_inhibitor_new                  (const char    *app_id,
                                                   guint          toplevel_xid,
                                                   guint          flags,
                                                   const char    *reason,
                                                   const char    *bus_name,
                                                   guint          cookie);
GsmInhibitor * gsm_inhibitor_new_for_client       (const char    *client_id,
                                                   const char    *app_id,
                                                   guint          flags,
                                                   const char    *reason,
                                                   const char    *bus_name,
                                                   guint          cookie);

const char *   gsm_inhibitor_peek_id              (GsmInhibitor  *inhibitor);
const char *   gsm_inhibitor_peek_app_id          (GsmInhibitor  *inhibitor);
const char *   gsm_inhibitor_peek_client_id       (GsmInhibitor  *inhibitor);
const char *   gsm_inhibitor_peek_reason          (GsmInhibitor  *inhibitor);
const char *   gsm_inhibitor_peek_bus_name        (GsmInhibitor  *inhibitor);
guint          gsm_inhibitor_peek_cookie          (GsmInhibitor  *inhibitor);
guint          gsm_inhibitor_peek_flags           (GsmInhibitor  *inhibitor);
guint          gsm_inhibitor_peek_toplevel_xid    (GsmInhibitor  *inhibitor);

/* exported to bus */
gboolean       gsm_inhibitor_get_app_id           (GsmInhibitor  *inhibitor,
                                                   char         **id,
                                                   GError       **error);
gboolean       gsm_inhibitor_get_client_id        (GsmInhibitor  *inhibitor,
                                                   char         **id,
                                                   GError       **error);
gboolean       gsm_inhibitor_get_reason           (GsmInhibitor  *inhibitor,
                                                   char         **reason,
                                                   GError       **error);
gboolean       gsm_inhibitor_get_flags            (GsmInhibitor  *inhibitor,
                                                   guint         *flags,
                                                   GError       **error);
gboolean       gsm_inhibitor_get_toplevel_xid     (GsmInhibitor  *inhibitor,
                                                   guint         *xid,
                                                   GError       **error);

G_END_DECLS

#endif /* __GSM_INHIBITOR_H__ */
