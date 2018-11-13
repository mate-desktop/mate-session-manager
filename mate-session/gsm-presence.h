/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifndef __GSM_PRESENCE_H__
#define __GSM_PRESENCE_H__

#include <glib-object.h>
#include <sys/types.h>

G_BEGIN_DECLS

#define GSM_TYPE_PRESENCE            (gsm_presence_get_type ())
G_DECLARE_DERIVABLE_TYPE (GsmPresence, gsm_presence, GSM, PRESENCE, GObject)

struct _GsmPresenceClass
{
        GObjectClass parent_class;

        void          (* status_changed)        (GsmPresence     *presence,
                                                 guint            status);
        void          (* status_text_changed)   (GsmPresence     *presence,
                                                 const char      *status_text);

};

typedef enum {
        GSM_PRESENCE_STATUS_AVAILABLE = 0,
        GSM_PRESENCE_STATUS_INVISIBLE,
        GSM_PRESENCE_STATUS_BUSY,
        GSM_PRESENCE_STATUS_IDLE,
} GsmPresenceStatus;

typedef enum
{
        GSM_PRESENCE_ERROR_GENERAL = 0,
        GSM_PRESENCE_NUM_ERRORS
} GsmPresenceError;

#define GSM_PRESENCE_ERROR gsm_presence_error_quark ()
GType          gsm_presence_error_get_type       (void);
#define GSM_PRESENCE_TYPE_ERROR (gsm_presence_error_get_type ())

GQuark         gsm_presence_error_quark          (void);

GsmPresence *  gsm_presence_new                  (void);

void           gsm_presence_set_idle_enabled     (GsmPresence  *presence,
                                                  gboolean      enabled);
void           gsm_presence_set_idle_timeout     (GsmPresence  *presence,
                                                  guint         n_seconds);

/* exported to bus */
gboolean       gsm_presence_set_status           (GsmPresence  *presence,
                                                  guint         status,
                                                  GError      **error);
gboolean       gsm_presence_set_status_text      (GsmPresence  *presence,
                                                  const char   *status_text,
                                                  GError      **error);

G_END_DECLS

#endif /* __GSM_PRESENCE_H__ */
