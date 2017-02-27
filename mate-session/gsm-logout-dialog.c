/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Vincent Untz
 * Copyright (C) 2008 Red Hat, Inc.
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

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gsm-logout-dialog.h"
#ifdef HAVE_SYSTEMD
#include "gsm-systemd.h"
#endif
#include "gsm-consolekit.h"
#include "mdm.h"

#define GSM_LOGOUT_DIALOG_GET_PRIVATE(o)                                \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSM_TYPE_LOGOUT_DIALOG, GsmLogoutDialogPrivate))

#define GSM_ICON_LOGOUT   "system-log-out"
#define GSM_ICON_SHUTDOWN "system-shutdown"

#define SESSION_SCHEMA     "org.mate.session"
#define KEY_LOGOUT_TIMEOUT "logout-timeout"

#define LOCKDOWN_SCHEMA            "org.mate.lockdown"
#define KEY_USER_SWITCHING_DISABLE "disable-user-switching"

typedef enum {
        GSM_DIALOG_LOGOUT_TYPE_LOGOUT,
        GSM_DIALOG_LOGOUT_TYPE_SHUTDOWN
} GsmDialogLogoutType;

struct _GsmLogoutDialogPrivate
{
        GsmDialogLogoutType  type;
#ifdef HAVE_SYSTEMD
        GsmSystemd          *systemd;
#endif
        GsmConsolekit       *consolekit;

        GtkWidget           *progressbar;

        int                  timeout;
        unsigned int         timeout_id;

        unsigned int         default_response;
};

static GsmLogoutDialog *current_dialog = NULL;

static void gsm_logout_dialog_set_timeout  (GsmLogoutDialog *logout_dialog);

static void gsm_logout_dialog_destroy  (GsmLogoutDialog *logout_dialog,
                                        gpointer         data);

static void gsm_logout_dialog_show     (GsmLogoutDialog *logout_dialog,
                                        gpointer         data);

enum {
        PROP_0,
        PROP_MESSAGE_TYPE
};

G_DEFINE_TYPE (GsmLogoutDialog, gsm_logout_dialog, GTK_TYPE_MESSAGE_DIALOG);

static void
gsm_logout_dialog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        switch (prop_id) {
        case PROP_MESSAGE_TYPE:
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_logout_dialog_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
        switch (prop_id) {
        case PROP_MESSAGE_TYPE:
                g_value_set_enum (value, GTK_MESSAGE_WARNING);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_logout_dialog_class_init (GsmLogoutDialogClass *klass)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (klass);

        /* This is a workaround to avoid a stupid crash: libmateui
         * listens for the "show" signal on all GtkMessageDialog and
         * gets the "message-type" of the dialogs. We will crash when
         * it accesses this property if we don't override it since we
         * didn't define it. */
        gobject_class->set_property = gsm_logout_dialog_set_property;
        gobject_class->get_property = gsm_logout_dialog_get_property;

        g_object_class_override_property (gobject_class,
                                          PROP_MESSAGE_TYPE,
                                          "message-type");

        g_type_class_add_private (klass, sizeof (GsmLogoutDialogPrivate));
}

static void
gsm_logout_dialog_init (GsmLogoutDialog *logout_dialog)
{
        logout_dialog->priv = GSM_LOGOUT_DIALOG_GET_PRIVATE (logout_dialog);

        logout_dialog->priv->timeout_id = 0;
        logout_dialog->priv->timeout = 0;
        logout_dialog->priv->default_response = GTK_RESPONSE_CANCEL;

        gtk_window_set_skip_taskbar_hint (GTK_WINDOW (logout_dialog), TRUE);
        gtk_window_set_keep_above (GTK_WINDOW (logout_dialog), TRUE);
        gtk_window_stick (GTK_WINDOW (logout_dialog));
#ifdef HAVE_SYSTEMD
        if (LOGIND_RUNNING())
            logout_dialog->priv->systemd = gsm_get_systemd ();
        else
#endif
        logout_dialog->priv->consolekit = gsm_get_consolekit ();

        g_signal_connect (logout_dialog,
                          "destroy",
                          G_CALLBACK (gsm_logout_dialog_destroy),
                          NULL);

        g_signal_connect (logout_dialog,
                          "show",
                          G_CALLBACK (gsm_logout_dialog_show),
                          NULL);
}

static void
gsm_logout_dialog_destroy (GsmLogoutDialog *logout_dialog,
                           gpointer         data)
{
        if (logout_dialog->priv->timeout_id != 0) {
                g_source_remove (logout_dialog->priv->timeout_id);
                logout_dialog->priv->timeout_id = 0;
        }
#ifdef HAVE_SYSTEMD
        if (logout_dialog->priv->systemd) {
                g_object_unref (logout_dialog->priv->systemd);
                logout_dialog->priv->systemd = NULL;
        }
#endif

        if (logout_dialog->priv->consolekit) {
                g_object_unref (logout_dialog->priv->consolekit);
                logout_dialog->priv->consolekit = NULL;
        }

        current_dialog = NULL;
}

static gboolean
gsm_logout_supports_system_suspend (GsmLogoutDialog *logout_dialog)
{
        gboolean ret;
        ret = FALSE;
#ifdef HAVE_SYSTEMD
        if (LOGIND_RUNNING())
            ret = gsm_systemd_can_suspend (logout_dialog->priv->systemd);
#else
	ret = gsm_consolekit_can_suspend (logout_dialog->priv->consolekit);
#endif
        return ret;
}

static gboolean
gsm_logout_supports_system_hibernate (GsmLogoutDialog *logout_dialog)
{
        gboolean ret;
        ret = FALSE;
#ifdef HAVE_SYSTEMD
        if (LOGIND_RUNNING())
            ret = gsm_systemd_can_hibernate (logout_dialog->priv->systemd);
#else
        ret = gsm_consolekit_can_hibernate (logout_dialog->priv->consolekit);
#endif
        return ret;
}

static gboolean
gsm_logout_supports_switch_user (GsmLogoutDialog *logout_dialog)
{
        GSettings *settings;
        gboolean   ret = FALSE;
	gboolean   locked;

        settings = g_settings_new (LOCKDOWN_SCHEMA);

	locked = g_settings_get_boolean (settings, KEY_USER_SWITCHING_DISABLE);
	g_object_unref (settings);

	if (!locked) {
#ifdef HAVE_SYSTEMD
            if (LOGIND_RUNNING())
                ret = gsm_systemd_can_switch_user (logout_dialog->priv->systemd);
            else
#endif
            ret = gsm_consolekit_can_switch_user (logout_dialog->priv->consolekit);
        }

        return ret;
}

static gboolean
gsm_logout_supports_reboot (GsmLogoutDialog *logout_dialog)
{
        gboolean ret;

#ifdef HAVE_SYSTEMD
        if (LOGIND_RUNNING())
            ret = gsm_systemd_can_restart (logout_dialog->priv->systemd);
        else
#endif
        ret = gsm_consolekit_can_restart (logout_dialog->priv->consolekit);
        if (!ret) {
                ret = mdm_supports_logout_action (MDM_LOGOUT_ACTION_REBOOT);
        }

        return ret;
}

static gboolean
gsm_logout_supports_shutdown (GsmLogoutDialog *logout_dialog)
{
        gboolean ret;

#ifdef HAVE_SYSTEMD
        if (LOGIND_RUNNING())
            ret = gsm_systemd_can_stop (logout_dialog->priv->systemd);
        else
#endif
        ret = gsm_consolekit_can_stop (logout_dialog->priv->consolekit);

        if (!ret) {
                ret = mdm_supports_logout_action (MDM_LOGOUT_ACTION_SHUTDOWN);
        }

        return ret;
}

static void
gsm_logout_dialog_show (GsmLogoutDialog *logout_dialog, gpointer user_data)
{
        gsm_logout_dialog_set_timeout (logout_dialog);
}

static gboolean
gsm_logout_dialog_timeout (gpointer data)
{
        GsmLogoutDialog *logout_dialog;
        char            *seconds_warning;
        char            *secondary_text;
        static char     *session_type = NULL;
        static gboolean  is_not_login;

        logout_dialog = (GsmLogoutDialog *) data;

        if (!logout_dialog->priv->timeout) {
                gtk_dialog_response (GTK_DIALOG (logout_dialog),
                                     logout_dialog->priv->default_response);

                return FALSE;
        }

        switch (logout_dialog->priv->type) {
        case GSM_DIALOG_LOGOUT_TYPE_LOGOUT:
                seconds_warning = ngettext ("You will be automatically logged "
                                            "out in %d second",
                                            "You will be automatically logged "
                                            "out in %d seconds",
                                            logout_dialog->priv->timeout);
                break;

        case GSM_DIALOG_LOGOUT_TYPE_SHUTDOWN:
                seconds_warning = ngettext ("This system will be automatically "
                                            "shut down in %d second",
                                            "This system will be automatically "
                                            "shut down in %d seconds",
                                            logout_dialog->priv->timeout);
                break;

        default:
                g_assert_not_reached ();
        }
        seconds_warning = g_strdup_printf (seconds_warning, logout_dialog->priv->timeout);

        if (session_type == NULL) {
#ifdef HAVE_SYSTEMD
                if (LOGIND_RUNNING()) {
                    GsmSystemd *systemd;
                    systemd = gsm_get_systemd ();
                    session_type = gsm_systemd_get_current_session_type (systemd);
                    g_object_unref (systemd);
                    is_not_login = (g_strcmp0 (session_type, GSM_SYSTEMD_SESSION_TYPE_LOGIN_WINDOW) != 0);
                }
                else {
#endif
                GsmConsolekit *consolekit;
                consolekit = gsm_get_consolekit ();
                session_type = gsm_consolekit_get_current_session_type (consolekit);
                g_object_unref (consolekit);
                is_not_login = (g_strcmp0 (session_type, GSM_CONSOLEKIT_SESSION_TYPE_LOGIN_WINDOW) != 0);
#ifdef HAVE_SYSTEMD
                }
#endif
        }

        if (is_not_login) {
                char *name;

                name = g_locale_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);

                if (!name || name[0] == '\0' || strcmp (name, "Unknown") == 0) {
                        name = g_locale_to_utf8 (g_get_user_name (), -1 , NULL, NULL, NULL);
                }

                if (!name) {
                        name = g_strdup (g_get_user_name ());
                }

                secondary_text = g_strdup_printf (_("You are currently logged in as \"%s\"."), name);

                g_free (name);
        } else {
                secondary_text = g_strdup (seconds_warning);
        }

        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (logout_dialog->priv->progressbar),
                                       logout_dialog->priv->timeout / 60.0);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (logout_dialog->priv->progressbar),
                                   seconds_warning);

        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (logout_dialog),
                                                  secondary_text,
                                                  NULL);

        logout_dialog->priv->timeout--;

        g_free (secondary_text);
        g_free (seconds_warning);

        return TRUE;
}

static void
gsm_logout_dialog_set_timeout (GsmLogoutDialog *logout_dialog)
{
        GSettings *settings;

        settings = g_settings_new (SESSION_SCHEMA);

        logout_dialog->priv->timeout = g_settings_get_int (settings, KEY_LOGOUT_TIMEOUT);

        if (logout_dialog->priv->timeout > 0) {
                /* Sets the secondary text */
                gsm_logout_dialog_timeout (logout_dialog);

                if (logout_dialog->priv->timeout_id != 0) {
                        g_source_remove (logout_dialog->priv->timeout_id);
                }

                logout_dialog->priv->timeout_id = g_timeout_add (1000,
                                                                 gsm_logout_dialog_timeout,
                                                                 logout_dialog);
        }
        else {
                gtk_widget_hide (logout_dialog->priv->progressbar);
        }

        g_object_unref (settings);
}

static GtkWidget *
gsm_get_dialog (GsmDialogLogoutType type,
                GdkScreen          *screen,
                guint32             activate_time)
{
        GsmLogoutDialog *logout_dialog;
        GtkWidget       *hbox;
        const char      *primary_text;
        const char      *icon_name;

        if (current_dialog != NULL) {
                gtk_widget_destroy (GTK_WIDGET (current_dialog));
        }

        logout_dialog = g_object_new (GSM_TYPE_LOGOUT_DIALOG, NULL);

        current_dialog = logout_dialog;

        gtk_window_set_title (GTK_WINDOW (logout_dialog), "");

        logout_dialog->priv->type = type;

        icon_name = NULL;
        primary_text = NULL;

        switch (type) {
        case GSM_DIALOG_LOGOUT_TYPE_LOGOUT:
                icon_name    = GSM_ICON_LOGOUT;
                primary_text = _("Log out of this system now?");

                logout_dialog->priv->default_response = GSM_LOGOUT_RESPONSE_LOGOUT;

                if (gsm_logout_supports_switch_user (logout_dialog)) {
                        gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                               _("_Switch User"),
                                               GSM_LOGOUT_RESPONSE_SWITCH_USER);
                }

                gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_CANCEL);

                gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                       _("_Log Out"),
                                       GSM_LOGOUT_RESPONSE_LOGOUT);

                break;
        case GSM_DIALOG_LOGOUT_TYPE_SHUTDOWN:
                icon_name    = GSM_ICON_SHUTDOWN;
                primary_text = _("Shut down this system now?");

                logout_dialog->priv->default_response = GSM_LOGOUT_RESPONSE_SHUTDOWN;

                if (gsm_logout_supports_system_suspend (logout_dialog)) {
                        gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                               _("S_uspend"),
                                               GSM_LOGOUT_RESPONSE_SLEEP);
                }

                if (gsm_logout_supports_system_hibernate (logout_dialog)) {
                        gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                               _("_Hibernate"),
                                               GSM_LOGOUT_RESPONSE_HIBERNATE);
                }

                if (gsm_logout_supports_reboot (logout_dialog)) {
                        gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                               _("_Restart"),
                                               GSM_LOGOUT_RESPONSE_REBOOT);
                }

                gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_CANCEL);

                if (gsm_logout_supports_shutdown (logout_dialog)) {
                        gtk_dialog_add_button (GTK_DIALOG (logout_dialog),
                                               _("_Shut Down"),
                                               GSM_LOGOUT_RESPONSE_SHUTDOWN);
                }
                break;
        default:
                g_assert_not_reached ();
        }

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        logout_dialog->priv->progressbar = gtk_progress_bar_new ();
        gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (logout_dialog->priv->progressbar), TRUE);
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (logout_dialog->priv->progressbar), 1.0);
        gtk_box_pack_start (GTK_BOX (hbox),
                            logout_dialog->priv->progressbar,
                            TRUE, TRUE, 12);
        gtk_widget_show_all (hbox);
        gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (logout_dialog))), hbox);

        gtk_window_set_icon_name (GTK_WINDOW (logout_dialog), icon_name);
        gtk_window_set_position (GTK_WINDOW (logout_dialog), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (logout_dialog), primary_text);

        gtk_dialog_set_default_response (GTK_DIALOG (logout_dialog),
                                         logout_dialog->priv->default_response);

        gtk_window_set_screen (GTK_WINDOW (logout_dialog), screen);

        return GTK_WIDGET (logout_dialog);
}

GtkWidget *
gsm_get_shutdown_dialog (GdkScreen *screen,
                         guint32    activate_time)
{
        return gsm_get_dialog (GSM_DIALOG_LOGOUT_TYPE_SHUTDOWN,
                               screen,
                               activate_time);
}

GtkWidget *
gsm_get_logout_dialog (GdkScreen *screen,
                       guint32    activate_time)
{
        return gsm_get_dialog (GSM_DIALOG_LOGOUT_TYPE_LOGOUT,
                               screen,
                               activate_time);
}
