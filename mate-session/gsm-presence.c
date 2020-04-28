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

#include "gs-idle-monitor.h"

#include "gsm-presence.h"
#include "org.gnome.SessionManager.Presence.h"

#define GSM_PRESENCE_DBUS_IFACE "org.gnome.SessionManager.Presence"
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
        GDBusConnection  *connection;
        GDBusProxy       *screensaver_proxy;

        GsmExportedPresence *skeleton;
} GsmPresencePrivate;

enum {
        PROP_0,
        PROP_IDLE_ENABLED,
        PROP_IDLE_TIMEOUT,
};

enum {
        STATUS_CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GsmPresence, gsm_presence, G_TYPE_OBJECT);

static const GDBusErrorEntry gsm_presence_error_entries[] = {
        { GSM_PRESENCE_ERROR_GENERAL, GSM_PRESENCE_DBUS_IFACE ".GeneralError" }
};

GQuark
gsm_presence_error_quark (void)
{
    static volatile gsize quark_volatile = 0;

    g_dbus_error_register_error_domain ("gsm_presence_error",
                                        &quark_volatile,
                                        gsm_presence_error_entries,
                                        G_N_ELEMENTS (gsm_presence_error_entries));
    return quark_volatile;
}

static void idle_became_active_cb (GSIdleMonitor *idle_monitor,
                                   guint             id,
                                   gpointer          user_data);
static void
gsm_presence_set_status (GsmPresence  *presence,
                         guint         status)
{
    GsmPresencePrivate *priv;

    priv = gsm_presence_get_instance_private (presence);
    if (status != priv->status) {
            priv->status = status;
            gsm_exported_presence_set_status (priv->skeleton, status);
            gsm_exported_presence_emit_status_changed (priv->skeleton, priv->status);
            g_signal_emit (presence, signals[STATUS_CHANGED], 0, priv->status);
    }
}

static gboolean
gsm_presence_set_status_text (GsmPresence  *presence,
                              const char   *status_text,
                              GError      **error)
{
    g_return_val_if_fail (GSM_IS_PRESENCE (presence), FALSE);
    GsmPresencePrivate *priv;

    priv = gsm_presence_get_instance_private (presence);

    g_free (priv->status_text);
	priv->status_text = NULL;

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

    gsm_exported_presence_set_status_text (priv->skeleton, priv->status_text);
    gsm_exported_presence_emit_status_text_changed (priv->skeleton, priv->status_text);
    return TRUE;
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
                gsm_presence_set_status (presence, GSM_PRESENCE_STATUS_IDLE);
        } else {
                if (priv->status != GSM_PRESENCE_STATUS_IDLE) {
                        g_debug ("GsmPresence: already not idle, ignoring");
                        return;
                }

                /* restore saved status */
                gsm_presence_set_status (presence, priv->saved_status);
                priv->saved_status = GSM_PRESENCE_STATUS_AVAILABLE;
        }
}

static gboolean
on_idle_timeout (GSIdleMonitor *monitor G_GNUC_UNUSED,
                 guint          id G_GNUC_UNUSED,
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
on_screensaver_dbus_signal (GDBusProxy  *proxy G_GNUC_UNUSED,
                            gchar       *sender_name G_GNUC_UNUSED,
                            gchar       *signal_name G_GNUC_UNUSED,
                            GVariant    *parameters,
                            GsmPresence *presence)
{
        GsmPresencePrivate *priv;
        priv = gsm_presence_get_instance_private (presence);
        gboolean is_active;

        if (g_strcmp0 (signal_name, "ActiveChanged") != 0) {
                return;
        }

        g_variant_get (parameters, "(b)", &is_active);

        if (priv->screensaver_active != is_active) {
                priv->screensaver_active = is_active;
                reset_idle_watch (presence);
                set_session_idle (presence, is_active);
        }
}

static void
on_screensaver_name_owner_changed (GDBusProxy  *screensaver_proxy,
                                   GParamSpec  *pspec G_GNUC_UNUSED,
                                   GsmPresence *presence)
{
        GsmPresencePrivate *priv;
        gchar *name_owner;

        g_warning ("Detected that screensaver has left the bus");
        priv = gsm_presence_get_instance_private (presence);

        name_owner = g_dbus_proxy_get_name_owner (screensaver_proxy);
        if (name_owner == NULL) {
                g_debug ("Detected that screensaver has left the bus");

                priv->screensaver_proxy = NULL;
                priv->screensaver_active = FALSE;
                set_session_idle (presence, FALSE);
        }

        g_free (name_owner);
        reset_idle_watch (presence);
}

static gboolean
gsm_presence_set_status_text_dbus (GsmExportedPresence   *skeleton,
                                   GDBusMethodInvocation *invocation,
                                   gchar                 *status_text,
                                   GsmPresence           *presence)
{
        GError *error = NULL;

        if (gsm_presence_set_status_text (presence, status_text, &error)) {
                gsm_exported_presence_complete_set_status_text (skeleton, invocation);
        } else {
                g_dbus_method_invocation_take_error (invocation, error);
        }
        return TRUE;
}

static gboolean
gsm_presence_set_status_dbus (GsmExportedPresence   *skeleton,
                              GDBusMethodInvocation *invocation,
                              guint                  status,
                              GsmPresence           *presence)
{
        gsm_presence_set_status (presence, status);
        gsm_exported_presence_complete_set_status (skeleton, invocation);
        return TRUE;
}

static gboolean
register_presence (GsmPresence *presence)
{
        GError *error;
        GsmPresencePrivate *priv;
        GsmExportedPresence *skeleton;

        error = NULL;

        priv = gsm_presence_get_instance_private (presence);
        priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
        if (error != NULL) {
                g_critical ("error getting session bus: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        skeleton = gsm_exported_presence_skeleton_new ();
        priv->skeleton = skeleton;
        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                          priv->connection,
                                          GSM_PRESENCE_DBUS_PATH, &error);
        if (error != NULL) {
                g_critical ("error registering presence object on session bus: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        g_signal_connect (skeleton, "handle-set-status",
                          G_CALLBACK (gsm_presence_set_status_dbus), presence);
        g_signal_connect (skeleton, "handle-set-status-text",
                          G_CALLBACK (gsm_presence_set_status_text_dbus), presence);
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
        GError      *error = NULL;

        presence = GSM_PRESENCE (G_OBJECT_CLASS (gsm_presence_parent_class)->constructor (type,
                                                                                          n_construct_properties,
                                                                                          construct_properties));
        priv = gsm_presence_get_instance_private (presence);

        res = register_presence (presence);
        if (! res) {
                g_warning ("Unable to register presence with session bus");
        }

        priv->screensaver_proxy = g_dbus_proxy_new_sync (priv->connection,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL,
                                                         GS_NAME,
                                                         GS_PATH,
                                                         GS_INTERFACE,
                                                         NULL, &error);
        if (error != NULL) {
                g_critical ("Unable to create a DBus proxy for GnomeScreensaver: %s",
                            error->message);
                g_error_free (error);
        } else {
                g_signal_connect (priv->screensaver_proxy, "notify::g-name-owner",
                                  G_CALLBACK (on_screensaver_name_owner_changed), presence);
                g_signal_connect (priv->screensaver_proxy, "g-signal",
                                  G_CALLBACK (on_screensaver_dbus_signal), presence);
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
}

GsmPresence *
gsm_presence_new (void)
{
        GsmPresence *presence;

        presence = g_object_new (GSM_TYPE_PRESENCE,
                                 NULL);

        return presence;
}
