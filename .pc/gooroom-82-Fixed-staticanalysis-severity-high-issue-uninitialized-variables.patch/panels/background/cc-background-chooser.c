#include <config.h>

#include <glib/gi18n-lib.h>

#include "cc-background-chooser.h"
#include "cc-background-item.h"
#include "bg-wallpapers-source.h"
#include "bg-recent-source.h"
#include "bg-colors-source.h"

#define COLOR_CHECK_ICON "/org/gnome/control-center/background/color_check_icon"
#define CHECK_ICON       "/org/gnome/control-center/background/check_icon"


typedef enum {
  IMAGE_WALLPAPER = 0,
  IMAGE_SINGLECOLOR,
}BackgroundImage;

struct _CcBackgroundChooser
{
  GtkBox        parent;

  GtkWidget     *background_select_combobox;
  GtkStack      *background_stack;
  GtkWidget     *chooser_button;

  GtkFlowBox    *wallpaper_flowbox;
  GtkFlowBox    *recent_flowbox;
  GtkFlowBox    *color_flowbox;

  GtkWidget     *selected_icon;

  BackgroundImage      selected_background;

  BgWallpapersSource  *wallpapers_source;
  BgRecentSource      *recent_source;
  BgColorsSource      *colors_source;
};

G_DEFINE_TYPE (CcBackgroundChooser, cc_background_chooser, GTK_TYPE_BOX)

enum
{
  SELECT_BACKGROUND,
  N_SIGNALS,
};

static guint signals [N_SIGNALS];

static void
on_delete_background_clicked_cb (GtkButton *button,
                                 BgRecentSource  *source)
{
  GtkWidget *parent;
  CcBackgroundItem *item;

  parent = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (button)));
  g_assert (GTK_IS_FLOW_BOX_CHILD (parent));

  item = g_object_get_data (G_OBJECT (parent), "item");

  bg_recent_source_remove_item (source, item);
}

static gboolean
run_warning (GtkWindow *parent, char *prompt, char *text, char *button_title)
{
  GtkWidget *dialog;
  GtkWidget *button;
  int result;
  dialog = gtk_message_dialog_new (parent,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   NULL);
  g_object_set (dialog,
                "text", prompt,
                "secondary-text", text,
                NULL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, GTK_RESPONSE_OK);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), FALSE);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return result == GTK_RESPONSE_OK;
}

static void
on_file_dialog_response_cb (GtkDialog           *dialog,
                            gint                 response,
                            CcBackgroundChooser *chooser)
{
  gboolean result;

  if (response == GTK_RESPONSE_ACCEPT) {
    g_autofree gchar *filename = NULL;
    GListStore *store;
    guint i, j;
    g_autofree gchar **filename_split = NULL;
    g_autofree gchar *image_name = NULL;
    guint filename_split_length;
  
    result = response == GTK_RESPONSE_ACCEPT;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    filename_split = g_strsplit (filename, "/", -1);
    filename_split_length = g_strv_length (filename_split) - 1;
    image_name = filename_split[filename_split_length];
  
    store = bg_source_get_liststore (BG_SOURCE (chooser->recent_source));
    
    for ( i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++) {

      g_autoptr(CcBackgroundItem) tmp_item = NULL;
      g_autofree gchar **tmp_filename_split = NULL;
      g_autofree gchar **tmp_image_name_split = NULL;
      g_autofree gchar **tmp_image_name_split_without_date = NULL;
      g_autofree gchar *tmp_filename = NULL;
      g_autofree gchar *tmp_image_name = NULL;
      guint tmp_filename_split_length;
      guint tmp_image_name_split_length;
  
      tmp_item = g_list_model_get_item (G_LIST_MODEL (store), i);
      tmp_filename_split = g_strsplit (cc_background_item_get_uri (tmp_item), "/", -1);
      tmp_filename_split_length = g_strv_length (tmp_filename_split) - 1;
      tmp_filename = tmp_filename_split[tmp_filename_split_length];

      tmp_image_name_split = g_strsplit (tmp_filename, "-", -1);
      tmp_image_name_split_length = g_strv_length (tmp_image_name_split) - 1;
      
      tmp_image_name_split_without_date = g_strdupv (&tmp_image_name_split[6]);
      tmp_image_name = g_strjoinv (g_strdup("-"), tmp_image_name_split_without_date);

      if (g_strcmp0 (image_name, tmp_image_name) == 0) {

        result = run_warning (dialog, _("Add the selected image file?"),
                              _("The file which has same name already exists."),
                              _("_Add"));

        if (result) 
          bg_recent_source_remove_item (chooser->recent_source, tmp_item);
        
        break;
      }
    }

    if (result)
      bg_recent_source_add_file (chooser->recent_source, filename);
  }
 
  if (result || response != GTK_RESPONSE_ACCEPT) 
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_background_item_activated (GtkFlowBox          *flowbox,
                   GtkFlowBoxChild     *child,
                   CcBackgroundChooser *chooser)
{
  g_autoptr(GList) list = NULL;
  GtkFlowBox *current_flowbox;
  GtkWidget *selected_icon;
  CcBackgroundItem *item;

  if (flowbox == chooser->wallpaper_flowbox) {
    current_flowbox = chooser->wallpaper_flowbox;

    gtk_flow_box_unselect_all (chooser->recent_flowbox);
    gtk_flow_box_unselect_all (chooser->color_flowbox);
  }
  else if (flowbox == chooser->recent_flowbox) {
    current_flowbox = chooser->recent_flowbox;

    gtk_flow_box_unselect_all (chooser->wallpaper_flowbox);
    gtk_flow_box_unselect_all (chooser->color_flowbox);
  }
  else {
    current_flowbox = chooser->color_flowbox;

    gtk_flow_box_unselect_all (chooser->wallpaper_flowbox);
    gtk_flow_box_unselect_all (chooser->recent_flowbox);
  }

  list = gtk_flow_box_get_selected_children (current_flowbox);
  selected_icon = g_object_get_data (list->data, "selected-icon");

  if (chooser->selected_icon)
    gtk_widget_set_visible (chooser->selected_icon, FALSE);
  gtk_widget_set_visible (selected_icon, TRUE);

  chooser->selected_icon = selected_icon;

  /* set item.. */
  item = g_object_get_data (list->data, "item");
  g_signal_emit (chooser, signals[SELECT_BACKGROUND], 0, item);
}

static void
on_background_changed (GtkComboBox         *combobox,
                       CcBackgroundChooser *chooser)
{
  gint selected_background = gtk_combo_box_get_active (combobox);

  if (selected_background == IMAGE_WALLPAPER) {
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->background_stack), "wallpaper");
    gtk_widget_set_visible (chooser->chooser_button, TRUE);
  }
  else if (selected_background == IMAGE_SINGLECOLOR) {
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->background_stack), "single-color");
    gtk_widget_set_visible (chooser->chooser_button, FALSE);
  }
}

static void
on_clicked_add_image (GtkButton           *button,
                      CcBackgroundChooser *chooser)
{
  GtkWidget *dlg;
  GtkWidget *dialog;
  GtkFileFilter *filter;

  dialog = gtk_file_chooser_dialog_new (_("Add Image"),
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                      g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));

  g_signal_connect_object (dialog,
                         "response",
                         G_CALLBACK (on_file_dialog_response_cb),
                         chooser,
                         0);

  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget*
create_flowbox_child (gpointer model_item,
                      gpointer data)
{
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  CcBackgroundItem *item;
  GtkWidget *overlay;
  GtkWidget *child;
  GtkWidget *image, *selected_icon;
  BgSource  *source;
  GtkWidget *button_image = NULL;
  GtkWidget *button = NULL;

  source = BG_SOURCE (data);
  item = CC_BACKGROUND_ITEM (model_item);
  pixbuf = cc_background_item_get_thumbnail (item,
                                             bg_source_get_thumbnail_factory (source),
                                             bg_source_get_thumbnail_width (source),
                                             bg_source_get_thumbnail_height (source),
                                             bg_source_get_scale_factor (source));

  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_widget_show (image);

  /* setup single-color selected image */
  if (BG_IS_COLORS_SOURCE (source)) {
    selected_icon = g_object_new (GTK_TYPE_IMAGE,
                                  "resource", COLOR_CHECK_ICON,
                                  "pixel-size", 24,
                                  "margin", 12,
                                  "halign", GTK_ALIGN_CENTER,
                                  "valign", GTK_ALIGN_CENTER,
                                  "visible", FALSE, NULL);
  }
  /* wallpaper or recent source */
  else {
    selected_icon = g_object_new (GTK_TYPE_IMAGE,
                                  "resource", CHECK_ICON,
                                  "pixel-size", 32,
                                  "halign", GTK_ALIGN_END,
                                  "valign", GTK_ALIGN_START,
                                  "visible", FALSE, NULL);

    //if (BG_IS_RECENT_SOURCE (source)) {
    //  button_image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    //  button = g_object_new (GTK_TYPE_BUTTON,
    //                         "image", button_image,
    //                         "halign", GTK_ALIGN_END,
    //                         "valign", GTK_ALIGN_START,
    //                         "margin", 6,
    //                         "visible",TRUE, NULL);

    //  //gtk_style_context_add_class (gtk_widget_get_style_context (button), "osd");
    //  //gtk_style_context_add_class (gtk_widget_get_style_context (button), "remove-button");

    //  g_signal_connect (button,
    //                    "clicked",
    //                    G_CALLBACK (on_delete_background_clicked_cb),
    //                    source);
    //}
  }

  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "halign", GTK_ALIGN_CENTER,
                        "valign", GTK_ALIGN_CENTER, NULL);

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (overlay), image);
  gtk_overlay_add_overlay (GTK_OVERLAY(overlay), selected_icon);
  if (button)
    gtk_overlay_add_overlay (GTK_OVERLAY(overlay), button);
  gtk_widget_show (overlay);

  gtk_container_add (GTK_CONTAINER (child), overlay);
  gtk_widget_show (child);

  g_object_set_data_full (G_OBJECT (child), "item", g_object_ref (item), g_object_unref);
  g_object_set_data_full (G_OBJECT (child), "selected-icon", g_object_ref (selected_icon),
                          g_object_unref);

  return child;
}

static void
setup_flowbox (CcBackgroundChooser *chooser)
{
  GListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (chooser->colors_source));
  gtk_flow_box_bind_model (chooser->color_flowbox,
                           G_LIST_MODEL (store),
                           create_flowbox_child,
                           chooser->colors_source, NULL);

  store = bg_source_get_liststore (BG_SOURCE (chooser->recent_source));
  gtk_flow_box_bind_model (chooser->recent_flowbox,
                           G_LIST_MODEL (store),
                           create_flowbox_child,
                           chooser->recent_source, NULL);

  store = bg_source_get_liststore (BG_SOURCE (chooser->wallpapers_source));
  gtk_flow_box_bind_model (chooser->wallpaper_flowbox,
                           G_LIST_MODEL (store),
                           create_flowbox_child,
                           chooser->wallpapers_source, NULL);
}

static void
cc_background_chooser_constructed (GObject *object)
{
  CcBackgroundChooser *chooser = CC_BACKGROUND_CHOOSER (object);

  G_OBJECT_CLASS (cc_background_chooser_parent_class)->constructed (object);

  gtk_combo_box_set_active (GTK_COMBO_BOX (chooser->background_select_combobox), IMAGE_WALLPAPER);
}

static void
cc_background_chooser_finalize (GObject *object)
{
  CcBackgroundChooser *chooser = CC_BACKGROUND_CHOOSER (object);

  g_clear_object (&chooser->wallpapers_source);
  g_clear_object (&chooser->recent_source);
  g_clear_object (&chooser->colors_source);

  G_OBJECT_CLASS (cc_background_chooser_parent_class)->finalize (object);
}

static void
cc_background_chooser_class_init (CcBackgroundChooserClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);;

  object_class->constructed = cc_background_chooser_constructed;
  object_class->finalize = cc_background_chooser_finalize;

  signals[SELECT_BACKGROUND] = g_signal_new ("select-background",
                                             CC_TYPE_BACKGROUND_CHOOSER,
                                             G_SIGNAL_RUN_FIRST,
                                             0, NULL, NULL, NULL,
                                             G_TYPE_NONE,
                                             1,
                                             CC_TYPE_BACKGROUND_ITEM);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/background-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, background_select_combobox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, background_stack);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, wallpaper_flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, recent_flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, color_flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, chooser_button);

  gtk_widget_class_bind_template_callback (widget_class, on_background_item_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_background_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_clicked_add_image);
}

static void
cc_background_chooser_init (CcBackgroundChooser *chooser)
{
  gtk_widget_init_template (GTK_WIDGET (chooser));

  chooser->selected_icon = NULL;
  chooser->wallpapers_source = bg_wallpapers_source_new (GTK_WIDGET (chooser));
  chooser->recent_source = bg_recent_source_new (GTK_WIDGET (chooser));
  chooser->colors_source = bg_colors_source_new (GTK_WIDGET (chooser));

  setup_flowbox (chooser);
}
