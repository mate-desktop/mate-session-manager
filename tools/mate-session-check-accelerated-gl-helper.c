/*
 * Copyright (C) 2010      Novell, Inc.
 * Copyright (C) 2006-2009 Red Hat, Inc.
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
 *   Vincent Untz <vuntz@gnome.org>
 *
 * Most of the code comes from desktop-effects [1], released under GPLv2+.
 * desktop-effects was written by:
 *   Soren Sandmann <sandmann@redhat.com>
 *
 * [1] http://git.fedorahosted.org/git/?p=desktop-effects.git;a=blob_plain;f=desktop-effects.c;hb=HEAD
 */

/*
 * Here's the rationale behind this helper, quoting Owen, in his mail to the
 * release team:
 * (http://mail.gnome.org/archives/release-team/2010-June/msg00079.html)
 *
 * """
 * There are some limits to what we can do here automatically without
 * knowing anything about the driver situation on the system. The basic
 * problem is that there are all sorts of suck:
 *
 *  * No GL at all. This typically only happens if a system is
 *    misconfigured.
 *
 *  * Only software GL. This one is easy to detect. We have code in
 *    the Fedora desktop-effects tool, etc.
 *
 *  * GL that isn't featureful enough. (Tiny texture size limits, no
 *    texture-from-pixmap, etc.) Possible to detect with more work, but
 *    largely a fringe case.
 *
 *  * Buggy GL. This isn't possible to detect. Except for the case where
 *    all GL programs crash. For that reason, we probably don't want
 *    gnome-session to directly try and do any GL detection; better to
 *    use a helper binary.
 *
 *  * Horribly slow hardware GL. We could theoretically develop some sort
 *    of benchmark, but it's a tricky area. And how slow is too slow?
 * """
 *
 * Some other tools are doing similar checks:
 *  - desktop-effects (Fedora Config Tool) [1]
 *  - drak3d (Mandriva Config Tool) [2]
 *  - compiz-manager (Compiz wrapper) [3]
 *
 * [1] http://git.fedorahosted.org/git/?p=desktop-effects.git;a=blob_plain;f=desktop-effects.c;hb=HEAD
 * [2] http://svn.mandriva.com/cgi-bin/viewvc.cgi/soft/drak3d/trunk/lib/Xconfig/glx.pm?view=markup
 * [3] http://git.compiz.org/fusion/misc/compiz-manager/tree/compiz-manager
 */

/* for strcasestr */
#define _GNU_SOURCE

#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <regex.h>

#ifdef __FreeBSD__
#include <kenv.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "mate-session-check-accelerated-common.h"

#define SIZE_UNSET 0
#define SIZE_ERROR -1
static int max_texture_size = SIZE_UNSET;
static int max_renderbuffer_size = SIZE_UNSET;
static gboolean has_llvmpipe = FALSE;

static inline void
_print_error (const char *str)
{
        fprintf (stderr, "mate-session-is-accelerated: %s\n", str);
}

#define CMDLINE_UNSET -1
#define CMDLINE_NON_FALLBACK_FORCED 0
#define CMDLINE_FALLBACK_FORCED 1

#if defined(__linux__)
static int
_parse_kcmdline (void)
{
        int ret = CMDLINE_UNSET;
        GRegex *regex;
        GMatchInfo *match;
        char *contents;
        char *word;
        const char *arg;

        if (!g_file_get_contents ("/proc/cmdline", &contents, NULL, NULL))
                return ret;

        regex = g_regex_new ("mate.fallback=(\\S+)", 0, G_REGEX_MATCH_NOTEMPTY, NULL);
        if (!g_regex_match (regex, contents, G_REGEX_MATCH_NOTEMPTY, &match))
                goto out;

        word = g_match_info_fetch (match, 0);
        g_debug ("Found command-line match '%s'", word);
        arg = word + strlen ("mate.fallback=");
        if (*arg != '0' && *arg != '1')
                fprintf (stderr, "mate-session-check-accelerated: Invalid value '%s' for mate.fallback passed in kernel command line.\n", arg);
        else
                ret = atoi (arg);
        g_free (word);

out:
        g_match_info_free (match);
        g_regex_unref (regex);
        g_free (contents);

        g_debug ("Command-line parsed to %d", ret);

        return ret;
}
#elif defined(__FreeBSD__)
static int
_parse_kcmdline (void)
{
        int ret = CMDLINE_UNSET;
        char value[KENV_MVALLEN];

        /* a compile time check to avoid unexpected stack overflow */
        _Static_assert(KENV_MVALLEN < 1024 * 1024, "KENV_MVALLEN is too large");

        if (kenv (KENV_GET, "mate.fallback", value, KENV_MVALLEN) == -1)
                return ret;

        if (*value != '0' && *value != '1')
                fprintf (stderr, "mate-session-is-accelerated: Invalid value '%s' for mate.fallback passed in kernel environment.\n", value);
        else
                ret = atoi (value);

        g_debug ("Kernel environment parsed to %d", ret);

        return ret;
}
#else
static int
_parse_kcmdline (void)
{
        return CMDLINE_UNSET;
}
#endif

static gboolean
_has_composite (Display *display)
{
        int dummy1, dummy2;

        return XCompositeQueryExtension (display, &dummy1, &dummy2);
}

static gboolean
_is_comment (const char *line)
{
        while (*line && isspace(*line))
                line++;

        if (*line == '#' || *line == '\0')
                return TRUE;
        return FALSE;
}

static gboolean
_is_gl_renderer_blacklisted (const char *renderer)
{
        FILE *blacklist;
        char *line = NULL;
        size_t line_len = 0;
        gboolean ret = TRUE;

        blacklist = fopen(PKGDATADIR "/hardware-compatibility", "r");
        if (blacklist == NULL)
                goto out;

        while (getline (&line, &line_len, blacklist) != -1) {
                int whitelist = 0;
                const char *re_str;
                regex_t re;
                int status;

                if (line == NULL)
                        break;

                /* Drop trailing \n */
                line[strlen(line) - 1] = '\0';

                if (_is_comment (line)) {
                        free (line);
                        line = NULL;
                        continue;
                }

                if (line[0] == '+')
                        whitelist = 1;
                else if (line[0] == '-')
                        whitelist = 0;
                else {
                        _print_error ("Invalid syntax in this line for hardware compatibility:");
                        _print_error (line);
                        free (line);
                        line = NULL;
                        continue;
                }

                re_str = line + 1;

                if (regcomp (&re, re_str, REG_EXTENDED|REG_ICASE|REG_NOSUB) != 0) {
                        _print_error ("Cannot use this regular expression for hardware compatibility:");
                        _print_error (re_str);
                } else {
                        status = regexec (&re, renderer, 0, NULL, 0);
                        regfree(&re);

                        if (status == 0) {
                                if (whitelist)
                                        ret = FALSE;
                                goto out;
                        }
                }

                free (line);
                line = NULL;
        }

        ret = FALSE;

out:
        if (line != NULL)
                free (line);

        if (blacklist != NULL)
                fclose (blacklist);

        return ret;
}

static char *
_get_hardware_gl (Display *display)
{
        int screen;
        Window root;
        XVisualInfo *visual = NULL;
        GLXContext context = NULL;
        XSetWindowAttributes cwa = { 0 };
        Window window = None;
        char *renderer = NULL;

        int attrlist[] = {
                GLX_RGBA,
                GLX_RED_SIZE, 1,
                GLX_GREEN_SIZE, 1,
                GLX_BLUE_SIZE, 1,
                GLX_DOUBLEBUFFER,
                None
        };

        screen = DefaultScreen (display);
        root = RootWindow (display, screen);

        visual = glXChooseVisual (display, screen, attrlist);
        if (!visual)
                goto out;

        context = glXCreateContext (display, visual, NULL, True);
        if (!context)
                goto out;

        cwa.colormap = XCreateColormap (display, root,
                                        visual->visual, AllocNone);
        cwa.background_pixel = 0;
        cwa.border_pixel = 0;
        window = XCreateWindow (display, root,
                                0, 0, 1, 1, 0,
                                visual->depth, InputOutput, visual->visual,
                                CWColormap | CWBackPixel | CWBorderPixel,
                                &cwa);

        if (!glXMakeCurrent (display, window, context))
                goto out;

        renderer = g_strdup ((const char *) glGetString (GL_RENDERER));
        if (_is_gl_renderer_blacklisted (renderer)) {
                g_clear_pointer (&renderer, g_free);
                goto out;
        }
        if (renderer && strcasestr (renderer, "llvmpipe"))
		has_llvmpipe = TRUE;

        /* we need to get the max texture and renderbuffer sizes while we have
         * a context, but we'll check their values later */

        glGetIntegerv (GL_MAX_TEXTURE_SIZE, &max_texture_size);
        if (glGetError() != GL_NO_ERROR)
                max_texture_size = SIZE_ERROR;

        glGetIntegerv (GL_MAX_RENDERBUFFER_SIZE_EXT, &max_renderbuffer_size);
        if (glGetError() != GL_NO_ERROR)
                max_renderbuffer_size = SIZE_ERROR;

out:
        glXMakeCurrent (display, None, None);
        if (context)
                glXDestroyContext (display, context);
        if (window)
                XDestroyWindow (display, window);
        if (cwa.colormap)
                XFreeColormap (display, cwa.colormap);

        return renderer;
}

static gboolean
_has_extension (const char *extension_list,
                const char *extension)
{
        char **extensions;
        guint i;
        gboolean ret;

        g_return_val_if_fail (extension != NULL, TRUE);

        /* Extension_list is one big string, containing extensions
         * separated by spaces. */
        if (extension_list == NULL)
                return FALSE;

        ret = FALSE;

        extensions = g_strsplit (extension_list, " ", -1);
        if (extensions == NULL)
                return FALSE;

        for (i = 0; extensions[i] != NULL; i++) {
                if (g_str_equal (extensions[i], extension)) {
                        ret = TRUE;
                        break;
                }
        }

        g_strfreev (extensions);

        return ret;
}

static gboolean
_has_texture_from_pixmap (Display *display)
{
        int screen;
        const char *server_extensions;
        const char *client_extensions;
        gboolean ret = FALSE;

        screen = DefaultScreen (display);

        server_extensions = glXQueryServerString (display, screen,
                                                  GLX_EXTENSIONS);
        if (!_has_extension (server_extensions,
                            "GLX_EXT_texture_from_pixmap"))
                goto out;

        client_extensions = glXGetClientString (display, GLX_EXTENSIONS);
        if (!_has_extension (client_extensions,
                            "GLX_EXT_texture_from_pixmap"))
                goto out;

        ret = TRUE;

out:
        return ret;
}

static void
_set_max_screen_size_property (Display *display, int screen, int size)
{
        Atom max_screen_size_atom;

        max_screen_size_atom = XInternAtom (display, "_GNOME_MAX_SCREEN_SIZE",
                                            False);

        /* Will be read by gnome-settings-daemon and
         * gnome-control-center to avoid display configurations where 3D
         * is not available (and would break gnome-shell) */
        XChangeProperty (display, RootWindow(display, screen),
                         max_screen_size_atom,
                         XA_CARDINAL, 32, PropModeReplace,
                         (unsigned char *)&size, 1);

        XSync(display, False);
}

static gboolean
_is_max_texture_size_big_enough (Display *display)
{
        int screen, size;

        screen = DefaultScreen (display);
        size = MIN(max_renderbuffer_size, max_texture_size);
        if (size < DisplayWidth (display, screen) ||
            size < DisplayHeight (display, screen))
                return FALSE;

        _set_max_screen_size_property (display, screen, size);

        return TRUE;
}

static gboolean print_renderer = FALSE;

static const GOptionEntry entries[] = {
        { "print-renderer", 'p', 0, G_OPTION_ARG_NONE, &print_renderer, "Print GL renderer name", NULL },
        { NULL },
};

int
main (int argc, char **argv)
{
        int             kcmdline_parsed;
        Display        *display = NULL;
        int             ret = HELPER_NO_ACCEL;
        GOptionContext *context;
        GError         *error = NULL;
        char           *renderer = NULL;

        setlocale (LC_ALL, "");

        context = g_option_context_new (NULL);
        g_option_context_add_main_entries (context, entries, NULL);

        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_error ("Can't parse command line: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        kcmdline_parsed = _parse_kcmdline ();
        if (kcmdline_parsed > CMDLINE_UNSET) {
                if (kcmdline_parsed == CMDLINE_NON_FALLBACK_FORCED) {
                        _print_error ("Non-fallback mode forced by kernel command line.");
                        ret = HELPER_ACCEL;
                        goto out;
                } else if (kcmdline_parsed == CMDLINE_FALLBACK_FORCED) {
                        _print_error ("Fallback mode forced by kernel command line.");
                        goto out;
                }
        }

        display = XOpenDisplay (NULL);
        if (!display) {
                _print_error ("No X display.");
                goto out;
        }

        if (!_has_composite (display)) {
                _print_error ("No composite extension.");
                goto out;
        }

        renderer = _get_hardware_gl (display);
        if (!renderer) {
                _print_error ("No hardware 3D support.");
                goto out;
        }

        if (!_has_texture_from_pixmap (display)) {
                _print_error ("No GLX_EXT_texture_from_pixmap support.");
                goto out;
        }

        if (!_is_max_texture_size_big_enough (display)) {
                _print_error ("GL_MAX_{TEXTURE,RENDERBUFFER}_SIZE is too small.");
                goto out;
        }

        ret = has_llvmpipe ? HELPER_SOFTWARE_RENDERING : HELPER_ACCEL;

        if (print_renderer)
                g_print ("%s", renderer);

out:
        if (display)
                XCloseDisplay (display);
        g_option_context_free (context);
        g_free (renderer);

        return ret;
}
