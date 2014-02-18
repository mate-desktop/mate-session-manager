/*
 * Copyright (C) 2013 Stefano Karapetsas <stefano@karapetsas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-login.h>
#endif

#include "gsm-marshal.h"
#include "gsm-systemd.h"

#define SD_NAME              "org.freedesktop.login1"
#define SD_PATH              "/org/freedesktop/login1"
#define SD_INTERFACE         "org.freedesktop.login1.Manager"
#define SD_SEAT_INTERFACE    "org.freedesktop.login1.Seat"
#define SD_SESSION_INTERFACE "org.freedesktop.login1.Session"

#define GSM_SYSTEMD_GET_PRIVATE(o)                                   \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSM_TYPE_SYSTEMD, GsmSystemdPrivate))

struct _GsmSystemdPrivate
{
    DBusGConnection *dbus_connection;
    DBusGProxy      *bus_proxy;
    DBusGProxy      *sd_proxy;
    guint32          is_connected : 1;
};

enum {
    PROP_0,
    PROP_IS_CONNECTED
};

enum {
    REQUEST_COMPLETED = 0,
    PRIVILEGES_COMPLETED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void     gsm_systemd_class_init   (GsmSystemdClass *klass);
static void     gsm_systemd_init         (GsmSystemd      *sd);
static void     gsm_systemd_finalize     (GObject         *object);

static void     gsm_systemd_free_dbus    (GsmSystemd      *manager);

static DBusHandlerResult gsm_systemd_dbus_filter (DBusConnection *connection,
                                                     DBusMessage    *message,
                                                     void           *user_data);

static void     gsm_systemd_on_name_owner_changed (DBusGProxy       *bus_proxy,
                                                   const char       *name,
                                                   const char       *prev_owner,
                                                   const char       *new_owner,
                                                   GsmSystemd       *manager);

G_DEFINE_TYPE (GsmSystemd, gsm_systemd, G_TYPE_OBJECT);

static void
gsm_systemd_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    GsmSystemd *manager = GSM_SYSTEMD (object);

    switch (prop_id) {
    case PROP_IS_CONNECTED:
        g_value_set_boolean (value,
                             manager->priv->is_connected);
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                               prop_id,
                                               pspec);
    }
}

static void
gsm_systemd_class_init (GsmSystemdClass *manager_class)
{
    GObjectClass *object_class;
    GParamSpec   *param_spec;

    object_class = G_OBJECT_CLASS (manager_class);

    object_class->finalize = gsm_systemd_finalize;
    object_class->get_property = gsm_systemd_get_property;

    param_spec = g_param_spec_boolean ("is-connected",
                                       "Is connected",
                                       "Whether the session is connected to Systemd",
                                       FALSE,
                                       G_PARAM_READABLE);

    g_object_class_install_property (object_class, PROP_IS_CONNECTED,
                                     param_spec);

    signals [REQUEST_COMPLETED] =
            g_signal_new ("request-completed",
                          G_OBJECT_CLASS_TYPE (object_class),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (GsmSystemdClass, request_completed),
                          NULL,
                          NULL,
                          g_cclosure_marshal_VOID__POINTER,
                          G_TYPE_NONE,
                          1, G_TYPE_POINTER);

    signals [PRIVILEGES_COMPLETED] =
            g_signal_new ("privileges-completed",
                          G_OBJECT_CLASS_TYPE (object_class),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (GsmSystemdClass, privileges_completed),
                          NULL,
                          NULL,
                          gsm_marshal_VOID__BOOLEAN_BOOLEAN_POINTER,
                          G_TYPE_NONE,
                          3, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_POINTER);

    g_type_class_add_private (manager_class, sizeof (GsmSystemdPrivate));
}

static DBusHandlerResult
gsm_systemd_dbus_filter (DBusConnection *connection,
                         DBusMessage    *message,
                         void           *user_data)
{
    GsmSystemd *manager;

    manager = GSM_SYSTEMD (user_data);

    if (dbus_message_is_signal (message,
                                DBUS_INTERFACE_LOCAL, "Disconnected") &&
        strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
            gsm_systemd_free_dbus (manager);
            return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
gsm_systemd_ensure_sd_connection (GsmSystemd  *manager,
                                  GError     **error)
{
    GError  *connection_error;
    gboolean is_connected;

    connection_error = NULL;

    if (manager->priv->dbus_connection == NULL) {
        DBusConnection *connection;

        manager->priv->dbus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM,
                                                         &connection_error);

        if (manager->priv->dbus_connection == NULL) {
            g_propagate_error (error, connection_error);
            is_connected = FALSE;
            goto out;
        }

        connection = dbus_g_connection_get_connection (manager->priv->dbus_connection);
        dbus_connection_set_exit_on_disconnect (connection, FALSE);
        dbus_connection_add_filter (connection,
                                    gsm_systemd_dbus_filter,
                                    manager, NULL);
    }

    if (manager->priv->bus_proxy == NULL) {
        manager->priv->bus_proxy =
            dbus_g_proxy_new_for_name_owner (manager->priv->dbus_connection,
                                             DBUS_SERVICE_DBUS,
                                             DBUS_PATH_DBUS,
                                             DBUS_INTERFACE_DBUS,
                                             &connection_error);

        if (manager->priv->bus_proxy == NULL) {
            g_propagate_error (error, connection_error);
            is_connected = FALSE;
            goto out;
        }

        dbus_g_proxy_add_signal (manager->priv->bus_proxy,
                                 "NameOwnerChanged",
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_INVALID);

        dbus_g_proxy_connect_signal (manager->priv->bus_proxy,
                                     "NameOwnerChanged",
                                     G_CALLBACK (gsm_systemd_on_name_owner_changed),
                                     manager, NULL);
    }

    if (manager->priv->sd_proxy == NULL) {
        manager->priv->sd_proxy =
                dbus_g_proxy_new_for_name_owner (manager->priv->dbus_connection,
                                                 SD_NAME,
                                                 SD_PATH,
                                                 SD_INTERFACE,
                                                 &connection_error);

        if (manager->priv->sd_proxy == NULL) {
            g_propagate_error (error, connection_error);
            is_connected = FALSE;
            goto out;
        }
    }

    is_connected = TRUE;

out:
    if (manager->priv->is_connected != is_connected) {
        manager->priv->is_connected = is_connected;
        g_object_notify (G_OBJECT (manager), "is-connected");
    }

    if (!is_connected) {
        if (manager->priv->dbus_connection == NULL) {
            if (manager->priv->bus_proxy != NULL) {
                g_object_unref (manager->priv->bus_proxy);
                manager->priv->bus_proxy = NULL;
            }

            if (manager->priv->sd_proxy != NULL) {
                g_object_unref (manager->priv->sd_proxy);
                manager->priv->sd_proxy = NULL;
            }
        } else if (manager->priv->bus_proxy == NULL) {
            if (manager->priv->sd_proxy != NULL) {
                g_object_unref (manager->priv->sd_proxy);
                manager->priv->sd_proxy = NULL;
            }
        }
    }

    return is_connected;
}

static void
gsm_systemd_on_name_owner_changed (DBusGProxy    *bus_proxy,
                                   const char    *name,
                                   const char    *prev_owner,
                                   const char    *new_owner,
                                   GsmSystemd    *manager)
{
    if (name != NULL && g_strcmp0 (name, SD_NAME) != 0) {
        return;
    }

    if (manager->priv->sd_proxy != NULL) {
        g_object_unref (manager->priv->sd_proxy);
        manager->priv->sd_proxy = NULL;
    }

    gsm_systemd_ensure_sd_connection (manager, NULL);
}

static void
gsm_systemd_init (GsmSystemd *manager)
{
    GError *error;

    manager->priv = GSM_SYSTEMD_GET_PRIVATE (manager);

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        g_error_free (error);
    }
}

static void
gsm_systemd_free_dbus (GsmSystemd *manager)
{
    if (manager->priv->bus_proxy != NULL) {
        g_object_unref (manager->priv->bus_proxy);
        manager->priv->bus_proxy = NULL;
    }

    if (manager->priv->sd_proxy != NULL) {
        g_object_unref (manager->priv->sd_proxy);
        manager->priv->sd_proxy = NULL;
    }

    if (manager->priv->dbus_connection != NULL) {
        DBusConnection *connection;
        connection = dbus_g_connection_get_connection (manager->priv->dbus_connection);
        dbus_connection_remove_filter (connection,
                                       gsm_systemd_dbus_filter,
                                       manager);

        dbus_g_connection_unref (manager->priv->dbus_connection);
        manager->priv->dbus_connection = NULL;
    }
}

static void
gsm_systemd_finalize (GObject *object)
{
    GsmSystemd *manager;
    GObjectClass  *parent_class;

    manager = GSM_SYSTEMD (object);

    parent_class = G_OBJECT_CLASS (gsm_systemd_parent_class);

    gsm_systemd_free_dbus (manager);

    if (parent_class->finalize != NULL) {
        parent_class->finalize (object);
    }
}

GQuark
gsm_systemd_error_quark (void)
{
    static GQuark error_quark = 0;

    if (error_quark == 0) {
        error_quark = g_quark_from_static_string ("gsm-systemd-error");
    }

    return error_quark;
}

GsmSystemd *
gsm_systemd_new (void)
{
    GsmSystemd *manager;

    manager = g_object_new (GSM_TYPE_SYSTEMD, NULL);

    return manager;
}

static void
emit_restart_complete (GsmSystemd *manager,
                       GError     *error)
{
    GError *call_error;

    call_error = NULL;

    if (error != NULL) {
        call_error = g_error_new_literal (GSM_SYSTEMD_ERROR,
                                          GSM_SYSTEMD_ERROR_RESTARTING,
                                          error->message);
    }

    g_signal_emit (G_OBJECT (manager),
                   signals [REQUEST_COMPLETED],
                   0, call_error);

    if (call_error != NULL) {
        g_error_free (call_error);
    }
}

static void
emit_stop_complete (GsmSystemd *manager,
                    GError     *error)
{
    GError *call_error;

    call_error = NULL;

    if (error != NULL) {
        call_error = g_error_new_literal (GSM_SYSTEMD_ERROR,
                                          GSM_SYSTEMD_ERROR_STOPPING,
                                          error->message);
    }

    g_signal_emit (G_OBJECT (manager),
                   signals [REQUEST_COMPLETED],
                   0, call_error);

    if (call_error != NULL) {
        g_error_free (call_error);
    }
}

void
gsm_systemd_attempt_restart (GsmSystemd *manager)
{
    gboolean res;
    GError  *error;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        emit_restart_complete (manager, error);
        g_error_free (error);
        return;
    }

    res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
                                          "Reboot",
                                          INT_MAX,
                                          &error,
                                          G_TYPE_BOOLEAN, TRUE, /* interactive */
                                          G_TYPE_INVALID,
                                          G_TYPE_INVALID);

    if (!res) {
        g_warning ("Unable to restart system: %s", error->message);
        emit_restart_complete (manager, error);
        g_error_free (error);
    } else {
        emit_restart_complete (manager, NULL);
    }
}

void
gsm_systemd_attempt_stop (GsmSystemd *manager)
{
    gboolean res;
    GError  *error;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        emit_stop_complete (manager, error);
        g_error_free (error);
        return;
    }

    res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
                                          "PowerOff",
                                          INT_MAX,
                                          &error,
                                          G_TYPE_BOOLEAN, TRUE, /* interactive */
                                          G_TYPE_INVALID,
                                          G_TYPE_INVALID);

    if (!res) {
        g_warning ("Unable to stop system: %s", error->message);
        emit_stop_complete (manager, error);
        g_error_free (error);
    } else {
        emit_stop_complete (manager, NULL);
    }
}

static void
gsm_systemd_get_session_path (DBusConnection  *connection,
                              char           **session_path)
{
    DBusError       local_error;
    DBusMessage    *message;
    DBusMessage    *reply;
    DBusMessageIter iter;
    gchar          *session_id = NULL;

#ifdef HAVE_SYSTEMD
    sd_pid_get_session (getpid (), &session_id);
#endif

    if (session_id == NULL)
        return;

    reply = NULL;

    dbus_error_init (&local_error);
    message = dbus_message_new_method_call (SD_NAME,
                                            SD_PATH,
                                            SD_INTERFACE,
                                            "GetSession");
    if (message == NULL) {
            goto out;
    }

    dbus_message_iter_init_append (message, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_STRING, &session_id);

    dbus_error_init (&local_error);
    reply = dbus_connection_send_with_reply_and_block (connection,
                                                       message,
                                                       -1,
                                                       &local_error);
    if (reply == NULL) {
        if (dbus_error_is_set (&local_error)) {
            g_warning ("Unable to get session path: %s", local_error.message);
            dbus_error_free (&local_error);
            goto out;
        }
    }

    dbus_message_iter_init (reply, &iter);
    dbus_message_iter_get_basic (&iter, session_path);

out:
    if (message != NULL) {
        dbus_message_unref (message);
    }
    if (reply != NULL) {
        dbus_message_unref (reply);
    }
    if (session_id != NULL) {
        g_free (session_id);
    }
}


void
gsm_systemd_set_session_idle (GsmSystemd *manager,
                              gboolean       is_idle)
{
    GError         *error;
    char           *session_path = NULL;
    DBusMessage    *message;
    DBusMessage    *reply;
    DBusError       dbus_error;
    DBusMessageIter iter;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        g_error_free (error);
        return;
    }

    gsm_systemd_get_session_path (dbus_g_connection_get_connection (manager->priv->dbus_connection), &session_path);

    g_return_if_fail (session_path != NULL);

    g_debug ("Updating Systemd idle status: %d", is_idle);
    message = dbus_message_new_method_call (SD_NAME,
                                            session_path,
                                            SD_SESSION_INTERFACE,
                                            "SetIdleHint");
    if (message == NULL) {
            g_debug ("Couldn't allocate the D-Bus message");
            return;
    }

    dbus_message_iter_init_append (message, &iter);
    dbus_message_iter_append_basic (&iter, DBUS_TYPE_BOOLEAN, &is_idle);

    /* FIXME: use async? */
    dbus_error_init (&dbus_error);
    reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection (manager->priv->dbus_connection),
                                                       message,
                                                       -1,
                                                       &dbus_error);
    dbus_message_unref (message);

    if (reply != NULL) {
        dbus_message_unref (reply);
    }

    if (dbus_error_is_set (&dbus_error)) {
        g_debug ("%s raised:\n %s\n\n", dbus_error.name, dbus_error.message);
        dbus_error_free (&dbus_error);
    }
}

gboolean
gsm_systemd_can_switch_user (GsmSystemd *manager)
{
    GError  *error;
    char    *session_id = NULL;
#ifdef HAVE_SYSTEMD
    char    *seat_id = NULL;
#endif
    int      ret = 0;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

#ifdef HAVE_SYSTEMD
    sd_pid_get_session (getpid (), &session_id);
#endif

    if (session_id == NULL)
        return FALSE;

#ifdef HAVE_SYSTEMD
    sd_session_get_seat (session_id, &seat_id);
    ret = sd_seat_can_multi_session (seat_id);

    g_free (session_id);
    g_free (seat_id);
#endif

    return ret > 0;
}

gboolean
gsm_systemd_get_restart_privileges (GsmSystemd *manager)
{
    g_signal_emit (G_OBJECT (manager),
               signals [PRIVILEGES_COMPLETED],
               0, TRUE, TRUE, NULL);

    return TRUE;
}

gboolean
gsm_systemd_get_stop_privileges (GsmSystemd *manager)
{
    g_signal_emit (G_OBJECT (manager),
               signals [PRIVILEGES_COMPLETED],
               0, TRUE, TRUE, NULL);

    return TRUE;
}

gboolean
gsm_systemd_can_restart (GsmSystemd *manager)
{
    gboolean res;
    gchar   *value;
    gboolean can_restart;
    GError  *error;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
                                          "CanReboot",
                                          INT_MAX,
                                          &error,
                                          G_TYPE_INVALID,
                                          G_TYPE_STRING, &value,
                                          G_TYPE_INVALID);
    if (res == FALSE) {
        g_warning ("Could not make DBUS call: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    can_restart = g_strcmp0 (value, "yes") == 0 ||
                  g_strcmp0 (value, "challenge") == 0;
    g_free (value);
    return can_restart;
}

gboolean
gsm_systemd_can_stop (GsmSystemd *manager)
{
    gboolean res;
    gchar   *value;
    gboolean can_stop;
    GError  *error;

    error = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
        g_warning ("Could not connect to Systemd: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
                                          "CanPowerOff",
                                          INT_MAX,
                                          &error,
                                          G_TYPE_INVALID,
                                          G_TYPE_STRING, &value,
                                          G_TYPE_INVALID);

    if (res == FALSE) {
        g_warning ("Could not make DBUS call: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    can_stop = g_strcmp0 (value, "yes") == 0 ||
               g_strcmp0 (value, "challenge") == 0;
    g_free (value);
    return can_stop;
}

gboolean
gsm_systemd_can_hibernate (GsmSystemd *manager)
{
  gboolean res;
  gchar   *value;
  gboolean can_hibernate;
  GError  *error;
  
  error = NULL;
  
  if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
    g_warning ("Could not connect to Systemd: %s",
	       error->message);
    g_error_free (error);
    return FALSE;
  }
  
  res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
					"CanHibernate",
					INT_MAX,
					&error,
					G_TYPE_INVALID,
					G_TYPE_STRING, &value,
					G_TYPE_INVALID);
  if (res == FALSE) {
    g_warning ("Could not make DBUS call: %s",
	       error->message);
    g_error_free (error);
    return FALSE;
  }
  
  can_hibernate = g_strcmp0 (value, "yes") == 0 ||
  g_strcmp0 (value, "challenge") == 0;
  g_free (value);
  return can_hibernate;
}

gboolean
gsm_systemd_can_suspend (GsmSystemd *manager)
{
  gboolean res;
  gchar   *value;
  gboolean can_suspend;
  GError  *error;
  
  error = NULL;
  
  if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
    g_warning ("Could not connect to Systemd: %s",
	       error->message);
    g_error_free (error);
    return FALSE;
  }
  
  res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
					"CanSuspend",
					INT_MAX,
					&error,
					G_TYPE_INVALID,
					G_TYPE_STRING, &value,
					G_TYPE_INVALID);
  if (res == FALSE) {
    g_warning ("Could not make DBUS call: %s",
	       error->message);
    g_error_free (error);
    return FALSE;
  }
  
  can_suspend = g_strcmp0 (value, "yes") == 0 ||
  g_strcmp0 (value, "challenge") == 0;
  g_free (value);
  return can_suspend;
}

void
gsm_systemd_attempt_hibernate (GsmSystemd *manager)
{
  gboolean res;
  GError  *error;
  
  error = NULL;
  
  if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
    g_warning ("Could not connect to Systemd: %s",
	       error->message);
    g_error_free (error);
    return;
  }
  
  res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
					"Hibernate",
					INT_MAX,
					&error,
                                        G_TYPE_BOOLEAN, TRUE, /* interactive */
					G_TYPE_INVALID,
					G_TYPE_INVALID);
  if (res == FALSE) {
    g_warning ("Could not make DBUS call: %s",
	       error->message);
    g_error_free (error);
    return;
  }
  
}

void
gsm_systemd_attempt_suspend (GsmSystemd *manager)
{
  gboolean res;
  GError  *error;
  
  error = NULL;
  
  if (!gsm_systemd_ensure_sd_connection (manager, &error)) {
    g_warning ("Could not connect to Systemd: %s",
	       error->message);
    g_error_free (error);
    return;
  }
  
  res = dbus_g_proxy_call_with_timeout (manager->priv->sd_proxy,
					"Suspend",
					INT_MAX,
					&error,
                                        G_TYPE_BOOLEAN, TRUE, /* interactive */
                                        G_TYPE_INVALID,
					G_TYPE_INVALID);
  if (res == FALSE) {
    g_warning ("Could not make DBUS call: %s",
	       error->message);
    g_error_free (error);
    return;
  }
}

gchar *
gsm_systemd_get_current_session_type (GsmSystemd *manager)
{
    GError   *gerror;
    gchar    *session_id = NULL;
    gchar    *session_class = NULL;
#ifdef HAVE_SYSTEMD
    int       res;
#endif

    gerror = NULL;

    if (!gsm_systemd_ensure_sd_connection (manager, &gerror)) {
        g_warning ("Could not connect to Systemd: %s",
                   gerror->message);
        g_error_free (gerror);
        return NULL;
    }

#ifdef HAVE_SYSTEMD
    sd_pid_get_session (getpid (), &session_id);
#endif

    if (session_id == NULL)
        return NULL;

#ifdef HAVE_SYSTEMD
    res = sd_session_get_class (session_id, &session_class);
    if (res < 0) {
        g_warning ("Could not get Systemd session class!");
        return NULL;
    }

    g_free (session_id);
#endif

    return session_class;
}


GsmSystemd *
gsm_get_systemd (void)
{
    static GsmSystemd *manager = NULL;

    if (manager == NULL) {
        manager = gsm_systemd_new ();
    }

    return g_object_ref (manager);
}
