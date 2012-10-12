/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

#define GS_TYPE_IDLE_MONITOR         (gs_idle_monitor_get_type ())
#define GS_IDLE_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_IDLE_MONITOR, GSIdleMonitor))
#define GS_IDLE_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_IDLE_MONITOR, GSIdleMonitorClass))
#define GS_IS_IDLE_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_IDLE_MONITOR))
#define GS_IS_IDLE_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_IDLE_MONITOR))
#define GS_IDLE_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_IDLE_MONITOR, GSIdleMonitorClass))

typedef struct GSIdleMonitorPrivate GSIdleMonitorPrivate;

typedef struct
{
        GObject               parent;
        GSIdleMonitorPrivate *priv;
} GSIdleMonitor;

typedef struct
{
        GObjectClass          parent_class;
} GSIdleMonitorClass;

typedef gboolean (*GSIdleMonitorWatchFunc) (GSIdleMonitor *monitor,
                                            guint          id,
                                            gboolean       condition,
                                            gpointer       user_data);

GType           gs_idle_monitor_get_type       (void);

GSIdleMonitor * gs_idle_monitor_new            (void);

guint           gs_idle_monitor_add_watch      (GSIdleMonitor         *monitor,
                                                guint                  interval,
                                                GSIdleMonitorWatchFunc callback,
                                                gpointer               user_data);

void            gs_idle_monitor_remove_watch   (GSIdleMonitor         *monitor,
                                                guint                  id);
void            gs_idle_monitor_reset          (GSIdleMonitor         *monitor);


#ifdef __cplusplus
}
#endif

#endif /* __GS_IDLE_MONITOR_H */
