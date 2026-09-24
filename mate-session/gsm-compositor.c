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
 *
 */

#include "config.h"

#include <sys/wait.h>

#include <gio/gio.h>
#include <glib.h>

#include "gsm-compositor.h"

static gboolean
_run_loginctl (char **argv)
{
	GError  *error = NULL;
	gint     status;

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
	                   NULL, NULL, NULL, NULL, &status, &error)) {
		g_warning ("GsmCompositor: unable to run loginctl (%s): %s",
		           argv[0] != NULL ? argv[0] : "unknown",
		           error != NULL ? error->message : "unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}

/* Ask logind to terminate the session scope. */
gboolean
gsm_compositor_terminate_session (void)
{
	const gchar *session_id;
	gchar      **argv;
	gboolean     ok;

	session_id = g_getenv ("XDG_SESSION_ID");
	if (session_id == NULL || session_id[0] == '\0') {
		g_debug ("GsmCompositor: XDG_SESSION_ID is not set, "
		         "skipping session scope termination");
		return FALSE;
	}

	if (g_find_program_in_path ("loginctl") == NULL) {
		g_debug ("GsmCompositor: loginctl not found, "
		         "skipping session scope termination");
		return FALSE;
	}

	argv = g_new0 (gchar *, 4);
	argv[0] = "loginctl";
	argv[1] = "terminate-session";
	argv[2] = (gchar *) session_id;

	ok = _run_loginctl (argv);
	g_free (argv);

	if (ok) {
		return TRUE;
	}

	/* Fall back to forcing every process in the session scope to be killed. */
	g_warning ("GsmCompositor: loginctl terminate-session failed, "
	           "trying kill-session --kill-who=all as fallback");

	argv = g_new0 (gchar *, 6);
	argv[0] = "loginctl";
	argv[1] = "kill-session";
	argv[2] = (gchar *) session_id;
	argv[3] = "--kill-who";
	argv[4] = "all";

	ok = _run_loginctl (argv);
	g_free (argv);

	return ok;
}
