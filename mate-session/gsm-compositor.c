/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "gsm-compositor.h"
#include "gsm-util.h"

#define GSM_SCHEMA             "org.mate.session"
#define GSM_COMPOSITOR_KEY     "compositor"

/* How long to wait for the compositor's Wayland socket to appear. */
#define GSM_COMPOSITOR_TIMEOUT     10 /* seconds */
#define GSM_COMPOSITOR_WAIT_US     100000 /* 100 ms */

/* How long to wait for the compositor to exit after SIGTERM. */
#define GSM_COMPOSITOR_EXIT_TIMEOUT 5 /* seconds */

typedef struct {
        const gchar *name;             /* value used in GSettings / MATE_COMPOSITOR */
        const gchar *binary;           /* executable to launch */
        const gchar *config_subdir;    /* directory under ~/.config/mate for the user config; NULL = none */
        const gchar *config_template;  /* default config installed under GTKBUILDER_DIR; NULL = none */
        gboolean     needs_config_arg; /* whether "-c <config>" is passed (user copy or system template) */
} GsmCompositorInfo;

/* Keep this table sorted for readability. To add support for another
 * compositor, add an entry here (e.g. labwc) and document its GSettings
 * value. The session startup and shutdown logic is compositor agnostic. */
static const GsmCompositorInfo compositor_infos[] = {
        { "wayfire", "wayfire", "wayfire", "wayfire.ini", TRUE },
        { NULL }
};

static GPid     compositor_pid     = 0;
static guint    compositor_watch_id = 0;
static gboolean compositor_started  = FALSE;
static gboolean compositor_stopping = FALSE;
static gchar   *xwayland_display   = NULL;

static gchar *
get_compositor_settings_name (void)
{
        GSettingsSchemaSource *source;
        GSettingsSchema       *schema;
        GSettings             *settings;
        gchar                 *name = NULL;

        source = g_settings_schema_source_get_default ();
        if (source == NULL) {
                return NULL;
        }

        schema = g_settings_schema_source_lookup (source, GSM_SCHEMA, TRUE);
        if (schema == NULL || !g_settings_schema_has_key (schema, GSM_COMPOSITOR_KEY)) {
                if (schema != NULL) {
                        g_settings_schema_unref (schema);
                }
                return NULL;
        }

        settings = g_settings_new (GSM_SCHEMA);
        name = g_settings_get_string (settings, GSM_COMPOSITOR_KEY);
        g_object_unref (settings);
        g_settings_schema_unref (schema);

        return name;
}

static const GsmCompositorInfo *
get_compositor_info (void)
{
        const GsmCompositorInfo *info;
        const gchar             *name;
        gchar                   *settings_name = NULL;

        name = g_getenv ("MATE_COMPOSITOR");
        if (name == NULL || name[0] == '\0') {
                settings_name = get_compositor_settings_name ();
                name = settings_name;
        }

        if (name != NULL && name[0] != '\0') {
                for (info = compositor_infos; info->name != NULL; info++) {
                        if (g_strcmp0 (info->name, name) == 0) {
                                g_free (settings_name);
                                return info;
                        }
                }
        }

        g_warning ("GsmCompositor: unknown compositor '%s', using the default one",
                   name != NULL ? name : "(none)");
        g_free (settings_name);

        return &compositor_infos[0];
}

static gchar *
get_config_dir (void)
{
        const gchar *config_home;

        config_home = g_getenv ("XDG_CONFIG_HOME");
        if (config_home != NULL && config_home[0] != '\0') {
                return g_build_filename (config_home, "mate", NULL);
        }

        return g_build_filename (g_get_home_dir (), ".config", "mate", NULL);
}

static gchar *
strip_quotes (const gchar *value)
{
        gchar *v;
        gsize  len;

        v = g_strstrip (g_strdup (value));
        len = strlen (v);

        if (len >= 2
            && (v[0] == '"' || v[0] == '\'')
            && v[len - 1] == v[0]) {
                memmove (v, v + 1, len - 2);
                v[len - 2] = '\0';
        }

        return v;
}

/* If the user did not set an explicit keyboard layout, export the system
 * layout from /etc/vconsole.conf so that libxkbcommon applies it when the
 * compositor configuration does not specify xkb_layout. */
static void
set_default_xkb_layout (void)
{
        gchar   *contents;
        gchar  **lines;
        gchar   *layout = NULL;
        gchar   *variant = NULL;
        gchar   *keymap = NULL;
        gint     i;

        if (g_getenv ("XKB_DEFAULT_LAYOUT") != NULL) {
                return;
        }

        if (!g_file_get_contents ("/etc/vconsole.conf", &contents, NULL, NULL)) {
                return;
        }

        lines = g_strsplit (contents, "\n", -1);
        for (i = 0; lines[i] != NULL; i++) {
                gchar *line = g_strstrip (g_strdup (lines[i]));

                if (g_str_has_prefix (line, "KEYMAP=")) {
                        keymap = strip_quotes (line + strlen ("KEYMAP="));
                } else if (g_str_has_prefix (line, "XKBLAYOUT=")) {
                        layout = strip_quotes (line + strlen ("XKBLAYOUT="));
                } else if (g_str_has_prefix (line, "XKBVARIANT=")) {
                        variant = strip_quotes (line + strlen ("XKBVARIANT="));
                }

                g_free (line);
        }
        g_strfreev (lines);

        if (layout != NULL && layout[0] != '\0') {
                gsm_util_setenv ("XKB_DEFAULT_LAYOUT", layout);
                if (variant != NULL && variant[0] != '\0') {
                        gsm_util_setenv ("XKB_DEFAULT_VARIANT", variant);
                }
        } else if (keymap != NULL && keymap[0] != '\0') {
                gsm_util_setenv ("XKB_DEFAULT_LAYOUT", keymap);
        }

        g_free (layout);
        g_free (variant);
        g_free (keymap);
        g_free (contents);
}

/* Return the compositor configuration file to use, creating a private copy
 * of the default configuration on the first login so the user can freely
 * customize it afterwards. If a user configuration already exists it is
 * reused as-is. Falls back to the system template when a copy cannot be
 * made, and returns NULL if no configuration is available at all. */
static gchar *
get_compositor_config_path (const GsmCompositorInfo *info)
{
        gchar *config_dir;
        gchar *config_path;
        gchar *template_path;

        config_dir = g_build_filename (get_config_dir (),
                                       info->config_subdir, NULL);
        config_path = g_build_filename (config_dir, info->config_template, NULL);

        if (g_mkdir_with_parents (config_dir, 0755) != 0) {
                g_debug ("GsmCompositor: cannot create %s: %s",
                         config_dir, g_strerror (errno));
        }
        g_free (config_dir);

        if (g_file_test (config_path, G_FILE_TEST_EXISTS)) {
                return config_path;
        }

        template_path = g_build_filename (GTKBUILDER_DIR, info->config_template, NULL);
        if (g_file_test (template_path, G_FILE_TEST_EXISTS)) {
                if (g_file_copy (g_file_new_for_path (template_path),
                                 g_file_new_for_path (config_path),
                                 G_FILE_COPY_NONE, NULL, NULL, NULL, NULL)) {
                        g_free (template_path);
                        return config_path;
                }

                g_warning ("GsmCompositor: unable to install %s, "
                           "using the system template %s instead",
                           config_path, template_path);
                g_free (config_path);
                return template_path;
        }

        g_warning ("GsmCompositor: no compositor configuration found "
                   "(looked in %s and %s)",
                   config_path, template_path);
        g_free (config_path);
        g_free (template_path);

        return NULL;
}

static gchar *
find_runtime_dir (void)
{
        const gchar *env;
        gchar       *path;

        env = g_getenv ("XDG_RUNTIME_DIR");
        if (env != NULL && env[0] != '\0') {
                return g_strdup (env);
        }

        path = g_strdup_printf ("/run/user/%u", (guint) getuid ());
        if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
                return path;
        }
        g_free (path);

        path = g_strdup_printf ("/var/run/user/%u", (guint) getuid ());
        if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
                return path;
        }
        g_free (path);

        return g_strdup_printf ("/tmp/%s-runtime",
                                g_get_user_name () != NULL
                                        ? g_get_user_name () : "mate");
}

static gboolean
ensure_runtime_dir (GError **error)
{
        gchar   *runtime_dir;
        gboolean ret = TRUE;

        runtime_dir = find_runtime_dir ();
        gsm_util_setenv ("XDG_RUNTIME_DIR", runtime_dir);

        if (!g_file_test (runtime_dir, G_FILE_TEST_IS_DIR)) {
                if (g_mkdir_with_parents (runtime_dir, 0700) != 0) {
                        g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                     "Unable to create runtime directory %s: %s",
                                     runtime_dir, g_strerror (errno));
                        ret = FALSE;
                }
        } else {
                chmod (runtime_dir, 0700);
        }

        g_free (runtime_dir);

        return ret;
}

static GList *
list_sockets (const gchar *dir)
{
        GList       *sockets = NULL;
        GDir        *gd;
        const gchar *name;

        gd = g_dir_open (dir, 0, NULL);
        if (gd == NULL) {
                return NULL;
        }

        while ((name = g_dir_read_name (gd)) != NULL) {
                gchar      *path;
                struct stat st;

                if (!g_str_has_prefix (name, "wayland-")) {
                        continue;
                }

                path = g_build_filename (dir, name, NULL);
                if (g_stat (path, &st) == 0 && S_ISSOCK (st.st_mode)) {
                        sockets = g_list_prepend (sockets, g_strdup (path));
                }
                g_free (path);
        }

        g_dir_close (gd);

        return sockets;
}

static gboolean
list_contains (GList      *list,
               const gchar *path)
{
        GList *l;

        for (l = list; l != NULL; l = l->next) {
                if (g_strcmp0 (l->data, path) == 0) {
                        return TRUE;
                }
        }

        return FALSE;
}

/* Wait until the compositor creates a new Wayland socket that did not exist
 * before it was launched (the compositor picks the first free "wayland-N"
 * name itself, so we cannot assume a specific one). */
static gchar *
wait_for_new_socket (const gchar *runtime_dir,
                     GList       *existing)
{
        gint i;

        for (i = 0; i < GSM_COMPOSITOR_TIMEOUT * (1000000 / GSM_COMPOSITOR_WAIT_US); i++) {
                GList  *current;
                GList  *l;
                gchar  *found = NULL;

                current = list_sockets (runtime_dir);
                for (l = current; l != NULL; l = l->next) {
                        if (!list_contains (existing, l->data)) {
                                found = g_strdup (l->data);
                                break;
                        }
                }
                g_list_free_full (current, g_free);

                if (found != NULL) {
                        return found;
                }

                g_usleep (GSM_COMPOSITOR_WAIT_US);
        }

        return NULL;
}

#define X11_SOCKET_DIR "/tmp/.X11-unix"

static GList *
list_x_displays (void)
{
        GList       *displays = NULL;
        GDir        *gd;
        const gchar *name;

        gd = g_dir_open (X11_SOCKET_DIR, 0, NULL);
        if (gd == NULL) {
                return NULL;
        }

        while ((name = g_dir_read_name (gd)) != NULL) {
                gchar      *path;
                struct stat st;

                if (!g_str_has_prefix (name, "X")) {
                        continue;
                }

                path = g_build_filename (X11_SOCKET_DIR, name, NULL);
                if (g_stat (path, &st) == 0 && S_ISSOCK (st.st_mode)) {
                        displays = g_list_prepend (displays, path);
                } else {
                        g_free (path);
                }
        }

        g_dir_close (gd);

        return displays;
}

/* Find the Xwayland display after the compositor has started.  Xwayland may
 * create a brand-new X11 socket or reuse one that already existed when we
 * snapshotted the directory (e.g. the previous session's :0).  Because of
 * this reuse, we cannot rely on "new socket" detection alone.  Instead we
 * poll for any X11 socket to appear; the first NEW one we find is Xwayland.
 * Returns the display number as a string (e.g. "1") or NULL on timeout. */
static gchar *
wait_for_new_xdisplay (GList *existing)
{
        gint i;

        for (i = 0; i < GSM_COMPOSITOR_TIMEOUT * (1000000 / GSM_COMPOSITOR_WAIT_US); i++) {
                GList  *current;
                GList  *l;
                gchar  *found = NULL;

                current = list_x_displays ();
                for (l = current; l != NULL; l = l->next) {
                        if (!list_contains (existing, l->data)) {
                                const gchar *basename;

                                basename = g_path_get_basename (l->data);
                                found = g_strdup (basename + 1); /* strip leading 'X' */
                                break;
                        }
                }
                g_list_free_full (current, g_free);

                if (found != NULL) {
                        return found;
                }

                g_usleep (GSM_COMPOSITOR_WAIT_US);
        }

        return NULL;
}

gboolean
gsm_compositor_start (GError **error)
{
        const GsmCompositorInfo *info;
        gchar                   *config_path = NULL;
        gchar                   *runtime_dir;
        gchar                   *socket_path;
        gchar                   *socket_name;
        gchar                  **argv;
        GList                   *existing;
        GList                   *existing_x;
        GError                  *spawn_error = NULL;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        info = get_compositor_info ();

        /* Make the environment look like a proper Wayland session. */
        gsm_util_setenv ("XDG_SESSION_TYPE", "wayland");
        gsm_util_setenv ("XDG_CURRENT_DESKTOP", "MATE");
        set_default_xkb_layout ();

        if (!ensure_runtime_dir (error)) {
                return FALSE;
        }

        if (g_find_program_in_path (info->binary) == NULL) {
                g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT,
                             "The Wayland compositor '%s' is not installed",
                             info->binary);
                return FALSE;
        }

        if (info->needs_config_arg) {
                config_path = get_compositor_config_path (info);
                if (config_path != NULL) {
                        argv = g_new0 (gchar *, 4);
                        argv[0] = g_strdup (info->binary);
                        argv[1] = g_strdup ("-c");
                        argv[2] = g_strdup (config_path);
                } else {
                        argv = g_new0 (gchar *, 2);
                        argv[0] = g_strdup (info->binary);
                }
        } else {
                argv = g_new0 (gchar *, 2);
                argv[0] = g_strdup (info->binary);
        }

        runtime_dir = g_strdup (g_getenv ("XDG_RUNTIME_DIR"));
        existing = list_sockets (runtime_dir);
        existing_x = list_x_displays ();

        g_debug ("GsmCompositor: starting %s", info->binary);

        if (!g_spawn_async (NULL, argv, NULL,
                            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                            NULL, NULL, &compositor_pid,
                            &spawn_error)) {
                g_propagate_error (error, spawn_error);
                g_strfreev (argv);
                g_free (config_path);
                g_list_free_full (existing, g_free);
                g_list_free_full (existing_x, g_free);
                g_free (runtime_dir);
                return FALSE;
        }

        g_strfreev (argv);
        g_free (config_path);

        compositor_started = TRUE;

        socket_path = wait_for_new_socket (runtime_dir, existing);
        g_list_free_full (existing, g_free);

        if (socket_path == NULL) {
                g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                             "The Wayland compositor '%s' failed to start "
                             "(no Wayland socket appeared within %d seconds)",
                             info->binary, GSM_COMPOSITOR_TIMEOUT);
                g_free (socket_path);
                g_free (runtime_dir);
                gsm_compositor_stop ();
                return FALSE;
        }

        socket_name = g_path_get_basename (socket_path);
        g_debug ("GsmCompositor: compositor '%s' running on %s",
                 info->binary, socket_name);
        gsm_util_setenv ("WAYLAND_DISPLAY", socket_name);

        g_free (socket_name);
        g_free (socket_path);
        g_free (runtime_dir);

        /* Detect the Xwayland display for X11 clients.  This is best-effort:
         * compositors that disable Xwayland will simply not create a display. */
        {
                gchar *xdisplay;

                xdisplay = wait_for_new_xdisplay (existing_x);

                if (xdisplay != NULL) {
                        gchar *display_name;

                        display_name = g_strdup_printf (":%s", xdisplay);
                        g_debug ("GsmCompositor: Xwayland running on %s", display_name);
                        gsm_util_setenv ("DISPLAY", display_name);

                        g_free (xwayland_display);
                        xwayland_display = display_name;
                        g_free (xdisplay);
                } else {
                        /* Xwayland socket not detected; assume the first
                         * display.  Without this, GDK will overwrite DISPLAY
                         * with the Wayland socket name during gtk_init. */
                        g_debug ("GsmCompositor: no Xwayland display detected "
                                 "(assuming :0)");
                        g_free (xwayland_display);
                        xwayland_display = g_strdup (":0");
                        gsm_util_setenv ("DISPLAY", ":0");
                }
        }

        g_list_free_full (existing_x, g_free);

        return TRUE;
}

static void
compositor_exited (GPid     pid,
                   gint     status,
                   gpointer data)
{
        GsmManager *manager = data;
        gboolean    running = FALSE;

        g_spawn_close_pid (pid);

        compositor_pid = 0;
        compositor_started = FALSE;
        compositor_watch_id = 0;

        if (compositor_stopping) {
                return;
        }

        g_warning ("GsmCompositor: the compositor exited unexpectedly");

        if (manager != NULL
            && gsm_manager_is_session_running (manager, &running, NULL)
            && running) {
                g_debug ("GsmCompositor: ending the session because the compositor died");
                gsm_manager_logout (manager, GSM_MANAGER_LOGOUT_MODE_FORCE, NULL);
        }
}

void
gsm_compositor_watch (GsmManager *manager)
{
        if (!compositor_started || compositor_pid <= 0) {
                return;
        }

        compositor_watch_id = g_child_watch_add (compositor_pid,
                                                 compositor_exited,
                                                 manager);
}

void
gsm_compositor_stop (void)
{
        gint64 deadline;
        gint   status;

        if (!compositor_started || compositor_pid <= 0) {
                return;
        }

        compositor_stopping = TRUE;

        if (compositor_watch_id > 0) {
                g_source_remove (compositor_watch_id);
                compositor_watch_id = 0;
        }

        g_debug ("GsmCompositor: terminating the compositor");
        kill (compositor_pid, SIGTERM);

        deadline = g_get_monotonic_time () + GSM_COMPOSITOR_EXIT_TIMEOUT * G_TIME_SPAN_SECOND;
        while (waitpid (compositor_pid, &status, WNOHANG) == 0) {
                if (g_get_monotonic_time () > deadline) {
                        g_warning ("GsmCompositor: compositor did not exit, sending SIGKILL");
                        kill (compositor_pid, SIGKILL);
                        waitpid (compositor_pid, &status, 0);
                        break;
                }
                g_usleep (50000);
        }

        compositor_pid = 0;
        compositor_started = FALSE;
}

const gchar *
gsm_compositor_get_display (void)
{
        return xwayland_display;
}
