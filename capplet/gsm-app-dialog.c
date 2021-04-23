/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gsm-util.h"

#include "gsm-app-dialog.h"

#ifdef __GNUC__
#define UNUSED_VARIABLE __attribute__ ((unused))
#else
#define UNUSED_VARIABLE
#endif

struct _GsmAppDialog
{
        GtkDialog  parent;

        GtkWidget *name_entry;
        GtkWidget *command_entry;
        GtkWidget *comment_entry;
        GtkWidget *delay_spin;
        GtkWidget *browse_button;
        GtkWidget *ok_button;
        GtkWidget *ok_button_img;

        char      *name;
        char      *command;
        char      *comment;
        guint      delay;
};

enum {
        PROP_0,
        PROP_NAME,
        PROP_COMMAND,
        PROP_COMMENT,
        PROP_DELAY
};

G_DEFINE_TYPE (GsmAppDialog, gsm_app_dialog, GTK_TYPE_DIALOG)

static char *
make_exec_uri (const char *exec)
{
        GString    *str;
        const char *c;

        if (exec == NULL) {
                return g_strdup ("");
        }

        if (strchr (exec, ' ') == NULL) {
                return g_strdup (exec);
        }

        str = g_string_new_len (NULL, strlen (exec));

        str = g_string_append_c (str, '"');
        for (c = exec; *c != '\0'; c++) {
                /* FIXME: GKeyFile will add an additional backslach so we'll
                 * end up with toto\\" instead of toto\"
                 * We could use g_key_file_set_value(), but then we don't
                 * benefit from the other escaping that glib is doing...
                 */
                if (*c == '"') {
                        str = g_string_append (str, "\\\"");
                } else {
                        str = g_string_append_c (str, *c);
                }
        }
        str = g_string_append_c (str, '"');

        return g_string_free (str, FALSE);
}

static void
on_browse_button_clicked (GtkWidget    *widget,
                          GsmAppDialog *dialog)
{
        GtkWidget *chooser;
        int        response;

        chooser = gtk_file_chooser_dialog_new ("",
                                               GTK_WINDOW (dialog),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               "gtk-cancel",
                                               GTK_RESPONSE_CANCEL,
                                               "gtk-open",
                                               GTK_RESPONSE_ACCEPT,
                                               NULL);

        gtk_window_set_transient_for (GTK_WINDOW (chooser),
                                      GTK_WINDOW (dialog));

        gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser), TRUE);

        gtk_window_set_title (GTK_WINDOW (chooser), _("Select Command"));

        gtk_widget_show (chooser);

        response = gtk_dialog_run (GTK_DIALOG (chooser));

        if (response == GTK_RESPONSE_ACCEPT) {
                char *text;
                char *uri;

                text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

                uri = make_exec_uri (text);

                g_free (text);

                gtk_entry_set_text (GTK_ENTRY (dialog->command_entry), uri);

                g_free (uri);
        }

        gtk_widget_destroy (chooser);
}

static void
on_entry_activate (GtkEntry     *entry,
                   GsmAppDialog *dialog)
{
        gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static gboolean
on_delay_spin_output (GtkSpinButton *spin,
                      GsmAppDialog  *dialog)
{
        GtkAdjustment *adjustment;
        gchar *text;
        int value;

        adjustment = gtk_spin_button_get_adjustment (spin);
        value = gtk_adjustment_get_value (adjustment);
        dialog->delay = value;

        if (value == 1)
                text = g_strdup_printf ("%d %s", value, _("second"));
        else if (value > 1)
                text = g_strdup_printf ("%d %s", value, _("seconds"));
        else
                text = g_strdup_printf ("%d", value);

        gtk_entry_set_text (GTK_ENTRY (spin), text);
        g_free (text);

        return TRUE;
}

static void
setup_dialog (GsmAppDialog *dialog)
{
        if (dialog->name == NULL
            && dialog->command == NULL
            && dialog->comment == NULL) {
                gtk_window_set_title (GTK_WINDOW (dialog), _("Add Startup Program"));
                gtk_button_set_label (GTK_BUTTON (dialog->ok_button), _("_Add"));
                gtk_image_set_from_icon_name (GTK_IMAGE (dialog->ok_button_img), "list-add", GTK_ICON_SIZE_BUTTON);
        } else {
                gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Startup Program"));
                gtk_button_set_label (GTK_BUTTON (dialog->ok_button), _("_Save"));
                gtk_image_set_from_icon_name (GTK_IMAGE (dialog->ok_button_img), "document-save", GTK_ICON_SIZE_BUTTON);
        }

        if (dialog->name != NULL)
                gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), dialog->name);

        if (dialog->command != NULL)
                gtk_entry_set_text (GTK_ENTRY (dialog->command_entry), dialog->command);

        if (dialog->comment != NULL)
                gtk_entry_set_text (GTK_ENTRY (dialog->comment_entry), dialog->comment);

        if (dialog->delay > 0) {
                GtkAdjustment *adjustment;
                adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON(dialog->delay_spin));
                gtk_adjustment_set_value (adjustment, (gdouble) dialog->delay);
        }
}

static GObject *
gsm_app_dialog_constructor (GType                  type,
                            guint                  n_construct_app,
                            GObjectConstructParam *construct_app)
{
        GsmAppDialog *dialog;

        dialog = GSM_APP_DIALOG (G_OBJECT_CLASS (gsm_app_dialog_parent_class)->constructor (type,
                                                                                            n_construct_app,
                                                                                            construct_app));

        setup_dialog (dialog);

        return G_OBJECT (dialog);
}

static void
gsm_app_dialog_dispose (GObject *object)
{
        GsmAppDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSM_IS_APP_DIALOG (object));

        dialog = GSM_APP_DIALOG (object);

        g_free (dialog->name);
        dialog->name = NULL;
        g_free (dialog->command);
        dialog->command = NULL;
        g_free (dialog->comment);
        dialog->comment = NULL;

        G_OBJECT_CLASS (gsm_app_dialog_parent_class)->dispose (object);
}

static void
gsm_app_dialog_set_name (GsmAppDialog *dialog,
                         const char   *name)
{
        g_return_if_fail (GSM_IS_APP_DIALOG (dialog));

        g_free (dialog->name);

        dialog->name = g_strdup (name);
        g_object_notify (G_OBJECT (dialog), "name");
}

static void
gsm_app_dialog_set_command (GsmAppDialog *dialog,
                            const char   *name)
{
        g_return_if_fail (GSM_IS_APP_DIALOG (dialog));

        g_free (dialog->command);

        dialog->command = g_strdup (name);
        g_object_notify (G_OBJECT (dialog), "command");
}

static void
gsm_app_dialog_set_comment (GsmAppDialog *dialog,
                            const char   *name)
{
        g_return_if_fail (GSM_IS_APP_DIALOG (dialog));

        g_free (dialog->comment);

        dialog->comment = g_strdup (name);
        g_object_notify (G_OBJECT (dialog), "comment");
}

static void
gsm_app_dialog_set_delay (GsmAppDialog *dialog,
                          guint delay)
{
        g_return_if_fail (GSM_IS_APP_DIALOG (dialog));

        dialog->delay = delay;
        g_object_notify (G_OBJECT (dialog), "delay");
}

const char *
gsm_app_dialog_get_name (GsmAppDialog *dialog)
{
        g_return_val_if_fail (GSM_IS_APP_DIALOG (dialog), NULL);
        return gtk_entry_get_text (GTK_ENTRY (dialog->name_entry));
}

const char *
gsm_app_dialog_get_command (GsmAppDialog *dialog)
{
        g_return_val_if_fail (GSM_IS_APP_DIALOG (dialog), NULL);
        return gtk_entry_get_text (GTK_ENTRY (dialog->command_entry));
}

const char *
gsm_app_dialog_get_comment (GsmAppDialog *dialog)
{
        g_return_val_if_fail (GSM_IS_APP_DIALOG (dialog), NULL);
        return gtk_entry_get_text (GTK_ENTRY (dialog->comment_entry));
}

guint
gsm_app_dialog_get_delay (GsmAppDialog *dialog)
{
        g_return_val_if_fail (GSM_IS_APP_DIALOG (dialog), 0);
        return dialog->delay;
}

static void
gsm_app_dialog_set_property (GObject        *object,
                             guint           prop_id,
                             const GValue   *value,
                             GParamSpec     *pspec)
{
        GsmAppDialog *dialog = GSM_APP_DIALOG (object);

        switch (prop_id) {
        case PROP_NAME:
                gsm_app_dialog_set_name (dialog, g_value_get_string (value));
                break;
        case PROP_COMMAND:
                gsm_app_dialog_set_command (dialog, g_value_get_string (value));
                break;
        case PROP_COMMENT:
                gsm_app_dialog_set_comment (dialog, g_value_get_string (value));
                break;
        case PROP_DELAY:
                gsm_app_dialog_set_delay (dialog, g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_app_dialog_get_property (GObject        *object,
                             guint           prop_id,
                             GValue         *value,
                             GParamSpec     *pspec)
{
        GsmAppDialog *dialog = GSM_APP_DIALOG (object);

        switch (prop_id) {
        case PROP_NAME:
                g_value_set_string (value, dialog->name);
                break;
        case PROP_COMMAND:
                g_value_set_string (value, dialog->command);
                break;
        case PROP_COMMENT:
                g_value_set_string (value, dialog->comment);
                break;
        case PROP_DELAY:
                g_value_set_uint (value, dialog->delay);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsm_app_dialog_class_init (GsmAppDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->get_property = gsm_app_dialog_get_property;
        object_class->set_property = gsm_app_dialog_set_property;
        object_class->constructor = gsm_app_dialog_constructor;
        object_class->dispose = gsm_app_dialog_dispose;

        g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name",
                                                              "name",
                                                              "name",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_COMMAND,
                                         g_param_spec_string ("command",
                                                              "command",
                                                              "command",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_COMMENT,
                                         g_param_spec_string ("comment",
                                                              "comment",
                                                              "comment",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_DELAY,
                                         g_param_spec_uint ("delay",
                                                            "delay",
                                                            "delay",
                                                             0,
                                                             100,
                                                             0,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT));


        gtk_widget_class_set_template_from_resource (widget_class, "/org/mate/desktop/session/properties/gsm-app-dialog.ui");
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, name_entry);
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, command_entry);
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, comment_entry);
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, delay_spin);
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, ok_button);
        gtk_widget_class_bind_template_child (widget_class, GsmAppDialog, ok_button_img);
        gtk_widget_class_bind_template_callback (widget_class, on_browse_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, on_delay_spin_output);
        gtk_widget_class_bind_template_callback (widget_class, on_entry_activate);
}

static void
gsm_app_dialog_init (GsmAppDialog *dialog)
{
        gtk_widget_init_template (GTK_WIDGET (dialog));
}

GtkWidget *
gsm_app_dialog_new (const char *name,
                    const char *command,
                    const char *comment,
                    guint delay)
{
        GObject *object;

        object = g_object_new (GSM_TYPE_APP_DIALOG,
                               "name", name,
                               "command", command,
                               "comment", comment,
                               "delay", delay,
                               NULL);

        return GTK_WIDGET (object);
}

gboolean
gsm_app_dialog_run (GsmAppDialog  *dialog,
                    char         **name_p,
                    char         **command_p,
                    char         **comment_p,
                    guint         *delay_p)
{
        gboolean retval;

        retval = FALSE;

        while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
                const char *name;
                const char *exec;
                const char *comment;
                guint       delay;
                const char *error_msg;
                GError     *error;
                char      **argv;
                int         argc;

                name = gsm_app_dialog_get_name (GSM_APP_DIALOG (dialog));
                exec = gsm_app_dialog_get_command (GSM_APP_DIALOG (dialog));
                comment = gsm_app_dialog_get_comment (GSM_APP_DIALOG (dialog));
                delay = gsm_app_dialog_get_delay (GSM_APP_DIALOG (dialog));

                error = NULL;
                error_msg = NULL;

                if (gsm_util_text_is_blank (exec)) {
                        error_msg = _("The startup command cannot be empty");
                } else {
                        if (!g_shell_parse_argv (exec, &argc, &argv, &error)) {
                                if (error != NULL) {
                                        error_msg = error->message;
                                } else {
                                        error_msg = _("The startup command is not valid");
                                }
                        }
                }

                if (error_msg != NULL) {
                        GtkWidget *msgbox;

                        msgbox = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         "%s", error_msg);

                        if (error != NULL) {
                                g_error_free (error);
                        }

                        gtk_dialog_run (GTK_DIALOG (msgbox));

                        gtk_widget_destroy (msgbox);

                        continue;
                }

                if (gsm_util_text_is_blank (name)) {
                        name = argv[0];
                }

                if (name_p) {
                        *name_p = g_strdup (name);
                }

                g_strfreev (argv);

                if (command_p) {
                        *command_p = g_strdup (exec);
                }

                if (comment_p) {
                        *comment_p = g_strdup (comment);
                }

                if (delay_p) {
                        *delay_p = delay;
                }

                retval = TRUE;
                break;
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));

        return retval;
}
