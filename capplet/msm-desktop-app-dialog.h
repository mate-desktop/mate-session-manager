#ifndef __MSM_DESKTOP_APP_DIALOG_H
#define __MSM_DESKTOP_APP_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>
#include "gsp-app-manager.h"

G_BEGIN_DECLS

#define MSM_TYPE_DESKTOP_APP_DIALOG              (msm_desktop_app_dialog_get_type ())
G_DECLARE_FINAL_TYPE (MsmDesktopAppDialog, msm_desktop_app_dialog, MSM, DESKTOP_APP_DIALOG, GtkDialog)

GtkWidget    *msm_desktop_app_dialog_new         (GspAppManager       *manager);

const char   *msm_dektop_app_dialog_get_selected (MsmDesktopAppDialog *dialog);

G_END_DECLS

#endif /* __GSM_APP_DIALOG_H */
