/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
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


#ifndef __GSM_STORE_H
#define __GSM_STORE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSM_TYPE_STORE         (gsm_store_get_type ())
G_DECLARE_DERIVABLE_TYPE (GsmStore, gsm_store, GSM, STORE, GObject)

struct _GsmStoreClass
{
        GObjectClass   parent_class;

        void          (* added)    (GsmStore   *store,
                                    const char *id);
        void          (* removed)  (GsmStore   *store,
                                    const char *id);
};

typedef enum
{
         GSM_STORE_ERROR_GENERAL
} GsmStoreError;

#define GSM_STORE_ERROR gsm_store_error_quark ()

typedef gboolean (*GsmStoreFunc) (const char *id,
                                  GObject    *object,
                                  gpointer    user_data);

GQuark              gsm_store_error_quark              (void);

GsmStore *          gsm_store_new                      (void);

gboolean            gsm_store_get_locked               (GsmStore    *store);
void                gsm_store_set_locked               (GsmStore    *store,
                                                        gboolean     locked);

guint               gsm_store_size                     (GsmStore    *store);
gboolean            gsm_store_add                      (GsmStore    *store,
                                                        const char  *id,
                                                        GObject     *object);
void                gsm_store_clear                    (GsmStore    *store);
gboolean            gsm_store_remove                   (GsmStore    *store,
                                                        const char  *id);

void                gsm_store_foreach                  (GsmStore    *store,
                                                        GsmStoreFunc func,
                                                        gpointer     user_data);
guint               gsm_store_foreach_remove           (GsmStore    *store,
                                                        GsmStoreFunc func,
                                                        gpointer     user_data);
GObject *           gsm_store_find                     (GsmStore    *store,
                                                        GsmStoreFunc predicate,
                                                        gpointer     user_data);
GObject *           gsm_store_lookup                   (GsmStore    *store,
                                                        const char  *id);

G_END_DECLS

#endif /* __GSM_STORE_H */
