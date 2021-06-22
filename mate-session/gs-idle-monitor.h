/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef __GS_IDLE_MONITOR_H
#define __GS_IDLE_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GS_TYPE_IDLE_MONITOR         (gs_idle_monitor_get_type ())
G_DECLARE_FINAL_TYPE (GSIdleMonitor, gs_idle_monitor, GS, IDLE_MONITOR, GObject)

typedef gboolean (*GSIdleMonitorWatchFunc) (GSIdleMonitor *monitor,
                                            guint          id,
                                            gboolean       condition,
                                            gpointer       user_data);

GSIdleMonitor * gs_idle_monitor_new            (void);

guint           gs_idle_monitor_add_watch      (GSIdleMonitor         *monitor,
                                                guint                  interval,
                                                GSIdleMonitorWatchFunc callback,
                                                gpointer               user_data);

void            gs_idle_monitor_remove_watch   (GSIdleMonitor         *monitor,
                                                guint                  id);
void            gs_idle_monitor_reset          (GSIdleMonitor         *monitor);


G_END_DECLS

#endif /* __GS_IDLE_MONITOR_H */
