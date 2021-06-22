/* -*- mode:c; c-basic-offset: 8; indent-tabs-mode: nil; -*- */
/* Tool to set the property _GNOME_SESSION_ACCELERATED on the root window */
/*
 * Copyright (C) 2011 Red Hat, Inc.
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:
 *   Colin Walters <walters@verbum.org>
 */

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <sys/wait.h>

#include "mate-session-check-accelerated-common.h"

/* Wait up to this long for a running check to finish */
#define PROPERTY_CHANGE_TIMEOUT 5000

/* Values used for the _GNOME_SESSION_ACCELERATED root window property */
#define NO_ACCEL            0
#define HAVE_ACCEL          1
#define ACCEL_CHECK_RUNNING 2

static Atom is_accelerated_atom;
static Atom is_software_rendering_atom;
static Atom renderer_atom;
static gboolean property_changed;

static gboolean
on_property_notify_timeout (gpointer data)
{
        gtk_main_quit ();
        return FALSE;
}

static GdkFilterReturn
property_notify_filter (GdkXEvent *xevent,
                        GdkEvent  *event,
                        gpointer   data)
{
        XPropertyEvent *ev = xevent;

        if (ev->type == PropertyNotify && ev->atom == is_accelerated_atom) {
                property_changed = TRUE;
                gtk_main_quit ();
        }

        return GDK_FILTER_CONTINUE;
}

static gboolean
wait_for_property_notify (void)
{
        GdkDisplay *display;
        GdkScreen *screen;
        GdkWindow *root;
        Window rootwin;

        property_changed = FALSE;

        display = gdk_display_get_default ();
        screen = gdk_display_get_default_screen (display);
        root = gdk_screen_get_root_window (screen);
        rootwin = gdk_x11_window_get_xid (root);

        XSelectInput (GDK_DISPLAY_XDISPLAY (display), rootwin, PropertyChangeMask);
        gdk_window_add_filter (root, property_notify_filter, NULL);
        g_timeout_add (PROPERTY_CHANGE_TIMEOUT, on_property_notify_timeout, NULL);

        gtk_main ();

        return property_changed;
}

static char *
get_gtk_gles_renderer (void)
{
        GtkWidget *win;
        GdkGLContext *context;
        char *renderer = NULL;

        win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_widget_realize (win);
        context = gdk_window_create_gl_context (gtk_widget_get_window (win), NULL);
        if (!context)
                return NULL;
        gdk_gl_context_make_current (context);
        renderer = g_strdup ((char *) glGetString (GL_RENDERER));
        gdk_gl_context_clear_current ();
        g_object_unref (context);

        return renderer;
}

static gboolean
is_discrete_gpu_check (void)
{
	const char *dri_prime;

	dri_prime = g_getenv ("DRI_PRIME");
	if (!dri_prime)
		return FALSE;
	if (*dri_prime != '1')
		return FALSE;
	return TRUE;
}

int
main (int argc, char **argv)
{
        GdkDisplay *display = NULL;
        int estatus;
        char *gl_helper_argv[] = { LIBEXECDIR "/mate-session-check-accelerated-gl-helper", "--print-renderer", NULL };
#ifdef HAVE_GLESV2
        char *gles_helper_argv[] = { LIBEXECDIR "/mate-session-check-accelerated-gles-helper", "--print-renderer", NULL };
        GError *gles_error = NULL;
        char *gles_renderer_string = NULL;
#endif
        char *renderer_string = NULL;
        char *gl_renderer_string = NULL;
        gboolean gl_software_rendering = FALSE, gles_software_rendering = FALSE;
        Window rootwin;
        glong is_accelerated, is_software_rendering;
        GError *gl_error = NULL;

        gtk_init (NULL, NULL);

        /* mate-session-check-accelerated gets run before X is started in the wayland
         * case, and it currently requires X. Until we have that working, just always
         * assume wayland will work.
         * Also make sure that we don't read cached information about the first GPU
         * when requesting information about the second.
         */
        if (is_discrete_gpu_check () || g_strcmp0 (g_getenv ("XDG_SESSION_TYPE"), "x11") != 0) {
                renderer_string = get_gtk_gles_renderer ();
                if (renderer_string) {
                        g_print ("%s", renderer_string);
                        return 0;
                }
                return 1;
        }

        display = gdk_display_get_default ();
        /* when running on X11 with a nested wayland GDK will default to wayland
         * so looking for X11 atoms will not work (and crash).
         */
        if (!GDK_IS_X11_DISPLAY (display)) {
                g_printerr ("mate-session-check-accelerated: no X11 display found\n");
                return 1;
        }

        rootwin = gdk_x11_get_default_root_xwindow ();

        is_accelerated_atom = gdk_x11_get_xatom_by_name_for_display (display, "_GNOME_SESSION_ACCELERATED");
        is_software_rendering_atom = gdk_x11_get_xatom_by_name_for_display (display, "_GNOME_IS_SOFTWARE_RENDERING");
        renderer_atom = gdk_x11_get_xatom_by_name_for_display (display, "_GNOME_SESSION_RENDERER");

        {
                Atom type;
                gint format;
                gulong nitems;
                gulong bytes_after;
                guchar *data;

 read:
                gdk_x11_display_error_trap_push (display);
                XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), rootwin,
                                    is_accelerated_atom,
                                    0, G_MAXLONG, False, XA_CARDINAL, &type, &format, &nitems,
                                    &bytes_after, &data);
                gdk_x11_display_error_trap_pop_ignored (display);

                if (type == XA_CARDINAL) {
                        glong *is_accelerated_ptr = (glong*) data;

                        if (*is_accelerated_ptr == ACCEL_CHECK_RUNNING) {
                                /* Test in progress, wait */
                                if (wait_for_property_notify ())
                                        goto read;
                                /* else fall through and do the check ourselves */

                        } else {
                                gdk_x11_display_error_trap_push (display);
                                XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), rootwin,
                                                    renderer_atom,
                                                    0, G_MAXLONG, False, XA_STRING, &type, &format, &nitems,
                                                    &bytes_after, &data);
                                gdk_x11_display_error_trap_pop_ignored (display);

                                if (type == XA_STRING) {
                                        g_print ("%s", data);
                                }

                                return (*is_accelerated_ptr == 0 ? 1 : 0);
                        }
                }
        }

        /* We don't have the property or it's the wrong type.
         * Try to compute it now.
         */

        /* First indicate that a test is in progress */
        is_accelerated = ACCEL_CHECK_RUNNING;
        is_software_rendering = FALSE;
        estatus = 1;

        XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                         rootwin,
                         is_accelerated_atom,
                         XA_CARDINAL, 32, PropModeReplace, (guchar *) &is_accelerated, 1);

        gdk_display_sync (display);

        /* First, try the GL helper */
        if (g_spawn_sync (NULL, (char **) gl_helper_argv, NULL, 0,
                           NULL, NULL, &gl_renderer_string, NULL, &estatus, &gl_error)) {
                is_accelerated = (WEXITSTATUS(estatus) == HELPER_ACCEL);
                gl_software_rendering = (WEXITSTATUS(estatus) == HELPER_SOFTWARE_RENDERING);
                if (is_accelerated) {
                        renderer_string = gl_renderer_string;
                        goto finish;
                }

                g_printerr ("mate-session-check-accelerated: GL Helper exited with code %d\n", estatus);
        }

#ifdef HAVE_GLESV2
        /* Then, try the GLES helper */
        if (g_spawn_sync (NULL, (char **) gles_helper_argv, NULL, 0,
                           NULL, NULL, &gles_renderer_string, NULL, &estatus, &gles_error)) {
                is_accelerated = (WEXITSTATUS(estatus) == HELPER_ACCEL);
                gles_software_rendering = (WEXITSTATUS(estatus) == HELPER_SOFTWARE_RENDERING);
                if (is_accelerated) {
                        renderer_string = gles_renderer_string;
                        goto finish;
                }

                g_printerr ("mate-session-check-accelerated: GLES Helper exited with code %d\n", estatus);
        }
#endif

        /* If we got here, GL software rendering is our best bet */
        if (gl_software_rendering || gles_software_rendering) {
                is_software_rendering = TRUE;
                is_accelerated = TRUE;

                if (gl_software_rendering)
                        renderer_string = gl_renderer_string;
#ifdef HAVE_GLESV2
                else if (gles_software_rendering)
                        renderer_string = gles_renderer_string;
#endif

                goto finish;
        }

        /* Both helpers failed; print their error messages */
        if (gl_error != NULL) {
                g_printerr ("mate-session-check-accelerated: Failed to run GL helper: %s\n", gl_error->message);
                g_clear_error (&gl_error);
        }

#ifdef HAVE_GLESV2
        if (gles_error != NULL) {
                g_printerr ("mate-session-check-accelerated: Failed to run GLES helper: %s\n", gles_error->message);
                g_clear_error (&gles_error);
        }
#endif

 finish:
	if (is_accelerated) {
		XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
				rootwin,
				is_accelerated_atom,
				XA_CARDINAL, 32, PropModeReplace, (guchar *) &is_accelerated, 1);
	}

	if (is_software_rendering) {
		XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
				rootwin,
				is_software_rendering_atom,
				XA_CARDINAL, 32, PropModeReplace, (guchar *) &is_software_rendering, 1);
	}

        if (renderer_string != NULL) {
                XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
				rootwin,
				renderer_atom,
				XA_STRING, 8, PropModeReplace, (guchar *) renderer_string, strlen (renderer_string));

                /* Print the renderer */
                g_print ("%s", renderer_string);
        }

        gdk_display_sync (display);

        g_free (gl_renderer_string);
#ifdef HAVE_GLESV2
        g_free (gles_renderer_string);
#endif

        return is_accelerated ? 0 : 1;
}
