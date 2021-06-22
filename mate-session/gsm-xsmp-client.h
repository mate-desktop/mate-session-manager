/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Novell, Inc.
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

#ifndef __GSM_XSMP_CLIENT_H__
#define __GSM_XSMP_CLIENT_H__

#include "gsm-client.h"

#include <X11/SM/SMlib.h>

G_BEGIN_DECLS

#define GSM_TYPE_XSMP_CLIENT              (gsm_xsmp_client_get_type ())
G_DECLARE_DERIVABLE_TYPE                  (GsmXSMPClient, gsm_xsmp_client, GSM, XSMP_CLIENT, GsmClient)

struct _GsmXSMPClientClass
{
        GsmClientClass parent_class;

        /* signals */
        gboolean (*register_request)     (GsmXSMPClient  *client,
                                          char          **client_id);
        gboolean (*logout_request)       (GsmXSMPClient  *client,
                                          gboolean        prompt);


        void     (*saved_state)          (GsmXSMPClient  *client);

        void     (*request_phase2)       (GsmXSMPClient  *client);

        void     (*request_interaction)  (GsmXSMPClient  *client);
        void     (*interaction_done)     (GsmXSMPClient  *client,
                                          gboolean        cancel_shutdown);

        void     (*save_yourself_done)   (GsmXSMPClient  *client);

};

GsmClient  *gsm_xsmp_client_new                  (IceConn         ice_conn);

void        gsm_xsmp_client_connect              (GsmXSMPClient  *client,
                                                  SmsConn         conn,
                                                  unsigned long  *mask_ret,
                                                  SmsCallbacks   *callbacks_ret);

void        gsm_xsmp_client_save_state           (GsmXSMPClient  *client);
void        gsm_xsmp_client_save_yourself        (GsmXSMPClient  *client,
                                                  gboolean        save_state);
void        gsm_xsmp_client_save_yourself_phase2 (GsmXSMPClient  *client);
void        gsm_xsmp_client_interact             (GsmXSMPClient  *client);
void        gsm_xsmp_client_shutdown_cancelled   (GsmXSMPClient  *client);

G_END_DECLS

#endif /* __GSM_XSMP_CLIENT_H__ */
