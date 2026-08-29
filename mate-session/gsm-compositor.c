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

/* Ask logind to terminate the session scope. */
gboolean
gsm_compositor_terminate_session (void)
{
	const gchar *session_id;
	gchar      **argv;
	gchar       *loginctl;
	GError      *error = NULL;
	gint         status;

	session_id = g_getenv ("XDG_SESSION_ID");
	if (session_id == NULL || session_id[0] == '\0') {
		return FALSE;
	}

	loginctl = g_find_program_in_path ("loginctl");
	if (loginctl == NULL) {
		return FALSE;
	}
	g_free (loginctl);

	argv = g_new0 (gchar *, 4);
	argv[0] = "loginctl";
	argv[1] = "terminate-session";
	argv[2] = (gchar *) session_id;

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
	                   NULL, NULL, NULL, NULL, &status, &error)) {
		g_warning ("GsmCompositor: unable to terminate the session: %s",
		           error != NULL ? error->message : "unknown error");
		g_clear_error (&error);
		g_free (argv);
		return FALSE;
	}

	g_free (argv);

	return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}
