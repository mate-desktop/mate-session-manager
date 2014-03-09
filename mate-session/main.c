/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Novell, Inc.
 * Copyright (C) 2008 Red Hat, Inc.
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

#include <config.h>

#include <libintl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "mdm-signal-handler.h"
#include "mdm-log.h"

#include "gsm-consolekit.h"
#ifdef HAVE_SYSTEMD
#include "gsm-systemd.h"
#endif
#include "gsm-util.h"
#include "gsm-manager.h"
#include "gsm-xsmp-server.h"
#include "gsm-store.h"

#include "msm-gnome.h"

#define GSM_SCHEMA "org.mate.session"
#define GSM_DEFAULT_SESSION_KEY "default-session"
#define GSM_REQUIRED_COMPONENTS_SCHEMA GSM_SCHEMA ".required-components"
#define GSM_REQUIRED_COMPONENTS_LIST_KEY "required-components-list"

#define ACCESSIBILITY_KEY     "accessibility"
#define ACCESSIBILITY_SCHEMA  "org.mate.interface"

#define VISUAL_SCHEMA         "org.mate.applications-at-visual"
#define VISUAL_KEY            "exec"
#define VISUAL_STARTUP_KEY    "startup"

#define MOBILITY_SCHEMA       "org.mate.applications-at-mobility"
#define MOBILITY_KEY          "exec"
#define MOBILITY_STARTUP_KEY  "startup"

#define GSM_DBUS_NAME "org.mate.SessionManager"

#define IS_STRING_EMPTY(x) \
	((x) == NULL || (x)[0] == '\0')

#define KEY_AUTOSAVE "auto-save-session"

static gboolean failsafe = FALSE;
static gboolean show_version = FALSE;
static gboolean debug = FALSE;

static gboolean
initialize_gsettings (void)
{
	GSettings* settings;
	time_t now = time (0);
	gboolean ret;

	settings = g_settings_new (GSM_SCHEMA);

	if (!settings)
		return FALSE;

	ret = g_settings_set_int (settings, "session-start", now);

	g_settings_sync ();

	g_object_unref (settings);

        return ret;
}
	
static void on_bus_name_lost(DBusGProxy* bus_proxy, const char* name, gpointer data)
{
	g_warning("Lost name on bus: %s, exiting", name);
	exit(1);
}

static gboolean acquire_name_on_proxy(DBusGProxy* bus_proxy, const char* name)
{
	GError* error;
	guint result;
	gboolean res;
	gboolean ret;

	ret = FALSE;

	if (bus_proxy == NULL)
	{
		goto out;
	}

	error = NULL;
	res = dbus_g_proxy_call(bus_proxy, "RequestName", &error, G_TYPE_STRING, name, G_TYPE_UINT, 0, G_TYPE_INVALID, G_TYPE_UINT, &result, G_TYPE_INVALID);

	if (! res)
	{
		if (error != NULL)
		{
			g_warning("Failed to acquire %s: %s", name, error->message);
			g_error_free(error);
		}
		else
		{
			g_warning ("Failed to acquire %s", name);
		}

		goto out;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
		if (error != NULL)
		{
			g_warning("Failed to acquire %s: %s", name, error->message);
			g_error_free(error);
		}
		else
		{
			g_warning("Failed to acquire %s", name);
		}

		goto out;
	}

	/* register for name lost */
	dbus_g_proxy_add_signal(bus_proxy, "NameLost", G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(bus_proxy, "NameLost", G_CALLBACK(on_bus_name_lost), NULL, NULL);

	ret = TRUE;

	out:

	return ret;
}

static gboolean acquire_name(void)
{
	DBusGProxy* bus_proxy;
	GError* error;
	DBusGConnection* connection;

	error = NULL;
	connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

	if (connection == NULL)
	{
		gsm_util_init_error(TRUE, "Could not connect to session bus: %s", error->message);
		/* not reached */
	}

	bus_proxy = dbus_g_proxy_new_for_name(connection, DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	if (!acquire_name_on_proxy(bus_proxy, GSM_DBUS_NAME))
	{
		gsm_util_init_error(TRUE, "%s", "Could not acquire name on session bus");
		/* not reached */
	}

	g_object_unref(bus_proxy);

	return TRUE;
}

/* This doesn't contain the required components, so we need to always
 * call append_required_apps() after a call to append_default_apps(). */
static void append_default_apps(GsmManager* manager, const char* default_session_key, char** autostart_dirs)
{
	gint i;
	gchar** default_apps;
	GSettings* settings;

	g_debug("main: *** Adding default apps");

	g_assert(default_session_key != NULL);
	g_assert(autostart_dirs != NULL);

	settings = g_settings_new (GSM_SCHEMA);
	default_apps = g_settings_get_strv (settings, default_session_key);
	g_object_unref(settings);

	for (i = 0; default_apps[i]; i++)
	{
		char* app_path;

		if (IS_STRING_EMPTY((char*) default_apps[i]))
		{
			continue;
		}

		app_path = gsm_util_find_desktop_file_for_app_name(default_apps[i], autostart_dirs);

		if (app_path != NULL)
		{
			gsm_manager_add_autostart_app(manager, app_path, NULL);
			g_free(app_path);
		}
	}

	g_strfreev (default_apps);
}

static void append_required_apps(GsmManager* manager)
{
	gchar** required_components;
	gint i;
	GSettings* settings;
	GSettings* settings_required_components;

	g_debug("main: *** Adding required apps");

	settings = g_settings_new (GSM_SCHEMA);
	settings_required_components = g_settings_new (GSM_REQUIRED_COMPONENTS_SCHEMA);

	required_components = g_settings_get_strv(settings, GSM_REQUIRED_COMPONENTS_LIST_KEY);

	if (required_components == NULL)
	{
		g_warning("No required applications specified");
	}

	for (i = 0; required_components[i]; i++)
	{
			char* default_provider;
			const char* component;

			if (IS_STRING_EMPTY((char*) required_components[i]))
			{
					continue;
			}

			component = required_components[i];

			default_provider = g_settings_get_string (settings_required_components, component);

			g_debug ("main: %s looking for component: '%s'", component, default_provider);

			if (default_provider != NULL)
			{
				char* app_path;

				app_path = gsm_util_find_desktop_file_for_app_name(default_provider, NULL);

				if (app_path != NULL)
				{
					gsm_manager_add_autostart_app(manager, app_path, component);
				}
				else
				{
					g_warning("Unable to find provider '%s' of required component '%s'", default_provider, component);
				}

				g_free(app_path);
			}

			g_free(default_provider);
	}

	g_debug("main: *** Done adding required apps");

	g_strfreev(required_components);

	g_object_unref(settings);
	g_object_unref(settings_required_components);
}

static void append_accessibility_apps(GsmManager* manager)
{
	GSettings* mobility_settings;
	GSettings* visual_settings;

	g_debug("main: *** Adding accesibility apps");

	mobility_settings = g_settings_new (MOBILITY_SCHEMA);
	visual_settings = g_settings_new (VISUAL_SCHEMA);

	if (g_settings_get_boolean (mobility_settings, MOBILITY_STARTUP_KEY))
	{
		gchar *mobility_exec;
		mobility_exec = g_settings_get_string (mobility_settings, MOBILITY_KEY);
		if (mobility_exec != NULL && mobility_exec[0] != 0)
		{
			char* app_path;
			app_path = gsm_util_find_desktop_file_for_app_name(mobility_exec, NULL);
			if (app_path != NULL)
			{
				gsm_manager_add_autostart_app(manager, app_path, NULL);
				g_free (app_path);
			}
			g_free (mobility_exec);
		}
	}

	if (g_settings_get_boolean (visual_settings, VISUAL_STARTUP_KEY))
	{
		gchar *visual_exec;
		visual_exec = g_settings_get_string (visual_settings, VISUAL_KEY);
		if (visual_exec != NULL && visual_exec[0] != 0)
		{
			char* app_path;
			app_path = gsm_util_find_desktop_file_for_app_name(visual_exec, NULL);
			if (app_path != NULL)
			{
				gsm_manager_add_autostart_app(manager, app_path, NULL);
				g_free (app_path);
			}
			g_free (visual_exec);
		}
	}

	g_object_unref (mobility_settings);
	g_object_unref (visual_settings);
}

static void maybe_load_saved_session_apps(GsmManager* manager)
{
	GsmConsolekit* consolekit = NULL;
#ifdef HAVE_SYSTEMD
	GsmSystemd* systemd = NULL;
#endif
	char* session_type;
	gboolean is_login;

#ifdef HAVE_SYSTEMD
	if (LOGIND_RUNNING()) {
		systemd = gsm_get_systemd();
		session_type = gsm_systemd_get_current_session_type(systemd);
		is_login = g_strcmp0 (session_type, GSM_SYSTEMD_SESSION_TYPE_LOGIN_WINDOW) == 0;
	}
	else {
#endif
	consolekit = gsm_get_consolekit();
	session_type = gsm_consolekit_get_current_session_type(consolekit);
	is_login = g_strcmp0 (session_type, GSM_CONSOLEKIT_SESSION_TYPE_LOGIN_WINDOW) == 0;
#ifdef HAVE_SYSTEMD
	}
#endif

	if (!is_login)
	{
		GSettings* settings;
		gboolean autostart;

		settings = g_settings_new (GSM_SCHEMA);
		autostart = g_settings_get_boolean (settings, KEY_AUTOSAVE);
		g_object_unref (settings);

		if (autostart == TRUE)
			gsm_manager_add_autostart_apps_from_dir(manager, gsm_util_get_saved_session_dir());
	}

	if (consolekit != NULL)
		g_object_unref(consolekit);
#ifdef HAVE_SYSTEMD
	if (systemd != NULL)
		g_object_unref(systemd);
#endif
	g_free(session_type);
}

static void load_standard_apps (GsmManager* manager, const char* default_session_key)
{
	char** autostart_dirs;
	int i;

	autostart_dirs = gsm_util_get_autostart_dirs();

	if (!failsafe)
	{
		maybe_load_saved_session_apps(manager);

		for (i = 0; autostart_dirs[i]; i++)
		{
			gsm_manager_add_autostart_apps_from_dir(manager, autostart_dirs[i]);
		}
	}

	/* We do this at the end in case a saved session contains an
	 * application that already provides one of the components. */
	append_default_apps(manager, default_session_key, autostart_dirs);
	append_required_apps(manager);
	append_accessibility_apps(manager);

	g_strfreev(autostart_dirs);
}

static void load_override_apps(GsmManager* manager, char** override_autostart_dirs)
{
	int i;

	for (i = 0; override_autostart_dirs[i]; i++)
	{
		gsm_manager_add_autostart_apps_from_dir(manager, override_autostart_dirs[i]);
	}
}

static gboolean signal_cb(int signo, gpointer data)
{
	int ret;
	GsmManager* manager;

	g_debug("Got callback for signal %d", signo);

	ret = TRUE;

	switch (signo)
	{
		case SIGFPE:
		case SIGPIPE:
			/* let the fatal signals interrupt us */
			g_debug ("Caught signal %d, shutting down abnormally.", signo);
			ret = FALSE;
			break;
		case SIGINT:
		case SIGTERM:
			manager = (GsmManager*) data;
			gsm_manager_logout(manager, GSM_MANAGER_LOGOUT_MODE_FORCE, NULL);

			/* let the fatal signals interrupt us */
			g_debug("Caught signal %d, shutting down normally.", signo);
			ret = TRUE;
			break;
		case SIGHUP:
			g_debug("Got HUP signal");
			ret = TRUE;
			break;
		case SIGUSR1:
			g_debug("Got USR1 signal");
			ret = TRUE;
			mdm_log_toggle_debug();
			break;
		default:
			g_debug("Caught unhandled signal %d", signo);
			ret = TRUE;

			break;
	}

	return ret;
}

static void shutdown_cb(gpointer data)
{
	GsmManager* manager = (GsmManager*) data;
	g_debug("Calling shutdown callback function");

	/*
	 * When the signal handler gets a shutdown signal, it calls
	 * this function to inform GsmManager to not restart
	 * applications in the off chance a handler is already queued
	 * to dispatch following the below call to gtk_main_quit.
	 */
	gsm_manager_set_phase(manager, GSM_MANAGER_PHASE_EXIT);

	gtk_main_quit();
}

static gboolean require_dbus_session(int argc, char** argv, GError** error)
{
	char** new_argv;
	int i;

	if (g_getenv("DBUS_SESSION_BUS_ADDRESS"))
	{
		return TRUE;
	}

	/* Just a sanity check to prevent infinite recursion if
	 * dbus-launch fails to set DBUS_SESSION_BUS_ADDRESS
	 */
	g_return_val_if_fail(!g_str_has_prefix(argv[0], "dbus-launch"), TRUE);

	/* +2 for our new arguments, +1 for NULL */
	new_argv = g_malloc(argc + 3 * sizeof (*argv));

	new_argv[0] = "dbus-launch";
	new_argv[1] = "--exit-with-session";

	for (i = 0; i < argc; i++)
	{
		new_argv[i + 2] = argv[i];
	}

	new_argv[i + 2] = NULL;

	if (!execvp("dbus-launch", new_argv))
	{
		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "No session bus and could not exec dbus-launch: %s", g_strerror(errno));
		return FALSE;
	}

	/* Should not be reached */
	return TRUE;
}

int main(int argc, char** argv)
{
	struct sigaction sa;
	GError* error;
#if GTK_CHECK_VERSION (3, 0, 0)
	const char* display_str;
#else
	char* display_str;
#endif
	GsmManager* manager;
	GsmStore* client_store;
	GsmXsmpServer* xsmp_server;
	GSettings* accessibility_settings;
	MdmSignalHandler* signal_handler;
	static char** override_autostart_dirs = NULL;

	static GOptionEntry entries[] = {
		{"autostart", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &override_autostart_dirs, N_("Override standard autostart directories"), NULL},
		{"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging code"), NULL},
		{"failsafe", 'f', 0, G_OPTION_ARG_NONE, &failsafe, N_("Do not load user-specified applications"), NULL},
		{"version", 0, 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	/* Make sure that we have a session bus */
	if (!require_dbus_session(argc, argv, &error))
	{
		gsm_util_init_error(TRUE, "%s", error->message);
	}

	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGPIPE, &sa, 0);

	error = NULL;
	gtk_init_with_args(&argc, &argv, (char*) _(" - the MATE session manager"), entries, GETTEXT_PACKAGE, &error);

	if (error != NULL)
	{
		g_warning("%s", error->message);
		exit(1);
	}

	if (show_version)
	{
		g_print("%s %s\n", argv [0], VERSION);
		exit(1);
	}

	mdm_log_init();
	mdm_log_set_debug(debug);

	/* Set DISPLAY explicitly for all our children, in case --display
	 * was specified on the command line.
	 */
#if GTK_CHECK_VERSION (3, 0, 0)
	display_str = gdk_display_get_name (gdk_display_get_default());
#else
	display_str = gdk_get_display();
#endif
	gsm_util_setenv("DISPLAY", display_str);
#if !GTK_CHECK_VERSION (3, 0, 0)
	g_free(display_str);
#endif

	/* Some third-party programs rely on MATE_DESKTOP_SESSION_ID to
	 * detect if MATE is running. We keep this for compatibility reasons.
	 */
	gsm_util_setenv("MATE_DESKTOP_SESSION_ID", "this-is-deprecated");

	/*
	 * Make sure gsettings is set up correctly.  If not, then bail.
	 */

	if (initialize_gsettings () != TRUE)
		exit (1);

	/* Look if accessibility is enabled */
	accessibility_settings = g_settings_new (ACCESSIBILITY_SCHEMA);
	if (g_settings_get_boolean (accessibility_settings, ACCESSIBILITY_KEY))
	{
		gsm_util_setenv("GTK_MODULES", "gail:atk-bridge");
	}
	g_object_unref (accessibility_settings);

	client_store = gsm_store_new();

	xsmp_server = gsm_xsmp_server_new(client_store);

	/* Now make sure they succeeded. (They'll call
	 * gsm_util_init_error() if they failed.)
	 */
	acquire_name();

	/* Starts gnome compat mode */
	msm_gnome_start();

	manager = gsm_manager_new(client_store, failsafe);

	signal_handler = mdm_signal_handler_new();
	mdm_signal_handler_add_fatal(signal_handler);
	mdm_signal_handler_add(signal_handler, SIGFPE, signal_cb, NULL);
	mdm_signal_handler_add(signal_handler, SIGHUP, signal_cb, NULL);
	mdm_signal_handler_add(signal_handler, SIGUSR1, signal_cb, NULL);
	mdm_signal_handler_add(signal_handler, SIGTERM, signal_cb, manager);
	mdm_signal_handler_add(signal_handler, SIGINT, signal_cb, manager);
	mdm_signal_handler_set_fatal_func(signal_handler, shutdown_cb, manager);

	if (override_autostart_dirs != NULL)
	{
		load_override_apps(manager, override_autostart_dirs);
	}
	else
	{
		load_standard_apps(manager, GSM_DEFAULT_SESSION_KEY);
	}

	gsm_xsmp_server_start(xsmp_server);
	gsm_manager_start(manager);

	gtk_main();

	if (xsmp_server != NULL)
	{
		g_object_unref(xsmp_server);
	}

	if (manager != NULL)
	{
		g_debug("Unreffing manager");
		g_object_unref(manager);
	}

	if (client_store != NULL)
	{
		g_object_unref(client_store);
	}

	msm_gnome_stop();
	mdm_log_shutdown();

	return 0;
}
