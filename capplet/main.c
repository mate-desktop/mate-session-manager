/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * main.c
 * Copyright (C) 1999 Free Software Foundation, Inc.
 * Copyright (C) 2008 Lucas Rocha.
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

#include <unistd.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gsm-properties-dialog.h"

static gboolean show_version = FALSE;

static GOptionEntry options[] = {
	{"version", 0, 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL},
	{NULL, 0, 0, 0, NULL, NULL, NULL}
};

int main(int argc, char* argv[])
{
	GError *error;
	GtkWidget *dialog;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	error = NULL;

	if (!gtk_init_with_args(&argc, &argv, _("- MATE Session Properties"), options, GETTEXT_PACKAGE, &error))
	{
		g_warning("Unable to start: %s", error->message);
		g_error_free(error);
		return 1;
	}

	if (show_version)
	{
		g_print("%s %s\n", argv[0], VERSION);
		return 0;
	}

	dialog = gsm_properties_dialog_new ();
	gtk_widget_show (dialog);

	gtk_main();

	return 0;
}
