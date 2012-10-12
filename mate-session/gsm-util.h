/* gsm-util.h
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

#ifndef __GSM_UTIL_H__
#define __GSM_UTIL_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

char *      gsm_util_find_desktop_file_for_app_name (const char  *app_name,
                                                     char       **dirs);

gchar      *gsm_util_get_empty_tmp_session_dir      (void);

const char *gsm_util_get_saved_session_dir          (void);

gchar**     gsm_util_get_app_dirs                   (void);

gchar**     gsm_util_get_autostart_dirs             (void);

gchar **    gsm_util_get_desktop_dirs               (void);

gboolean    gsm_util_text_is_blank                  (const char *str);

void        gsm_util_init_error                     (gboolean    fatal,
                                                     const char *format, ...);

char *      gsm_util_generate_startup_id            (void);

void        gsm_util_setenv                         (const char *variable,
                                                     const char *value);

#ifdef __cplusplus
}
#endif

#endif /* __GSM_UTIL_H__ */
