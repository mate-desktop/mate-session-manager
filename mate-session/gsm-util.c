/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * gsm-util.c
 * Copyright (C) 2008 Lucas Rocha.
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

#include <config.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>

#include "gsm-util.h"

static gchar *_saved_session_dir = NULL;

gchar **
gsm_get_screen_locker_command (void)
{
        const char *screen_locker_command[] = {
                "mate-screensaver-command --lock",
                "xscreensaver-command -lock",
                NULL
        };
        gchar **screen_locker_argv = NULL;
        gsize   i;

        for (i = 0; screen_locker_command[i] != NULL && screen_locker_argv == NULL; i++) {
                gchar **argv;
                char   *path;

                argv = g_strsplit (screen_locker_command [i], " ", -1);
                path = g_find_program_in_path (argv[0]);
                if (path)
                        screen_locker_argv = g_strdupv (argv);

                g_free (path);
                g_strfreev (argv);
        }

        return screen_locker_argv;
}

char *
gsm_util_find_desktop_file_for_app_name (const char *name,
                                         char      **autostart_dirs)
{
        char     *app_path;
        char    **app_dirs;
        GKeyFile *key_file;
        char     *desktop_file;
        int       i;

        app_path = NULL;

        app_dirs = gsm_util_get_app_dirs ();

        key_file = g_key_file_new ();

        desktop_file = g_strdup_printf ("%s.desktop", name);

        g_debug ("GsmUtil: Looking for file '%s'", desktop_file);

        for (i = 0; app_dirs[i] != NULL; i++) {
                g_debug ("GsmUtil: Looking in '%s'", app_dirs[i]);
        }

        g_key_file_load_from_dirs (key_file,
                                   desktop_file,
                                   (const char **) app_dirs,
                                   &app_path,
                                   G_KEY_FILE_NONE,
                                   NULL);

        if (app_path != NULL) {
                g_debug ("GsmUtil: found in XDG app dirs: '%s'", app_path);
        }

        if (app_path == NULL && autostart_dirs != NULL) {
                g_key_file_load_from_dirs (key_file,
                                           desktop_file,
                                           (const char **) autostart_dirs,
                                           &app_path,
                                           G_KEY_FILE_NONE,
                                           NULL);
                if (app_path != NULL) {
                        g_debug ("GsmUtil: found in autostart dirs: '%s'", app_path);
                }

        }

        /* look for mate vender prefix */
        if (app_path == NULL) {
                g_free (desktop_file);
                desktop_file = g_strdup_printf ("mate-%s.desktop", name);

                g_key_file_load_from_dirs (key_file,
                                           desktop_file,
                                           (const char **) app_dirs,
                                           &app_path,
                                           G_KEY_FILE_NONE,
                                           NULL);
                if (app_path != NULL) {
                        g_debug ("GsmUtil: found in XDG app dirs: '%s'", app_path);
                }
        }

        if (app_path == NULL && autostart_dirs != NULL) {
                g_key_file_load_from_dirs (key_file,
                                           desktop_file,
                                           (const char **) autostart_dirs,
                                           &app_path,
                                           G_KEY_FILE_NONE,
                                           NULL);
                if (app_path != NULL) {
                        g_debug ("GsmUtil: found in autostart dirs: '%s'", app_path);
                }
        }

        g_free (desktop_file);
        g_key_file_free (key_file);

        g_strfreev (app_dirs);

        return app_path;
}

static gboolean
ensure_dir_exists (const char *dir)
{
        if (g_file_test (dir, G_FILE_TEST_IS_DIR))
                return TRUE;

        if (g_mkdir_with_parents (dir, 0755) == 0)
                return TRUE;

        if (errno == EEXIST)
                return g_file_test (dir, G_FILE_TEST_IS_DIR);

        g_warning ("GsmSessionSave: Failed to create directory %s: %s", dir, strerror (errno));

        return FALSE;
}

gchar *
gsm_util_get_empty_tmp_session_dir (void)
{
        char *tmp;
        gboolean exists;

        tmp = g_build_filename (g_get_user_config_dir (),
                                "mate-session",
                                "saved-session.new",
                                NULL);

        exists = ensure_dir_exists (tmp);

        if (G_UNLIKELY (!exists)) {
                g_warning ("GsmSessionSave: could not create directory for saved session: %s", tmp);
                g_free (tmp);
                return NULL;
        } else {
                /* make sure it's empty */
                GDir       *dir;
                const char *filename;

                dir = g_dir_open (tmp, 0, NULL);
                if (dir) {
                        while ((filename = g_dir_read_name (dir))) {
                                gchar *path = g_build_filename (tmp, filename,
                                                               NULL);
                                g_unlink (path);
                                g_free (path);
                        }
                        g_dir_close (dir);
                }
        }

        return tmp;
}

const gchar *
gsm_util_get_saved_session_dir (void)
{
        if (_saved_session_dir == NULL) {
                gboolean exists;

                _saved_session_dir =
                        g_build_filename (g_get_user_config_dir (),
                                          "mate-session",
                                          "saved-session",
                                          NULL);

                exists = ensure_dir_exists (_saved_session_dir);

                if (G_UNLIKELY (!exists)) {
                        static gboolean printed_warning = FALSE;

                        if (!printed_warning) {
                                g_warning ("GsmSessionSave: could not create directory for saved session: %s", _saved_session_dir);
                                printed_warning = TRUE;
                        }

                        _saved_session_dir = NULL;

                        return NULL;
                }
        }

        return _saved_session_dir;
}


char **
gsm_util_get_autostart_dirs ()
{
        GPtrArray          *dirs;
        const char * const *system_config_dirs;
        const char * const *system_data_dirs;
        int                 i;

        dirs = g_ptr_array_new ();

        g_ptr_array_add (dirs,
                         g_build_filename (g_get_user_config_dir (),
                                           "autostart", NULL));

        system_data_dirs = g_get_system_data_dirs ();
        for (i = 0; system_data_dirs[i]; i++) {
                g_ptr_array_add (dirs,
                                 g_build_filename (system_data_dirs[i],
                                                   "mate", "autostart", NULL));
        }

        system_config_dirs = g_get_system_config_dirs ();
        for (i = 0; system_config_dirs[i]; i++) {
                g_ptr_array_add (dirs,
                                 g_build_filename (system_config_dirs[i],
                                                   "autostart", NULL));
        }

        g_ptr_array_add (dirs, NULL);

        return (char **) g_ptr_array_free (dirs, FALSE);
}

char **
gsm_util_get_app_dirs ()
{
        GPtrArray          *dirs;
        const char * const *system_data_dirs;
        int                 i;

        dirs = g_ptr_array_new ();

        g_ptr_array_add (dirs,
			 g_build_filename (g_get_user_data_dir (),
					   "applications",
					   NULL));

        system_data_dirs = g_get_system_data_dirs ();
        for (i = 0; system_data_dirs[i]; i++) {
                g_ptr_array_add (dirs,
                                 g_build_filename (system_data_dirs[i],
                                                   "applications",
                                                   NULL));
        }

        g_ptr_array_add (dirs, NULL);

        return (char **) g_ptr_array_free (dirs, FALSE);
}

char **
gsm_util_get_desktop_dirs ()
{
	char **apps;
	char **autostart;
	char **result;
	int    size;
	int    i;

	apps = gsm_util_get_app_dirs ();
	autostart = gsm_util_get_autostart_dirs ();

	size = 0;
	for (i = 0; apps[i] != NULL; i++) { size++; }
	for (i = 0; autostart[i] != NULL; i++) { size++; }
	size += 2; /* saved session + last NULL */

	result = g_new (char *, size + 1);

	for (i = 0; apps[i] != NULL; i++) {
		result[i] = apps[i];
	}
	g_free (apps);
	size = i;

	for (i = 0; autostart[i] != NULL; i++) {
		result[size + i] = autostart[i];
	}
	g_free (autostart);
	size = size + i;

	result[size] = g_strdup (gsm_util_get_saved_session_dir ());
	result[size + 1] = NULL;

	return result;
}

gboolean
gsm_util_text_is_blank (const char *str)
{
        if (str == NULL) {
                return TRUE;
        }

        while (*str) {
                if (!isspace(*str)) {
                        return FALSE;
                }

                str++;
        }

        return TRUE;
}

/**
 * gsm_util_init_error:
 * @fatal: whether or not the error is fatal to the login session
 * @format: printf-style error message format
 * @...: error message args
 *
 * Displays the error message to the user. If @fatal is %TRUE, gsm
 * will exit after displaying the message.
 *
 * This should be called for major errors that occur before the
 * session is up and running. (Notably, it positions the dialog box
 * itself, since no window manager will be running yet.)
 **/
void
gsm_util_init_error (gboolean    fatal,
                     const char *format, ...)
{
        GtkWidget *dialog;
        char      *msg;
        va_list    args;

        va_start (args, format);
        msg = g_strdup_vprintf (format, args);
        va_end (args);

        /* If option parsing failed, Gtk won't have been initialized... */
        if (!gdk_display_get_default ()) {
                if (!gtk_init_check (NULL, NULL)) {
                        /* Oh well, no X for you! */
                        g_printerr (_("Unable to start login session (and unable to connect to the X server)"));
                        g_printerr ("%s", msg);
                        exit (1);
                }
        }

        dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE, "%s", msg);

        g_free (msg);

        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
        gtk_dialog_run (GTK_DIALOG (dialog));

        gtk_widget_destroy (dialog);

        if (fatal) {
                gtk_main_quit ();
        }
}

/**
 * gsm_util_generate_startup_id:
 *
 * Generates a new SM client ID.
 *
 * Return value: an SM client ID.
 **/
char *
gsm_util_generate_startup_id (void)
{
        static int     sequence = -1;
        static guint   rand1 = 0;
        static guint   rand2 = 0;
        static pid_t   pid = 0;
        struct timeval tv;

        /* The XSMP spec defines the ID as:
         *
         * Version: "1"
         * Address type and address:
         *   "1" + an IPv4 address as 8 hex digits
         *   "2" + a DECNET address as 12 hex digits
         *   "6" + an IPv6 address as 32 hex digits
         * Time stamp: milliseconds since UNIX epoch as 13 decimal digits
         * Process-ID type and process-ID:
         *   "1" + POSIX PID as 10 decimal digits
         * Sequence number as 4 decimal digits
         *
         * XSMP client IDs are supposed to be globally unique: if
         * SmsGenerateClientID() is unable to determine a network
         * address for the machine, it gives up and returns %NULL.
         * MATE and KDE have traditionally used a fourth address
         * format in this case:
         *   "0" + 16 random hex digits
         *
         * We don't even bother trying SmsGenerateClientID(), since the
         * user's IP address is probably "192.168.1.*" anyway, so a random
         * number is actually more likely to be globally unique.
         */

        if (!rand1) {
                rand1 = g_random_int ();
                rand2 = g_random_int ();
                pid = getpid ();
        }

        sequence = (sequence + 1) % 10000;
        gettimeofday (&tv, NULL);
        return g_strdup_printf ("10%.04x%.04x%.10lu%.3u%.10lu%.4d",
                                rand1,
                                rand2,
                                (unsigned long) tv.tv_sec,
                                (unsigned) tv.tv_usec,
                                (unsigned long) pid,
                                sequence);
}

static gboolean
gsm_util_update_activation_environment (const char  *variable,
                                        const char  *value,
                                        GError     **error)
{
        GDBusConnection *connection;
        gboolean         environment_updated;
        GVariantBuilder  builder;
        GVariant        *reply;
        GError          *bus_error = NULL;

        environment_updated = FALSE;
        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

        if (connection == NULL) {
                return FALSE;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));
        g_variant_builder_add (&builder, "{ss}", variable, value);

        reply = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "UpdateActivationEnvironment",
                                             g_variant_new ("(@a{ss})",
                                                            g_variant_builder_end (&builder)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1, NULL, &bus_error);

        if (bus_error != NULL) {
                g_propagate_error (error, bus_error);
        } else {
                environment_updated = TRUE;
                g_variant_unref (reply);
        }

        g_clear_object (&connection);

        return environment_updated;
}

gboolean
gsm_util_export_activation_environment (GError     **error)
{
        GDBusConnection *connection;
        gboolean         environment_updated = FALSE;
        char           **entry_names;
        int              i = 0;
        GVariantBuilder  builder;
        GRegex          *name_regex, *value_regex;
        GVariant        *reply;
        GError          *bus_error = NULL;

        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

        if (connection == NULL) {
                return FALSE;
        }

        name_regex = g_regex_new ("^[a-zA-Z_][a-zA-Z0-9_]*$", G_REGEX_OPTIMIZE, 0, error);

        if (name_regex == NULL) {
                return FALSE;
        }

        value_regex = g_regex_new ("^([[:blank:]]|[^[:cntrl:]])*$", G_REGEX_OPTIMIZE, 0, error);

        if (value_regex == NULL) {
                return FALSE;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{ss}"));
        for (entry_names = g_listenv (); entry_names[i] != NULL; i++) {
                const char *entry_name = entry_names[i];
                const char *entry_value = g_getenv (entry_name);

                if (!g_utf8_validate (entry_name, -1, NULL))
                    continue;

                if (!g_regex_match (name_regex, entry_name, 0, NULL))
                    continue;

                if (!g_utf8_validate (entry_value, -1, NULL))
                    continue;

                if (!g_regex_match (value_regex, entry_value, 0, NULL))
                    continue;

                g_variant_builder_add (&builder, "{ss}", entry_name, entry_value);
        }
        g_regex_unref (name_regex);
        g_regex_unref (value_regex);

        g_strfreev (entry_names);

        reply = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.DBus",
                                             "/org/freedesktop/DBus",
                                             "org.freedesktop.DBus",
                                             "UpdateActivationEnvironment",
                                             g_variant_new ("(@a{ss})",
                                                            g_variant_builder_end (&builder)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1, NULL, &bus_error);

        if (bus_error != NULL) {
                g_propagate_error (error, bus_error);
        } else {
                environment_updated = TRUE;
                g_variant_unref (reply);
        }

        g_clear_object (&connection);

        return environment_updated;
}

#ifdef HAVE_SYSTEMD
gboolean
gsm_util_export_user_environment (GError     **error)
{
        GDBusConnection *connection;
        gboolean         environment_updated = FALSE;
        char           **entries;
        int              i = 0;
        GVariantBuilder  builder;
        GRegex          *regex;
        GVariant        *reply;
        GError          *bus_error = NULL;

        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

        if (connection == NULL) {
                return FALSE;
        }

        regex = g_regex_new ("^[a-zA-Z_][a-zA-Z0-9_]*=([[:blank:]]|[^[:cntrl:]])*$", G_REGEX_OPTIMIZE, 0, error);

        if (regex == NULL) {
                return FALSE;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
        for (entries = g_get_environ (); entries[i] != NULL; i++) {
                const char *entry = entries[i];

                if (!g_utf8_validate (entry, -1, NULL))
                    continue;

                if (!g_regex_match (regex, entry, 0, NULL))
                    continue;

                g_variant_builder_add (&builder, "s", entry);
        }
        g_regex_unref (regex);

        g_strfreev (entries);

        reply = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "SetEnvironment",
                                             g_variant_new ("(@as)",
                                                            g_variant_builder_end (&builder)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1, NULL, &bus_error);

        if (bus_error != NULL) {
                g_propagate_error (error, bus_error);
        } else {
                environment_updated = TRUE;
                g_variant_unref (reply);
        }

        g_clear_object (&connection);

        return environment_updated;
}

static gboolean
gsm_util_update_user_environment (const char  *variable,
                                  const char  *value,
                                  GError     **error)
{
        GDBusConnection *connection;
        gboolean         environment_updated;
        char            *entry;
        GVariantBuilder  builder;
        GVariant        *reply;
        GError          *bus_error = NULL;

        environment_updated = FALSE;
        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);

        if (connection == NULL) {
                return FALSE;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
        entry = g_strdup_printf ("%s=%s", variable, value);
        g_variant_builder_add (&builder, "s", entry);
        g_free (entry);

        reply = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "SetEnvironment",
                                             g_variant_new ("(@as)",
                                                            g_variant_builder_end (&builder)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1, NULL, &bus_error);

        if (bus_error != NULL) {
                g_propagate_error (error, bus_error);
        } else {
                environment_updated = TRUE;
                g_variant_unref (reply);
        }

        g_clear_object (&connection);

        return environment_updated;
}
#endif

void
gsm_util_setenv (const char *variable,
                 const char *value)
{
        GError *error = NULL;

        g_setenv (variable, value, TRUE);

        /* If this fails it isn't fatal, it means some things like session
         * management and keyring won't work in activated clients.
         */
        if (!gsm_util_update_activation_environment (variable, value, &error)) {
                g_warning ("Could not make bus activated clients aware of %s=%s environment variable: %s", variable, value, error->message);
                g_clear_error (&error);
        }

#ifdef HAVE_SYSTEMD
        /* If this fails, the system user session won't get the updated environment
         */
        if (!gsm_util_update_user_environment (variable, value, &error)) {
                g_debug ("Could not make systemd aware of %s=%s environment variable: %s", variable, value, error->message);
                g_clear_error (&error);
        }
#endif
}

GtkWidget*
gsm_util_dialog_add_button (GtkDialog   *dialog,
                            const gchar *button_text,
                            const gchar *icon_name,
                            gint         response_id)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic (button_text);
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON));

	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	gtk_style_context_add_class (gtk_widget_get_style_context (button), "text-button");
	gtk_widget_set_can_default (button, TRUE);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);

	return button;
}
