/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 1999 Free Software Foundation, Inc.
 * Copyright (C) 2007, 2009 Vincent Untz.
 * Copyright (C) 2008 Lucas Rocha.
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GSP_APP_H
#define __GSP_APP_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GSP_TYPE_APP            (gsp_app_get_type ())
G_DECLARE_DERIVABLE_TYPE (GspApp, gsp_app, GSP, APP, GObject)

struct _GspAppClass
{
        GObjectClass parent_class;

        void (* changed) (GspApp *app);
        void (* removed) (GspApp *app);
};

void             gsp_app_create            (const char   *name,
                                            const char   *comment,
                                            const char   *exec,
                                            guint         delay);
void             gsp_app_update            (GspApp       *app,
                                            const char   *name,
                                            const char   *comment,
                                            const char   *exec,
                                            guint         delay);

gboolean         gsp_app_copy_desktop_file (const char   *uri);

void             gsp_app_delete            (GspApp       *app);

const char      *gsp_app_get_basename      (GspApp       *app);
const char      *gsp_app_get_path          (GspApp       *app);

gboolean         gsp_app_get_hidden        (GspApp       *app);
void             gsp_app_set_hidden        (GspApp       *app,
                                            gboolean      hidden);

gboolean         gsp_app_get_nodisplay     (GspApp       *app);

const char      *gsp_app_get_name          (GspApp       *app);
const char      *gsp_app_get_exec          (GspApp       *app);
const char      *gsp_app_get_comment       (GspApp       *app);
guint            gsp_app_get_delay         (GspApp       *app);

const char      *gsp_app_get_description   (GspApp       *app);
GIcon           *gsp_app_get_icon          (GspApp       *app);

/* private interface for GspAppManager only */

GspApp          *gsp_app_new                      (const char   *path,
                                                   unsigned int  xdg_position);

void             gsp_app_reload_at                (GspApp       *app,
                                                   const char   *path,
                                                   unsigned int  xdg_position);

unsigned int     gsp_app_get_xdg_position         (GspApp       *app);
unsigned int     gsp_app_get_xdg_system_position  (GspApp       *app);
void             gsp_app_set_xdg_system_position  (GspApp       *app,
                                                   unsigned int  position);

G_END_DECLS

#endif /* __GSP_APP_H */
