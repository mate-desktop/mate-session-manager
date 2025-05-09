/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#include "config.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <dbus/dbus-glib.h>

#include "gs-idle-monitor.h"

#include "gsm-presence.h"
#include "gsm-presence-glue.h"

#define GSM_PRESENCE_DBUS_PATH "/org/gnome/SessionManager/Presence"

#define GS_NAME      "org.mate.ScreenSaver"
#define GS_PATH      "/org/mate/ScreenSaver"
#define GS_INTERFACE "org.mate.ScreenSaver"

#define MAX_STATUS_TEXT 140

typedef struct {
        guint            status;
        guint            saved_status;
        char            *status_text;
        gboolean         idle_enabled;
        GSIdleMonitor   *idle_monitor;
        guint            idle_watch_id;
        guint            idle_timeout;
        gboolean         screensaver_active;
        DBusGConnection *bus_connection;
        DBusGProxy      *bus_proxy;
        DBusGProxy      *screensaver_proxy;
} GsmPresencePrivate;

enum {
        PROP_0,
        PROP_STATUS,
        PROP_STATUS_TEXT,
        PROP_IDLE_ENABLED,
        PROP_IDLE_TIMEOUT,
};

enum {
        STATUS_CHANGED,
        STATUS_TEXT_CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GsmPresence, gsm_presence, G_TYPE_OBJECT);

GQuark
gsm_presence_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("gsm_presence_error");
        }

        return ret;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
gsm_presence_error_get_type (void)
{
        static GType etype = 0;

        if (etype == 0) {
                static const GEnumValue values[] = {
                        ENUM_ENTRY (GSM_PRESENCE_ERROR_GENERAL, "GeneralError"),
                        { 0, 0, 0 }
                };

                g_assert (GSM_PRESENCE_NUM_ERRORS == G_N_ELEMENTS (values) - 1);

                etype = g_enum_register_static ("GsmPresenceError", values);
        }

        return etype;
}

static void
set_session_idle (GsmPresence   *presence,
                  gboolean       is_idle)
{
        GsmPresencePrivate *priv;

        g_debug ("GsmPresence: setting idle: %d", is_idle);
        priv = gsm_presence_get_instance_private (presence);

        if (is_idle) {
                if (priv->status == GSM_PRESENCE_STATUS_IDLE) {
                        g_debug ("GsmPresence: already idle, ignoring");
                        return;
                }

                /* save current status */
                priv->saved_status = priv->status;
                gsm_presence_set_status (presence, GSM_PRESENCE_STATUS_IDLE, NULL);
        } else {
                if (priv->status != GSM_PRESENCE_STATUS_IDLE) {
                        g_debug ("GsmPresence: already not idle, ignoring");
                        return;
                }

                /* restore saved status */
                gsm_presence_set_status (presence, priv->saved_status, NULL);
                priv->saved_status = GSM_PRESENCE_STATUS_AVAILABLE;
        }
}

static gboolean
on_idle_timeout (GSIdleMonitor *monitor,
                 guint          id,
                 gboolean       condition,
                 GsmPresence   *presence)
{
        gboolean handled;

        handled = TRUE;
        set_session_idle (presence, condition);

        return handled;
}

static void
reset_idle_watch (GsmPresence  *presence)
{
        GsmPresencePrivate *priv;

        priv = gsm_presence_get_instance_private (presence);
        if (priv->idle_monitor == NULL) {
                return;
        }

        if (priv->idle_watch_id > 0) {
                g_debug ("GsmPresence: removing idle watch");
                gs_idle_monitor_remove_watch (priv->idle_monitor,
                                              priv->idle_watch_id);
                priv->idle_watch_id = 0;
        }

        if (! priv->screensaver_active
            && priv->idle_enabled
            && priv->idle_timeout > 0) {
                g_debug ("GsmPresence: adding idle watch");

                priv->idle_watch_id = gs_idle_monitor_add_watch (priv->idle_monitor,
                                                                 priv->idle_timeout,
                                                                 (GSIdleMonitorWatchFunc)on_idle_timeout,
                                                                 presence);
        }
}

static void
on_screensaver_active_changed (DBusGProxy  *proxy,
                               gboolean     is_active,
                               GsmPresence *presence)
{
        GsmPresencePrivate *priv;

        g_debug ("screensaver status changed: %d", is_active);
        priv = gsm_presence_get_instance_private (presence);
        if (priv->screensaver_active != is_active) {
                priv->screensaver_active = is_active;
                reset_idle_watch (presence);
                set_session_idle (presence, is_active);
        }
}

static void
on_screensaver_proxy_destroy (GObject     *proxy,
                              GsmPresence *presence)
{
        GsmPresencePrivate *priv;

        g_info ("Detected that screensaver has left the bus");
        priv = gsm_presence_get_instance_private (presence);

        priv->screensaver_proxy = NULL;
        priv->screensaver_active = FALSE;
        set_session_idle (presence, FALSE);
        reset_idle_watch (presence);
}

static void
on_bus_name_owner_changed (DBusGProxy  *bus_proxy,
                           const char  *service_name,
                           const char  *old_service_name,
                           const char  *new_service_name,
                           GsmPresence *presence)
{
        GError *error;
        GsmPresencePrivate *priv;

        priv = gsm_presence_get_instance_private (presence);

        if (service_name == NULL
            || strcmp (service_name, GS_NAME) != 0) {
                /* ignore */
                return;
        }

        if (strlen (new_service_name) == 0
            && strlen (old_service_name) > 0) {
                /* service removed */
                /* let destroy signal handle this? */
        } else if (strlen (old_service_name) == 0
                   && strlen (new_service_name) > 0) {
                /* service added */
                error = NULL;
                priv->screensaver_proxy = dbus_g_proxy_new_for_name_owner (priv->bus_connection,
                                                                           GS_NAME,
                                                                           GS_PATH,
                                                                           GS_INTERFACE,
                                                                           &error);
                if (priv->screensaver_proxy != NULL) {
                        g_signal_connect (priv->screensaver_proxy,
                                          "destroy",
                                          G_CALLBACK (on_screensaver_proxy_destroy),
                                          presence);
                        dbus_g_proxy_add_signal (priv->screensaver_proxy,
                                                 "ActiveChanged",
                                                 G_TYPE_BOOLEAN,
                                                 G_TYPE_INVALID);
                        dbus_g_proxy_connect_signal (priv->screensaver_proxy,
                                                     "ActiveChanged",
                                                     G_CALLBACK (on_screensaver_active_changed),
                                                     presence,
                                                     NULL);
                } else {
                        g_warning ("Unable to get screensaver proxy: %s", error->message);
                        g_error_free (error);
                }
        }
}

static gboolean
register_presence (GsmPresence *presence)
{
        GError *error;
        GsmPresencePrivate *priv;

        error = NULL;

        priv = gsm_presence_get_instance_private (presence);
        priv->bus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (priv->bus_connection == NULL) {
                if (error != NULL) {
                        g_critical ("error getting session bus: %s", error->message);
                        g_error_free (error);
                }
                return FALSE;
        }

        dbus_g_connection_register_g_object (priv->bus_connection, GSM_PRESENCE_DBUS_PATH, G_OBJECT (presence));

        return TRUE;
}

static GObject *
gsm_presence_constructor (GType                  type,
                          guint                  n_construct_properties,
                          GObjectConstructParam *construct_properties)
{
        GsmPresence *presence;
        gboolean     res;
        GsmPresencePrivate *priv;

        presence = GSM_PRESENCE (G_OBJECT_CLASS (gsm_presence_parent_class)->constructor (type,
                                                                                          n_construct_properties,
                                                                                          construct_properties));
        priv = gsm_presence_get_instance_private (presence);

        res = register_presence (presence);
        if (! res) {
                g_warning ("Unable to register presence with session bus");
        }

        priv->bus_proxy = dbus_g_proxy_new_for_name (priv->bus_connection,
                                                     DBUS_SERVICE_DBUS,
                                                     DBUS_PATH_DBUS,
                                                     DBUS_INTERFACE_DBUS);
        if (priv->bus_proxy != NULL) {
                dbus_g_proxy_add_signal (priv->bus_proxy,
                                         "NameOwnerChanged",
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_INVALID);
                dbus_g_proxy_connect_signal (priv->bus_proxy,
                                             "NameOwnerChanged",
                                             G_CALLBACK (on_bus_name_owner_changed),
                                             presence,
                                             NULL);
        }

        return G_OBJECT (presence);
}

static void
gsm_presence_init (GsmPresence *presence)
{
        GsmPresencePrivate *priv;

        priv = gsm_presence_get_instance_private (presence);

        priv->idle_monitor = gs_idle_monitor_new ();
}

void
gsm_presence_set_idle_enabled (GsmPresence  *presence,
                               gboolean      enabled)
{
        g_return_if_fail (GSM_IS_PRESENCE (presence));
        GsmPresencePrivate *priv;

        priv = gsm_presence_get_instance_private (presence);

        if (priv->idle_enabled != enabled) {
                priv->idle_enabled = enabled;
                reset_idle_watch (presence);
                g_object_notify (G_OBJECT (presence), "idle-enabled");

        }
}

gboolean
gsm_presence_set_status_text (GsmPresence  *presence,
                              const char   *status_text,
                              GError      **error)
{
        GsmPresencePrivate *priv;
        g_return_val_if_fail (GSM_IS_PRESENCE (presence), FALSE);

        priv = gsm_presence_get_instance_private (presence);

        g_free (priv->status_text);

        /* check length */
        if (status_text != NULL && strlen (status_text) > MAX_STATUS_TEXT) {
                g_set_error (error,
                             GSM_PRESENCE_ERROR,
                             GSM_PRESENCE_ERROR_GENERAL,
                             "Status text too long");
                return FALSE;
        }

        if (status_text != NULL) {
                priv->status_text = g_strdup (status_text);
        } else {
                priv->status_text = g_strdup ("");
        }
        g_object_notify (G_OBJECT (presence), "status-text");
        g_signal_emit (presence, signals[STATUS_TEXT_CHANGED], 0, priv->status_text);
        return TRUE;
}

gboolean
gsm_presence_set_status (GsmPresence  *presence,
                         guint         status,
                         GError      **error)
{
        GsmPresencePrivate *priv;

        g_return_val_if_fail (GSM_IS_PRESENCE (presence), FALSE);
        priv = gsm_presence_get_instance_private (presence);

        if (status != priv->status) {
                priv->status = status;
                g_object_notify (G_OBJECT (presence), "status");
                g_signal_emit (presence, signals[STATUS_CHANGED], 0, priv->status);
        }
        return TRUE;
}

void
gsm_presence_set_idle_timeout (GsmPresence  *presence,
                               guint         timeout)
{
        GsmPresencePrivate *priv;

        g_return_if_fail (GSM_IS_PRESENCE (presence));
        priv = gsm_presence_get_instance_private (presence);

        if (timeout != priv->idle_timeout) {
                priv->idle_timeout = timeout;
                reset_idle_watch (presence);
                g_object_notify (G_OBJECT (presence), "idle-timeout");
        }
}

static void
gsm_presence_set_property (GObject       *object,
                           guint          prop_id,
                           const GValue  *value,
                           GParamSpec    *pspec)
{
        GsmPresence *self;

        self = GSM_PRESENCE (object);

        switch (prop_id) {
        case PROP_STATUS:
                gsm_presence_set_status (self, g_value_get_uint (value), NULL);
                break;
        case PROP_STATUS_TEXT:
                gsm_presence_set_status_text (self, g_value_get_string (value), NULL);
                break;
        case PROP_IDLE_ENABLED:
                gsm_presence_set_idle_enabled (self, g_value_get_boolean (value));
                break;
        case PROP_IDLE_TIMEOUT:
                gsm_presence_set_idle_timeout (self, g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_presence_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
        GsmPresence *self;
        GsmPresencePrivate *priv;

        self = GSM_PRESENCE (object);

        priv = gsm_presence_get_instance_private (self);

        switch (prop_id) {
        case PROP_STATUS:
                g_value_set_uint (value, priv->status);
                break;
        case PROP_STATUS_TEXT:
                g_value_set_string (value, priv->status_text);
                break;
        case PROP_IDLE_ENABLED:
                g_value_set_boolean (value, priv->idle_enabled);
                break;
        case PROP_IDLE_TIMEOUT:
                g_value_set_uint (value, priv->idle_timeout);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_presence_finalize (GObject *object)
{
        GsmPresence *presence = (GsmPresence *) object;
        GsmPresencePrivate *priv;

        priv = gsm_presence_get_instance_private (presence);

        if (priv->idle_watch_id > 0) {
                gs_idle_monitor_remove_watch (priv->idle_monitor,
                                              priv->idle_watch_id);
                priv->idle_watch_id = 0;
        }

        if (priv->status_text != NULL) {
                g_free (priv->status_text);
                priv->status_text = NULL;
        }

        if (priv->idle_monitor != NULL) {
                g_object_unref (priv->idle_monitor);
                priv->idle_monitor = NULL;
        }

        G_OBJECT_CLASS (gsm_presence_parent_class)->finalize (object);
}

static void
gsm_presence_class_init (GsmPresenceClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize             = gsm_presence_finalize;
        object_class->constructor          = gsm_presence_constructor;
        object_class->get_property         = gsm_presence_get_property;
        object_class->set_property         = gsm_presence_set_property;

        signals [STATUS_CHANGED] =
                g_signal_new ("status-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsmPresenceClass, status_changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__UINT,
                              G_TYPE_NONE,
                              1, G_TYPE_UINT);
        signals [STATUS_TEXT_CHANGED] =
                g_signal_new ("status-text-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GsmPresenceClass, status_text_changed),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1, G_TYPE_STRING);

        g_object_class_install_property (object_class,
                                         PROP_STATUS,
                                         g_param_spec_uint ("status",
                                                            "status",
                                                            "status",
                                                            0,
                                                            G_MAXINT,
                                                            0,
                                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_STATUS_TEXT,
                                         g_param_spec_string ("status-text",
                                                              "status text",
                                                              "status text",
                                                              "",
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_IDLE_ENABLED,
                                         g_param_spec_boolean ("idle-enabled",
                                                               NULL,
                                                               NULL,
                                                               FALSE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_IDLE_TIMEOUT,
                                         g_param_spec_uint ("idle-timeout",
                                                            "idle timeout",
                                                            "idle timeout",
                                                            0,
                                                            G_MAXINT,
                                                            300000,
                                                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        dbus_g_object_type_install_info (GSM_TYPE_PRESENCE, &dbus_glib_gsm_presence_object_info);
        dbus_g_error_domain_register (GSM_PRESENCE_ERROR, NULL, GSM_PRESENCE_TYPE_ERROR);
}

GsmPresence *
gsm_presence_new (void)
{
        GsmPresence *presence;

        presence = g_object_new (GSM_TYPE_PRESENCE,
                                 NULL);

        return presence;
}
