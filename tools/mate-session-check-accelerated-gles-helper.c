/* -*- mode:c; c-basic-offset: 8; indent-tabs-mode: nil; -*- */
/*
 *
 * Copyright (C) 2016 Endless Mobile, Inc
 * Copyright (C) 2016-2021 MATE Developers
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author:
 *   Cosimo Cecchi <cosimo@endlessm.com>
 */

/* for strcasestr */
#define _GNU_SOURCE

#include <config.h>

#include <gtk/gtk.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
#define GL_GLEXT_PROTOTYPES

#include <gdk/gdkx.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include "mate-session-check-accelerated-common.h"

#ifdef GDK_WINDOWING_X11
static EGLDisplay
get_display (void *native)
{
        EGLDisplay dpy = NULL;
        const char *client_exts = eglQueryString (NULL, EGL_EXTENSIONS);

        if (g_strstr_len (client_exts, -1, "EGL_KHR_platform_base")) {
                PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
                        (void *) eglGetProcAddress ("eglGetPlatformDisplay");

                if (get_platform_display)
                        dpy = get_platform_display (EGL_PLATFORM_X11_KHR, native, NULL);

                if (dpy)
                        return dpy;
        }

        if (g_strstr_len (client_exts, -1, "EGL_EXT_platform_base")) {
                PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
                        (void *) eglGetProcAddress ("eglGetPlatformDisplayEXT");

                if (get_platform_display)
                        dpy = get_platform_display (EGL_PLATFORM_X11_KHR, native, NULL);

                if (dpy)
                        return dpy;
        }

        return eglGetDisplay ((EGLNativeDisplayType) native);
}

static char *
get_gles_renderer (void)
{
        /* Select GLESv2 config */
        EGLint attribs[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 1,
                EGL_GREEN_SIZE, 1,
                EGL_BLUE_SIZE, 1,
                EGL_NONE
        };

        EGLint ctx_attribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL_NONE
        };

        gboolean egl_inited = FALSE;
        GdkDisplay *gdk_dpy;
        Display *display;
        Window win = None;
        EGLContext egl_ctx = NULL;
        EGLDisplay egl_dpy = NULL;
        EGLSurface egl_surf = NULL;
        char *renderer = NULL;

        gdk_dpy = gdk_display_get_default ();
        gdk_x11_display_error_trap_push (gdk_dpy);
        display = GDK_DISPLAY_XDISPLAY (gdk_dpy);
        egl_dpy = get_display (display);
        if (!egl_dpy) {
                g_warning ("eglGetDisplay() failed");
                goto out;
        }

        EGLint egl_major, egl_minor;
        if (!eglInitialize (egl_dpy, &egl_major, &egl_minor)) {
                g_warning ("eglInitialize() failed");
                goto out;
        }

        egl_inited = TRUE;

        EGLint num_configs;
        EGLConfig config;
        if (!eglChooseConfig (egl_dpy, attribs, &config, 1, &num_configs)) {
                g_warning ("Failed to get EGL configuration");
                goto out;
        }

        EGLint vid;
        if (!eglGetConfigAttrib (egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
                g_warning ("Failed to get EGL visual");
                goto out;
        }

        /* The X window visual must match the EGL config */
        XVisualInfo *vis_info, vis_template;
        int num_visuals;
        vis_template.visualid = vid;
        vis_info = XGetVisualInfo (display, VisualIDMask, &vis_template, &num_visuals);
        if (!vis_info) {
                g_warning ("Failed to get X visual");
                goto out;
        }

        XSetWindowAttributes attr;
        attr.colormap = XCreateColormap (display, DefaultRootWindow (display),
                                         vis_info->visual, AllocNone);
        win = XCreateWindow (display, DefaultRootWindow (display),
                             0, 0, /* x, y */
                             1, 1, /* width, height */
                             0,    /* border_width */
                             vis_info->depth, InputOutput,
                             vis_info->visual, CWColormap, &attr);
        XFree (vis_info);

        eglBindAPI (EGL_OPENGL_ES_API);

        egl_ctx = eglCreateContext (egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs);
        if (!egl_ctx) {
                g_warning ("Failed to create EGL context");
                goto out;
        }

        egl_surf = eglCreateWindowSurface (egl_dpy, config, win, NULL);
        if (!egl_surf) {
                g_warning ("Failed to create EGL surface");
                goto out;
        }

        if (!eglMakeCurrent (egl_dpy, egl_surf, egl_surf, egl_ctx)) {
                g_warning ("eglMakeCurrent() failed");
                goto out;
        }

        renderer = g_strdup ((const char *) glGetString (GL_RENDERER));

 out:
        if (egl_ctx)
                eglDestroyContext (egl_dpy, egl_ctx);
        if (egl_surf)
                eglDestroySurface (egl_dpy, egl_surf);
        if (egl_inited)
                eglTerminate (egl_dpy);
        if (win != None)
                XDestroyWindow (display, win);

        gdk_x11_display_error_trap_pop_ignored (gdk_dpy);
        return renderer;
}
#endif

static gboolean print_renderer = FALSE;

static const GOptionEntry entries[] = {
        { "print-renderer", 'p', 0, G_OPTION_ARG_NONE, &print_renderer, "Print EGL renderer name", NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },
};

int
main (int argc,
      char **argv)
{
        GOptionContext *context;
        int ret = HELPER_NO_ACCEL;
        GError *error = NULL;

        setlocale (LC_ALL, "");

        context = g_option_context_new (NULL);
        g_option_context_add_group (context, gtk_get_option_group (TRUE));
        g_option_context_add_main_entries (context, entries, NULL);

        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_error ("Can't parse command line: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

#ifdef GDK_WINDOWING_X11
        char *renderer = get_gles_renderer ();
        if (renderer != NULL) {
                if (print_renderer)
                        g_print ("%s", renderer);
                if (strcasestr (renderer, "llvmpipe"))
                        ret = HELPER_SOFTWARE_RENDERING;
                else
                        ret = HELPER_ACCEL;
        }
        g_free (renderer);
#endif

out:
        g_option_context_free (context);
        return ret;
}
