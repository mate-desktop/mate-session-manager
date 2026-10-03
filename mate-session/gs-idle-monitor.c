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

#include "config.h"

#include <time.h>
#include <string.h>

#ifdef HAVE_WAYLAND
#include <wayland-client.h>
#include "ext-idle-notify-v1-client.h"
#endif /* HAVE_WAYLAND */

#include <X11/Xlib.h>
#include <X11/extensions/sync.h>

#ifdef HAVE_XTEST
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */

#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#include "gs-idle-monitor.h"

static void gs_idle_monitor_finalize   (GObject             *object);

struct _GSIdleMonitor {
        GObject      parent;
        GHashTable  *watches;
        int          sync_event_base;
        XSyncCounter counter;

        /* For use with XTest */
        int         *keycode;
        int          keycode1;
        int          keycode2;
        gboolean     have_xtest;

#ifdef HAVE_WAYLAND
        /* For use with the ext-idle-notify-v1 protocol */
        struct wl_display           *wl_display;
        struct ext_idle_notifier_v1 *wl_notifier;
        struct wl_seat              *wl_seat;
        guint                        wl_source_id;
        uint32_t                     wl_notifier_version;
#endif
};

typedef struct
{
        guint                  id;
        guint                  interval_ms;
        GSIdleMonitorWatchFunc callback;
        gpointer               user_data;
        GSIdleMonitor         *monitor;
        XSyncAlarm             xalarm_positive;
        XSyncAlarm             xalarm_negative;

#ifdef HAVE_WAYLAND
        struct ext_idle_notification_v1 *notification;
#endif
} GSIdleMonitorWatch;

static guint32 watch_serial = 1;

G_DEFINE_TYPE (GSIdleMonitor, gs_idle_monitor, G_TYPE_OBJECT)

static gboolean
gs_idle_monitor_using_wayland (GSIdleMonitor *monitor)
{
        GdkDisplay *display;

        display = gdk_display_get_default ();

        if (display == NULL) {
                return FALSE;
        }

        return ! GDK_IS_X11_DISPLAY (display);
}

static gint64
_xsyncvalue_to_int64 (XSyncValue value)
{
        return ((guint64) XSyncValueHigh32 (value)) << 32
                | (guint64) XSyncValueLow32 (value);
}

static XSyncValue
_int64_to_xsyncvalue (gint64 value)
{
        XSyncValue ret;

        XSyncIntsToValue (&ret, value, ((guint64)value) >> 32);

        return ret;
}

static void
gs_idle_monitor_dispose (GObject *object)
{
        GSIdleMonitor *monitor;

        g_return_if_fail (GS_IS_IDLE_MONITOR (object));

        monitor = GS_IDLE_MONITOR (object);

#ifdef HAVE_WAYLAND
        if (monitor->wl_source_id != 0) {
                g_source_remove (monitor->wl_source_id);
                monitor->wl_source_id = 0;
        }

        if (monitor->wl_display != NULL) {
                wl_display_disconnect (monitor->wl_display);
                monitor->wl_display = NULL;
                monitor->wl_notifier = NULL;
                monitor->wl_seat = NULL;
        }
#endif

        if (monitor->watches != NULL) {
                g_hash_table_destroy (monitor->watches);
                monitor->watches = NULL;
        }

        G_OBJECT_CLASS (gs_idle_monitor_parent_class)->dispose (object);
}

static gboolean
_find_alarm (gpointer            key,
             GSIdleMonitorWatch *watch,
             XSyncAlarm         *alarm)
{
        g_debug ("Searching for %d in %d,%d", (int)*alarm, (int)watch->xalarm_positive, (int)watch->xalarm_negative);
        if (watch->xalarm_positive == *alarm
            || watch->xalarm_negative == *alarm) {
                return TRUE;
        }
        return FALSE;
}

static GSIdleMonitorWatch *
find_watch_for_alarm (GSIdleMonitor *monitor,
                      XSyncAlarm     alarm)
{
        GSIdleMonitorWatch *watch;

        watch = g_hash_table_find (monitor->watches,
                                   (GHRFunc)_find_alarm,
                                   &alarm);
        return watch;
}

#ifdef HAVE_XTEST
static gboolean
send_fake_event (GSIdleMonitor *monitor)
{
        if (! monitor->have_xtest) {
                return FALSE;
        }

        g_debug ("GSIdleMonitor: sending fake key");

        XLockDisplay (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
        XTestFakeKeyEvent (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                           *monitor->keycode,
                           True,
                           CurrentTime);
        XTestFakeKeyEvent (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                           *monitor->keycode,
                           False,
                           CurrentTime);
        XUnlockDisplay (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));

        /* Swap the keycode */
        if (monitor->keycode == &monitor->keycode1) {
                monitor->keycode = &monitor->keycode2;
        } else {
                monitor->keycode = &monitor->keycode1;
        }

        return TRUE;
}
#endif /* HAVE_XTEST */

void
gs_idle_monitor_reset (GSIdleMonitor *monitor)
{
        g_return_if_fail (GS_IS_IDLE_MONITOR (monitor));

        if (gs_idle_monitor_using_wayland (monitor)) {
                /* The Wayland compositor keeps track of the idle time on its
                 * own; there is no way for a client to reset it. */
                return;
        }

#ifdef HAVE_XTEST
        /* FIXME: is there a better way to reset the IDLETIME? */
        send_fake_event (monitor);
#endif
}

#ifdef HAVE_WAYLAND

static void
wl_idle_notification_handle_idled (void                            *data,
                                   struct ext_idle_notification_v1 *notification)
{
        GSIdleMonitorWatch *watch = data;
        gboolean            res;

        g_debug ("GSIdleMonitor: wayland watch %u went idle", watch->id);

        res = TRUE;
        if (watch->callback != NULL) {
                res = watch->callback (watch->monitor,
                                       watch->id,
                                       TRUE,
                                       watch->user_data);
        }

        (void) res;
}

static void
wl_idle_notification_handle_resumed (void                            *data,
                                     struct ext_idle_notification_v1 *notification)
{
        GSIdleMonitorWatch *watch = data;

        g_debug ("GSIdleMonitor: wayland watch %u resumed", watch->id);

        if (watch->callback != NULL) {
                watch->callback (watch->monitor,
                                 watch->id,
                                 FALSE,
                                 watch->user_data);
        }
}

static const struct ext_idle_notification_v1_listener wl_idle_notification_listener = {
        wl_idle_notification_handle_idled,
        wl_idle_notification_handle_resumed,
};

static void
wl_registry_handle_global (void                *data,
                           struct wl_registry *registry,
                           uint32_t             name,
                           const char          *interface,
                           uint32_t             version)
{
        GSIdleMonitor *monitor = data;

        if (g_strcmp0 (interface, ext_idle_notifier_v1_interface.name) == 0) {
                monitor->wl_notifier_version = MIN (version, 2);
                monitor->wl_notifier = wl_registry_bind (registry,
                                                         name,
                                                         &ext_idle_notifier_v1_interface,
                                                         monitor->wl_notifier_version);
        } else if (monitor->wl_seat == NULL
                   && g_strcmp0 (interface, wl_seat_interface.name) == 0) {
                /* ext-idle-notify-v1 notifications are tied to a seat. */
                monitor->wl_seat = wl_registry_bind (registry,
                                                     name,
                                                     &wl_seat_interface,
                                                     1);
        }
}

static void
wl_registry_handle_global_remove (void                *data,
                                  struct wl_registry *registry,
                                  uint32_t             name)
{
}

static const struct wl_registry_listener wl_registry_listener = {
        wl_registry_handle_global,
        wl_registry_handle_global_remove,
};

static gboolean
wl_dispatch_source_cb (GIOChannel   *source,
                       GIOCondition  condition,
                       gpointer      data)
{
        GSIdleMonitor *monitor = data;

        if (condition & (G_IO_HUP | G_IO_ERR)) {
                g_warning ("GSIdleMonitor: Wayland connection lost; idle detection disabled");
                wl_display_disconnect (monitor->wl_display);
                monitor->wl_display = NULL;
                monitor->wl_notifier = NULL;
                return G_SOURCE_REMOVE;
        }

        if (wl_display_dispatch (monitor->wl_display) < 0) {
                g_warning ("GSIdleMonitor: Wayland dispatch failed; idle detection disabled");
                wl_display_disconnect (monitor->wl_display);
                monitor->wl_display = NULL;
                monitor->wl_notifier = NULL;
                return G_SOURCE_REMOVE;
        }

        return G_SOURCE_CONTINUE;
}

static gboolean
init_wayland (GSIdleMonitor *monitor)
{
        struct wl_registry *registry;
        GIOChannel         *channel;

        monitor->wl_display = wl_display_connect (NULL);
        if (monitor->wl_display == NULL) {
                return FALSE;
        }

        registry = wl_display_get_registry (monitor->wl_display);
        wl_registry_add_listener (registry, &wl_registry_listener, monitor);

        if (wl_display_roundtrip (monitor->wl_display) < 0
            || monitor->wl_notifier == NULL) {
                wl_display_disconnect (monitor->wl_display);
                monitor->wl_display = NULL;
                return FALSE;
        }

        channel = g_io_channel_unix_new (wl_display_get_fd (monitor->wl_display));
        monitor->wl_source_id = g_io_add_watch (channel,
                                                G_IO_IN | G_IO_HUP | G_IO_ERR,
                                                wl_dispatch_source_cb,
                                                monitor);
        g_io_channel_unref (channel);

        return TRUE;
}

static gboolean
_wayland_notification_set (GSIdleMonitor      *monitor,
                           GSIdleMonitorWatch *watch)
{
        if (monitor->wl_notifier == NULL || monitor->wl_seat == NULL) {
                return FALSE;
        }

        if (monitor->wl_notifier_version >= 2) {
                /* Track raw user input, matching the X11 IDLETIME counter
                 * semantics used on X11 sessions. */
                watch->notification = ext_idle_notifier_v1_get_input_idle_notification (monitor->wl_notifier,
                                                                                        (uint32_t) watch->interval_ms,
                                                                                        monitor->wl_seat);
        } else {
                watch->notification = ext_idle_notifier_v1_get_idle_notification (monitor->wl_notifier,
                                                                                  (uint32_t) watch->interval_ms,
                                                                                  monitor->wl_seat);
        }

        if (watch->notification == NULL) {
                return FALSE;
        }

        ext_idle_notification_v1_add_listener (watch->notification,
                                               &wl_idle_notification_listener,
                                               watch);
        return TRUE;
}

#endif /* HAVE_WAYLAND */

static void
handle_alarm_notify_event (GSIdleMonitor         *monitor,
                           XSyncAlarmNotifyEvent *alarm_event)
{
        GSIdleMonitorWatch *watch;
        gboolean            res;
        gboolean            condition;

        if (alarm_event->state == XSyncAlarmDestroyed) {
                return;
        }

        watch = find_watch_for_alarm (monitor, alarm_event->alarm);

        if (watch == NULL) {
                g_debug ("Unable to find watch for alarm %d", (int)alarm_event->alarm);
                return;
        }

        g_debug ("Watch %d fired, idle time = %" G_GINT64_FORMAT,
                 watch->id,
                 _xsyncvalue_to_int64 (alarm_event->counter_value));

        if (alarm_event->alarm == watch->xalarm_positive) {
                condition = TRUE;
        } else {
                condition = FALSE;
        }

        res = TRUE;
        if (watch->callback != NULL) {
                res = watch->callback (monitor,
                                       watch->id,
                                       condition,
                                       watch->user_data);
        }

        if (! res) {
                /* reset all timers */
                g_debug ("GSIdleMonitor: callback returned FALSE; resetting idle time");
                gs_idle_monitor_reset (monitor);
        }
}

static GdkFilterReturn
xevent_filter (GdkXEvent     *xevent,
               GdkEvent      *event,
               GSIdleMonitor *monitor)
{
        XEvent                *ev;
        XSyncAlarmNotifyEvent *alarm_event;

        ev = xevent;
        if (ev->xany.type != monitor->sync_event_base + XSyncAlarmNotify) {
                return GDK_FILTER_CONTINUE;
        }

        alarm_event = xevent;

        handle_alarm_notify_event (monitor, alarm_event);

        return GDK_FILTER_CONTINUE;
}

static gboolean
init_xsync (GSIdleMonitor *monitor)
{
        int                 sync_error_base;
        int                 res;
        int                 major;
        int                 minor;
        int                 i;
        int                 ncounters;
        XSyncSystemCounter *counters;

        res = XSyncQueryExtension (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                   &monitor->sync_event_base,
                                   &sync_error_base);
        if (! res) {
                g_warning ("GSIdleMonitor: Sync extension not present");
                return FALSE;
        }

        res = XSyncInitialize (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &major, &minor);
        if (! res) {
                g_warning ("GSIdleMonitor: Unable to initialize Sync extension");
                return FALSE;
        }

        counters = XSyncListSystemCounters (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &ncounters);
        for (i = 0; i < ncounters; i++) {
                if (counters[i].name != NULL
                    && strcmp (counters[i].name, "IDLETIME") == 0) {
                        monitor->counter = counters[i].counter;
                        break;
                }
        }
        XSyncFreeSystemCounterList (counters);

        if (monitor->counter == None) {
                g_warning ("GSIdleMonitor: IDLETIME counter not found");
                return FALSE;
        }

        gdk_window_add_filter (NULL, (GdkFilterFunc)xevent_filter, monitor);

        return TRUE;
}

static void
_init_xtest (GSIdleMonitor *monitor)
{
#ifdef HAVE_XTEST
        int a, b, c, d;

        XLockDisplay (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
        monitor->have_xtest = (XTestQueryExtension (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &a, &b, &c, &d) == True);
        if (monitor->have_xtest) {
                monitor->keycode1 = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XK_Alt_L);
                if (monitor->keycode1 == 0) {
                        g_warning ("keycode1 not existent");
                }
                monitor->keycode2 = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XK_Alt_R);
                if (monitor->keycode2 == 0) {
                        monitor->keycode2 = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XK_Alt_L);
                        if (monitor->keycode2 == 0) {
                                g_warning ("keycode2 not existent");
                        }
                }
                monitor->keycode = &monitor->keycode1;
        }
        XUnlockDisplay (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()));
#endif /* HAVE_XTEST */
}

static GObject *
gs_idle_monitor_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
        GSIdleMonitor *monitor;
        GdkDisplay    *display;

        monitor = GS_IDLE_MONITOR (G_OBJECT_CLASS (gs_idle_monitor_parent_class)->constructor (type,
                                                                                               n_construct_properties,
                                                                                               construct_properties));

        display = gdk_display_get_default ();

        if (display != NULL && GDK_IS_X11_DISPLAY (display)) {
                _init_xtest (monitor);

                if (! init_xsync (monitor)) {
                        g_object_unref (monitor);
                        return NULL;
                }
        } else {
#ifdef HAVE_WAYLAND
                if (! init_wayland (monitor)) {
                        /* Fall back to a monitor that never reports idle. */
                        g_warning ("GSIdleMonitor: unable to initialize Wayland idle notifications; idle detection disabled");
                }
#else
                g_warning ("GSIdleMonitor: this build has no Wayland idle support; idle detection disabled");
#endif
        }

        return G_OBJECT (monitor);
}

static void
gs_idle_monitor_class_init (GSIdleMonitorClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gs_idle_monitor_finalize;
        object_class->dispose = gs_idle_monitor_dispose;
        object_class->constructor = gs_idle_monitor_constructor;
}

static guint32
get_next_watch_serial (void)
{
        guint32 serial;

        serial = watch_serial++;

        if ((gint32)watch_serial < 0) {
                watch_serial = 1;
        }

        /* FIXME: make sure it isn't in the hash */

        return serial;
}

static GSIdleMonitorWatch *
idle_monitor_watch_new (guint interval)
{
        GSIdleMonitorWatch *watch;

        watch = g_slice_new0 (GSIdleMonitorWatch);
        watch->interval_ms = interval;
        watch->id = get_next_watch_serial ();
        watch->xalarm_positive = None;
        watch->xalarm_negative = None;

        return watch;
}

static void
idle_monitor_watch_free (GSIdleMonitorWatch *watch)
{
        if (watch == NULL) {
                return;
        }
#ifdef HAVE_WAYLAND
        if (watch->notification != NULL) {
                ext_idle_notification_v1_destroy (watch->notification);
                watch->notification = NULL;
        }
#endif
        if (watch->xalarm_positive != None) {
                XSyncDestroyAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), watch->xalarm_positive);
        }
        if (watch->xalarm_negative != None) {
                XSyncDestroyAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), watch->xalarm_negative);
        }
        g_slice_free (GSIdleMonitorWatch, watch);
}

static void
gs_idle_monitor_init (GSIdleMonitor *monitor)
{
        monitor->watches = g_hash_table_new_full (NULL,
                                                  NULL,
                                                  NULL,
                                                  (GDestroyNotify)idle_monitor_watch_free);

        monitor->counter = None;
}

static void
gs_idle_monitor_finalize (GObject *object)
{
        GSIdleMonitor *idle_monitor;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GS_IS_IDLE_MONITOR (object));

        idle_monitor = GS_IDLE_MONITOR (object);

        g_return_if_fail (idle_monitor != NULL);

        G_OBJECT_CLASS (gs_idle_monitor_parent_class)->finalize (object);
}

GSIdleMonitor *
gs_idle_monitor_new (void)
{
        GObject *idle_monitor;

        idle_monitor = g_object_new (GS_TYPE_IDLE_MONITOR,
                                     NULL);

        return GS_IDLE_MONITOR (idle_monitor);
}

static gboolean
_xsync_alarm_set (GSIdleMonitor      *monitor,
                  GSIdleMonitorWatch *watch)
{
        XSyncAlarmAttributes attr;
        XSyncValue           delta;
        XSyncValue           interval;
        guint                flags;

        flags = XSyncCACounter
                | XSyncCAValueType
                | XSyncCATestType
                | XSyncCAValue
                | XSyncCADelta
                | XSyncCAEvents;

        XSyncIntToValue (&delta, 0);
        interval = _int64_to_xsyncvalue ((gint64)watch->interval_ms);
        attr.trigger.counter = monitor->counter;
        attr.trigger.value_type = XSyncAbsolute;
        attr.trigger.wait_value = interval;
        attr.delta = delta;
        attr.events = TRUE;

        attr.trigger.wait_value = _int64_to_xsyncvalue ((gint64)watch->interval_ms - 1);
        attr.trigger.test_type = XSyncPositiveTransition;
        if (watch->xalarm_positive != None) {
                g_debug ("GSIdleMonitor: updating alarm for positive transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value));
                XSyncChangeAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), watch->xalarm_positive, flags, &attr);
        } else {
                g_debug ("GSIdleMonitor: creating new alarm for positive transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value));
                watch->xalarm_positive = XSyncCreateAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), flags, &attr);
        }

        attr.trigger.test_type = XSyncNegativeTransition;
        if (watch->xalarm_negative != None) {
                g_debug ("GSIdleMonitor: updating alarm for negative transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value));
                XSyncChangeAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), watch->xalarm_negative, flags, &attr);
        } else {
                g_debug ("GSIdleMonitor: creating new alarm for negative transition wait=%" G_GINT64_FORMAT,
                         _xsyncvalue_to_int64 (attr.trigger.wait_value));
                watch->xalarm_negative = XSyncCreateAlarm (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), flags, &attr);
        }

        return TRUE;
}

guint
gs_idle_monitor_add_watch (GSIdleMonitor         *monitor,
                           guint                  interval,
                           GSIdleMonitorWatchFunc callback,
                           gpointer               user_data)
{
        GSIdleMonitorWatch *watch;

        g_return_val_if_fail (GS_IS_IDLE_MONITOR (monitor), 0);
        g_return_val_if_fail (callback != NULL, 0);

        watch = idle_monitor_watch_new (interval);
        watch->callback = callback;
        watch->user_data = user_data;
        watch->monitor = monitor;

        if (gs_idle_monitor_using_wayland (monitor)) {
#ifdef HAVE_WAYLAND
                _wayland_notification_set (monitor, watch);
#else
                g_warning ("GSIdleMonitor: Wayland idle notifications not supported");
#endif
        } else {
                _xsync_alarm_set (monitor, watch);
        }

        g_hash_table_insert (monitor->watches,
                             GUINT_TO_POINTER (watch->id),
                             watch);
        return watch->id;
}

void
gs_idle_monitor_remove_watch (GSIdleMonitor *monitor,
                              guint          id)
{
        g_return_if_fail (GS_IS_IDLE_MONITOR (monitor));

        g_hash_table_remove (monitor->watches,
                             GUINT_TO_POINTER (id));
}
