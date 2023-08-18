#include "config.h"
#include <glib.h>
#include <glib/gi18n.h>

#include "msm-desktop-app-dialog.h"

struct _MsmDesktopAppDialog
{
    GtkDialog  parent;

    GtkWidget  *entry;
    GtkWidget  *listbox;
};

G_DEFINE_TYPE (MsmDesktopAppDialog, msm_desktop_app_dialog, GTK_TYPE_DIALOG)

static void
on_search_entry_changed (GtkSearchEntry      *entry,
                         MsmDesktopAppDialog *dialog)
{
    GtkListBoxRow *selected;

    gtk_list_box_invalidate_filter (GTK_LIST_BOX (dialog->listbox));
    selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->listbox));

    if (selected != NULL && gtk_widget_get_mapped (GTK_WIDGET (selected)))
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
    }
    else
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
    }
}

static gint
list_sort_app_func (GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data)
{
    GAppInfo *app1, *app2;

    app1 = g_object_get_data (G_OBJECT (row1), "appinfo");
    app2 = g_object_get_data (G_OBJECT (row2), "appinfo");

    return g_utf8_collate (g_app_info_get_name (app1), g_app_info_get_name (app2));
}

static void
list_header_app_func (GtkListBoxRow *row, GtkListBoxRow *before, gpointer user_data)
{

    if (before != NULL && (gtk_list_box_row_get_header (row) == NULL))
    {
        gtk_list_box_row_set_header (row,
                                     gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
    }
}

static gboolean
list_filter_app_func (GtkListBoxRow *row,
                      gpointer       user_data)
{
    MsmDesktopAppDialog *dialog = MSM_DESKTOP_APP_DIALOG (user_data);
    GAppInfo *app;
    GString  *s1, *s2;

    app = g_object_get_data (G_OBJECT (row), "appinfo");

    s1 = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->entry)));
    s2 = g_string_new (g_app_info_get_name (app));

    g_string_ascii_up (s1);
    g_string_ascii_up (s2);

    if (strstr (s2->str, s1->str) != NULL)
        return TRUE;

    return FALSE;
}

static void
on_row_selected (GtkListBox          *box,
                 GtkListBoxRow       *row,
                 MsmDesktopAppDialog *dialog)
{
    if (row != NULL && gtk_widget_get_mapped (GTK_WIDGET (row)))
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
    }
    else
    {
        gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
    }
}

static void
setup_dialog (MsmDesktopAppDialog *dialog)
{
    GtkStyleContext *context;
    GtkWidget   *content_area;
    GtkWidget   *searchbar;
    GtkWidget   *searchbutton;
    GtkWidget   *searchimage;
    GtkWidget   *headerbar;
    GtkWidget   *sw;

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (content_area), 6);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (dialog), 600, 400);
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    dialog->entry = gtk_search_entry_new ();
    g_signal_connect (dialog->entry,
                     "search-changed",
                      G_CALLBACK (on_search_entry_changed),
                      dialog);

    gtk_entry_set_placeholder_text (GTK_ENTRY (dialog->entry), _("Search Applications…"));
    gtk_entry_set_width_chars (GTK_ENTRY (dialog->entry), 30);
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->entry), FALSE);
    gtk_entry_set_input_hints (GTK_ENTRY (dialog->entry), GTK_INPUT_HINT_NO_EMOJI);

    searchbar = gtk_search_bar_new ();
    gtk_container_add (GTK_CONTAINER (searchbar), dialog->entry);
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (searchbar), TRUE);
    gtk_widget_set_hexpand (searchbar, FALSE);
    gtk_box_pack_start (GTK_BOX (content_area), searchbar, FALSE, FALSE, 0);

    dialog->listbox = gtk_list_box_new ();
    gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (dialog->listbox), FALSE);
    gtk_list_box_set_sort_func (GTK_LIST_BOX (dialog->listbox), list_sort_app_func, dialog, NULL);
    gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->listbox), list_header_app_func, dialog, NULL);
    gtk_list_box_set_filter_func (GTK_LIST_BOX (dialog->listbox), list_filter_app_func, dialog, NULL);
    g_signal_connect (dialog->listbox,
                     "row-selected",
                      G_CALLBACK (on_row_selected),
                      dialog);

    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Close"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Add"), GTK_RESPONSE_OK);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (sw), dialog->listbox);
    gtk_box_pack_start (GTK_BOX (content_area), sw, TRUE, TRUE, 0);

    searchbutton = gtk_toggle_button_new ();
    gtk_widget_set_valign (searchbutton, GTK_ALIGN_CENTER);
    searchimage = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
    gtk_container_add (GTK_CONTAINER (searchbutton), searchimage);

    context = gtk_widget_get_style_context (searchbutton);
    gtk_style_context_add_class (context, "image-button");
    gtk_style_context_remove_class (context, "text-button");

    headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
    gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), searchbutton);
    g_object_bind_property (searchbutton, "active", searchbar, "search-mode-enabled", G_BINDING_BIDIRECTIONAL);
}

static GObject *
msm_desktop_app_dialog_constructor (GType                  type,
                                    guint                  n_construct_app,
                                    GObjectConstructParam *construct_app)
{
    MsmDesktopAppDialog *dialog;

    dialog = MSM_DESKTOP_APP_DIALOG (G_OBJECT_CLASS (msm_desktop_app_dialog_parent_class)->constructor (type,
                                                                                            n_construct_app,
                                                                                            construct_app));

    setup_dialog (dialog);

    return G_OBJECT (dialog);
}

static void
msm_desktop_app_dialog_dispose (GObject *object)
{
    g_return_if_fail (object != NULL);
    g_return_if_fail (MSM_IS_DESKTOP_APP_DIALOG (object));

    G_OBJECT_CLASS (msm_desktop_app_dialog_parent_class)->dispose (object);
}

static void
msm_desktop_app_dialog_class_init (MsmDesktopAppDialogClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->constructor = msm_desktop_app_dialog_constructor;
    object_class->dispose = msm_desktop_app_dialog_dispose;
}

static void
msm_desktop_app_dialog_init (MsmDesktopAppDialog *dialog)
{

}

static GtkWidget *
create_app_info_row (GAppInfo *app)
{
    GtkWidget  *row;
    GtkWidget  *grid;
    GtkWidget  *image = NULL;
    GtkWidget  *label, *label2;
    gint        height;
    const char *app_name;
    const char *app_summary;
    GIcon      *app_icon;

    row = gtk_list_box_row_new ();
    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);

    app_name = g_app_info_get_name (app);
    app_icon = g_app_info_get_icon (app);
    app_summary = g_app_info_get_description (app);

    if (app_name == NULL)
        return NULL;

    if (app_icon != NULL)
    {
        image = gtk_image_new_from_gicon (app_icon, GTK_ICON_SIZE_DIALOG);
        gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, NULL, &height);
        gtk_image_set_pixel_size (GTK_IMAGE (image), height);
        gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 2, 2);
        gtk_widget_set_hexpand (image, FALSE);
    }

    label = gtk_label_new (app_name);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_vexpand (label, FALSE);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_grid_attach_next_to (GTK_GRID (grid), label, image, GTK_POS_RIGHT, 1, 1);

    label2 = gtk_label_new (app_summary);
    gtk_label_set_ellipsize (GTK_LABEL (label2), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (label2), 50);
    gtk_widget_set_hexpand (label2, TRUE);
    gtk_widget_set_halign (label2, GTK_ALIGN_START);
    gtk_widget_set_vexpand (label2, FALSE);
    gtk_widget_set_valign (label2, GTK_ALIGN_CENTER);
    gtk_grid_attach_next_to (GTK_GRID (grid), label2, label, GTK_POS_BOTTOM, 1, 1);

    gtk_container_add (GTK_CONTAINER (row), grid);
    g_object_set_data (G_OBJECT (row), "appinfo", app);

    return row;
}

GtkWidget *
msm_desktop_app_dialog_new (GspAppManager *manager)
{
    MsmDesktopAppDialog *dialog;
    gboolean   use_header;
    GList     *apps, *l;

    g_object_get (gtk_settings_get_default (),
                 "gtk-dialogs-use-header", &use_header,
                  NULL);

    dialog = g_object_new (MSM_TYPE_DESKTOP_APP_DIALOG,
                           "use-header-bar", use_header,
                           NULL);

    apps = g_app_info_get_all ();
    for (l = apps; l != NULL; l = l->next)
    {
        GAppInfo   *info = l->data;
        GtkWidget  *row;
        const char *id;

        id = g_app_info_get_id (info);
        if (id != NULL && gsp_app_manager_find_app_with_basename (manager, id) == NULL)
        {
            if (g_app_info_should_show (info))
            {
                row = create_app_info_row (info);
                if (row != NULL)
                {
                    gtk_list_box_prepend (GTK_LIST_BOX (dialog->listbox), row);
                }
            }
        }
    }

    return GTK_WIDGET (dialog);
}

const char *
msm_dektop_app_dialog_get_selected (MsmDesktopAppDialog *dialog)
{
    GtkListBoxRow *selected = NULL;
    GAppInfo *app;

    selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->listbox));
    if (selected != NULL)
    {
        app = g_object_get_data (G_OBJECT (selected), "appinfo");
        return g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app));
    }

    return NULL;
}
