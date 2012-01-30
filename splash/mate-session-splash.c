/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * mate-session-splash.c
 * Copyright (C) 2007 Novell, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <mateconf/mateconf-client.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <glib/gi18n.h>

#include "eggsmclient.h"

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn-monitor.h>

#include "splash-window.h"

#define MATE_SESSION_DBUS_NAME      "org.mate.SessionManager"
#define MATE_SESSION_DBUS_OBJECT    "/org/mate/SessionManager"
#define MATE_SESSION_DBUS_INTERFACE "org.mate.SessionManager"

static int splash_clients = 0;

static DBusGConnection *
get_session_bus (void)
{
        GError *error;
        DBusGConnection *bus;
        DBusConnection *connection;

        error = NULL;
        bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (bus == NULL) {
                g_warning ("Couldn't connect to session bus: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        connection = dbus_g_connection_get_connection (bus);
        dbus_connection_set_exit_on_disconnect (connection, TRUE);

 out:
        return bus;
}

static void
on_session_running (DBusGProxy *proxy, gpointer data)
{
        gtk_main_quit ();
}

static void
set_session_running_handler ()
{
        DBusGConnection *bus;
        DBusGProxy *session_proxy;

        bus = get_session_bus ();
        if (bus == NULL) {
                g_warning ("Could not get a connection to the bus");
                return;
        }

        session_proxy = dbus_g_proxy_new_for_name (bus,
                                                   MATE_SESSION_DBUS_NAME,
                                                   MATE_SESSION_DBUS_OBJECT,
                                                   MATE_SESSION_DBUS_INTERFACE);

        dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE,
                                           G_TYPE_INVALID);

        dbus_g_proxy_add_signal (session_proxy,
                                 "SessionRunning",
                                 G_TYPE_INVALID);

        dbus_g_proxy_connect_signal (session_proxy,
                                     "SessionRunning",
                                     G_CALLBACK (on_session_running),
                                     NULL,
                                     NULL);

        dbus_g_connection_unref (bus);
}

static void
event_func (SnMonitorEvent *event,
            void *user_data)
{
        GsmSplashWindow   *splash = user_data;
        SnStartupSequence *seq;

        seq = sn_monitor_event_get_startup_sequence (event);

        switch (sn_monitor_event_get_type (event)) {
        case SN_MONITOR_EVENT_INITIATED:
                gsm_splash_window_start (splash,
                                         sn_startup_sequence_get_name (seq),
                                         sn_startup_sequence_get_icon_name (seq));
                splash_clients++;
                break;

        case SN_MONITOR_EVENT_COMPLETED:
                /* GSM (ab)uses startup notification to tell us when to quit */
                /* FIXME: Nope, doesn't actually work; libsn eats the notification */
                if (!strcmp (sn_startup_sequence_get_id (seq), "splash")) {
                        gtk_main_quit ();
                        return;
                }

                gsm_splash_window_finish (splash,
                                          sn_startup_sequence_get_name (seq));
                splash_clients--;
                if (splash_clients <= 0) {
                        gtk_main_quit ();
                        return;
                }
                break;

        default:
                break;
        }
}

static gboolean
splash_clicked (GtkWidget      *widget,
                GdkEventButton *event,
                gpointer        user_data)
{
        gtk_main_quit ();
        return TRUE;
}

static GdkFilterReturn
filter_func (GdkXEvent *gdkxevent,
             GdkEvent  *event,
             gpointer   user_data)
{
        SnDisplay *sn_display = user_data;

        sn_display_process_event (sn_display, (XEvent *)gdkxevent);
        return GDK_FILTER_CONTINUE;
}

static void
push_trap_func (SnDisplay *display, Display *xdisplay)
{
        gdk_error_trap_push ();
}

static void
pop_trap_func (SnDisplay *display, Display *xdisplay)
{
        gdk_error_trap_pop ();
}

static GdkPixbuf *
load_pixbuf (const char *path)
{
        GdkPixbuf *pixbuf;
        GError    *error = NULL;

        pixbuf = gdk_pixbuf_new_from_file (path, &error);
        if (pixbuf == NULL) {
                g_warning ("Failed to load image from '%s': %s\n",
                           path, error->message);
                g_error_free (error);
        }

        return pixbuf;
}

static GdkPixbuf *
load_pixbuf_relative_path (const char *filename)
{
        const char * const *xdg_data_dirs;
        int                 i;

        if (filename == NULL) {
                return NULL;
        }

        if (g_path_is_absolute (filename)) {
                return load_pixbuf (filename);
        }

        xdg_data_dirs = g_get_system_data_dirs ();

        for (i = 0; xdg_data_dirs[i] != NULL; i++) {
                GdkPixbuf *pixbuf;
                char      *path;

                path = g_build_filename (xdg_data_dirs[i], "pixmaps",
                                         filename, NULL);

                if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
                        g_free (path);
                        continue;
                }

                pixbuf = load_pixbuf (path);
                g_free (path);

                if (pixbuf)
                        return pixbuf;
        }

        return NULL;
}

static GdkPixbuf *
load_splash_pixbuf (void)
{
        MateConfClient *mateconf;
        char        *filename;
        GdkPixbuf   *pixbuf;

        mateconf = mateconf_client_get_default ();
        filename = mateconf_client_get_string (mateconf,
                                            "/apps/mate-session/options/splash_image",
                                            NULL);

        if (filename != NULL) {
                pixbuf = load_pixbuf_relative_path (filename);
                g_free (filename);
        } else {
                pixbuf = NULL;
        }

        if (pixbuf == NULL) {
                pixbuf = load_pixbuf_relative_path ("splash/mate-splash.png");
        }

        return pixbuf;
}

static void
setup_splash_window (void)
{
        GdkDisplay      *display;
        GdkScreen       *screen;
        GdkWindow       *root;
        SnDisplay       *sn_display;
        GtkWidget       *splash_widget;
        GsmSplashWindow *splash;
        GdkPixbuf       *background;

        /* Create the splash window */
        background = load_splash_pixbuf ();
        splash_widget = gsm_splash_window_new (background);
        splash = (GsmSplashWindow *)splash_widget;
        if (background)
                g_object_unref (background);

        /* Set up startup notification monitoring */
        display = gdk_display_get_default ();
        screen = gdk_display_get_default_screen (display);

        sn_display = sn_display_new (gdk_x11_display_get_xdisplay (display),
                                     push_trap_func, pop_trap_func);
        sn_monitor_context_new (sn_display, gdk_screen_get_number (screen),
                                event_func, splash, NULL);

        root = gdk_screen_get_root_window (screen);
        gdk_window_set_events (root, gdk_window_get_events (root) | GDK_PROPERTY_CHANGE_MASK);
        gdk_window_add_filter (NULL, filter_func, sn_display);

        /* Display the splash */
        gtk_widget_show (splash_widget);
        g_signal_connect (splash, "button-release-event",
                          G_CALLBACK (splash_clicked), NULL);
}

static void
quit (EggSMClient *smclient, gpointer user_data)
{
        gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
        GOptionContext *context;
        GError         *error;

        bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        context = g_option_context_new (N_("- MATE Splash Screen"));
        g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
        g_option_context_add_group (context,
                                    egg_sm_client_get_option_group ());
        g_option_context_add_group (context, gtk_get_option_group (TRUE));

        gtk_init (&argc, &argv);

        error = NULL;
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_printerr ("Cannot parse arguments: %s\n", error->message);
                g_error_free (error);
                g_option_context_free (context);

                return 1;
        }

        g_option_context_free (context);

        g_signal_connect (egg_sm_client_get (),
                          "quit",
                          G_CALLBACK (quit),
                          NULL);

        set_session_running_handler ();

        setup_splash_window ();
        gtk_main ();

        return 0;
}
