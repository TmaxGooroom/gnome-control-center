/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2019 gooroom <gooroom@gooroom.kr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "cc-themes-panel.h"
#include "cc-themes-resources.h"

#include <locale.h>
#include <config.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define DEFAULT_GTK_THEME   "Default"
#define DEFAULT_WM_THEME    "Default"
#define DEFAULT_ICON_THEME  "Rodent"

#define DEFAULT_THEME_DIR   "/usr/share/icons"
#define DEFAULT_THEMES_INI  "/usr/share/gnome-control-center/themes/gooroom-themes.ini"

#define WID(y) (GtkWidget *) gtk_builder_get_object (panel->builder, y)

enum
{
  COL_THUMBNAIL,
  COL_NAME,
  COL_ICON,
  COL_BACKGROUND,
  COL_THUMB_PATH,
  NUM_COLS
};

struct _CcThemesPanel
{
  CcPanel           parent_instance;

  GtkBuilder        *builder;

  GSettings         *interface_settings;
  GSettings         *bg_settings;
  GSettings         *screensaver_settings;

  GtkIconView       *icon_view;
  GtkListStore      *themes_liststore;
  GtkWidget         *current_theme_label;

  GdkPixbuf         *current_theme_thumbnail;
  gchar             *icon;
  gchar             *background;
};

CC_PANEL_REGISTER (CcThemesPanel, cc_themes_panel)

static const char *
cc_themes_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/themes";
}

static void
set_thumbnail (CcThemesPanel *panel, gchar *thumb_path)
{
  GdkPixbuf *old_pixbuf;
  old_pixbuf = panel->current_theme_thumbnail;
  gint width;
  gint height;

  if (panel->current_theme_thumbnail != NULL)
  {
    g_clear_object (&panel->current_theme_thumbnail);
    panel->current_theme_thumbnail = NULL;
  }

  panel->current_theme_thumbnail = gdk_pixbuf_new_from_file (thumb_path, NULL);
}

static GtkListStore*
create_store (void)
{
  GtkListStore *store;

  store = gtk_list_store_new (NUM_COLS,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING);
  return store;
}

static void
set_theme_liststore (GtkListStore *store, GKeyFile *keyfile, gchar *group)
{
  gchar *name;
  gchar *background;
  gchar *icon;
  gchar *thumb_path;
  g_autoptr(GError) error = NULL;
  GtkTreeIter iter;
  

  name = g_key_file_get_locale_string (keyfile, group, "Name", NULL, NULL);
  background = g_key_file_get_value (keyfile, group, "Background", NULL);
  icon = g_key_file_get_value (keyfile, group, "Icon", &error);
  if (icon == NULL) {
    g_warning ("Error icon:%s", error->message);
    return;
  }

  thumb_path = g_strdup_printf ("%s/%s/thumbnail.png", DEFAULT_THEME_DIR, icon);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      COL_THUMBNAIL, gdk_pixbuf_new_from_file_at_size (thumb_path, 180, 110, NULL),
                      COL_NAME, name ? name : NULL,
                      COL_ICON, icon ? icon : NULL,
                      COL_BACKGROUND, background ? background : NULL,
                      COL_THUMB_PATH, thumb_path, -1);
}

static GtkListStore *
load_themes_liststore (CcThemesPanel *panel)
{
  GtkListStore *store;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  gchar **groups = NULL;
  gsize group_length;

  if (!g_key_file_load_from_file (keyfile, DEFAULT_THEMES_INI, G_KEY_FILE_KEEP_TRANSLATIONS, &error))
  {
    g_warning ("Error loading default-themes.ini: %s", error->message);
    return NULL;
  }

  store = create_store ();

  groups = g_key_file_get_groups (keyfile, &group_length);
  for (int i = 0; i < group_length; i++)
  {
    set_theme_liststore (store, keyfile, groups[i]);
  }

  return store;
}

static gboolean
cc_themes_panel_draw_theme (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  GdkPixbuf *pixbuf;
  CcThemesPanel *panel = CC_THEMES_PANEL (data);
  cairo_t *_cr;
  GdkRGBA color;

  color.red = 1.0;
  color.green = 1.0;
  color.blue = 1.0;
  color.alpha = 1.0;

  _cr = gdk_cairo_create (gtk_widget_get_window (widget));

  /* border color */
  gdk_cairo_set_source_rgba (_cr, &color);

  if (panel->current_theme_thumbnail)
  {
    pixbuf = gdk_pixbuf_scale_simple (panel->current_theme_thumbnail, 302, 181, GDK_INTERP_BILINEAR);
    gdk_cairo_set_source_pixbuf (_cr, pixbuf, 20, 22);
  }

  cairo_paint (_cr);
  cairo_destroy (_cr);

  return FALSE;
}

static void 
cc_themes_panel_selected_theme (GtkIconView *icon_view,
                                CcThemesPanel *panel)
{
  GList *list;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *icon_name;
  gchar *background;
  gchar *thumb_path;

  list = gtk_icon_view_get_selected_items (icon_view);
  if (list == NULL)
    return;

  model = gtk_icon_view_get_model (icon_view);

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*)list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (model, &iter,
                      COL_ICON, &icon_name,
                      COL_BACKGROUND, &background,
                      COL_THUMB_PATH, &thumb_path, -1);

  set_thumbnail (panel, thumb_path);

  g_settings_set_string (panel->interface_settings, "icon-theme", icon_name);
  g_settings_set_string (panel->bg_settings, "picture-uri", background);

bail:
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

}

static gboolean
cc_themes_panel_apply_theme (GtkButton *button, CcThemesPanel *panel)
{
  GList *list;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *icon;
  gchar *icon_name;
  gchar *background;

  list = gtk_icon_view_get_selected_items (panel->icon_view);
  if (list == NULL)
    return FALSE;

  model = gtk_icon_view_get_model (panel->icon_view);
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, (GtkTreePath*)list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
                      COL_NAME, &icon_name,
                      COL_ICON, &icon,
                      COL_BACKGROUND, &background, -1);

  panel->icon = icon;
  panel->background = background;

  g_settings_set_string (panel->screensaver_settings, "picture-uri", background);

  //gtk_label_set_text (WID ("current-theme-label"), icon_name);
  gtk_label_set_text (GTK_LABEL (panel->current_theme_label), icon_name);

bail:
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

return FALSE;
}

static void
cc_themes_panel_constructed (GObject *object)
{
  CcThemesPanel *panel = CC_THEMES_PANEL (object);
  CcShell *shell;
  GtkWidget *button;

  G_OBJECT_CLASS (cc_themes_panel_parent_class)->constructed (object);

  button = (GtkWidget *)gtk_button_new_with_mnemonic (_("_Apply"));
  gtk_widget_set_visible (button, TRUE);

  shell = cc_panel_get_shell (CC_PANEL (panel));
  cc_shell_embed_widget_in_header (shell, button, GTK_POS_RIGHT);

  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (cc_themes_panel_apply_theme),
                    panel);
}

static void
cc_themes_panel_dispose (GObject *object)
{
  CcThemesPanel *panel = CC_THEMES_PANEL (object);

  if (panel->interface_settings)
    g_settings_set_string (panel->interface_settings, "icon-theme", panel->icon);
  if (panel->bg_settings)
    g_settings_set_string (panel->bg_settings, "picture-uri", panel->background);

  g_clear_object (&panel->interface_settings);
  g_clear_object (&panel->bg_settings);
  g_clear_object (&panel->screensaver_settings);
  g_clear_object (&panel->builder);

  g_clear_object (&panel->current_theme_thumbnail);

  G_OBJECT_CLASS (cc_themes_panel_parent_class)->dispose (object);
}

static void
cc_themes_panel_class_init (CcThemesPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->constructed = cc_themes_panel_constructed;
  object_class->dispose = cc_themes_panel_dispose;

  panel_class->get_help_uri = cc_themes_panel_get_help_uri;
}

static void
cc_themes_panel_init (CcThemesPanel *panel)
{
  gchar *objects[] = {"themes-main-scrolled-window", NULL};
  g_autoptr(GError) err = NULL;
  GtkWidget *w;
  gint button_val;
  gdouble i_val;
  GtkWidget *themes_list_sw;
  GtkTreeIter iter;
  gchar *icon;
  gchar *icon_name;
  gchar *thumb_path;
  gboolean ret = FALSE;

  panel->current_theme_thumbnail = NULL;

  g_resources_register (cc_themes_get_resource ());

  panel->builder = gtk_builder_new ();
  gtk_builder_add_objects_from_resource (panel->builder,
                                         "/org/gnome/control-center/themes/themes.ui",
                                         objects, &err);

  panel->current_theme_label = WID ("current-theme-label");

  if (err)
  {
    g_warning ("Could not load ui: %s", err->message);
    return;
  }

  panel->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  panel->bg_settings = g_settings_new ("org.gnome.desktop.background");
  panel->screensaver_settings = g_settings_new ("org.gnome.desktop.screensaver");

  panel->icon = g_settings_get_string (panel->interface_settings, "icon-theme");
  panel->background = g_settings_get_string (panel->bg_settings, "picture-uri");

  panel->themes_liststore = load_themes_liststore (panel);

  /* init current thumbnail and current label */
  ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (panel->themes_liststore), &iter);
  do
  {
    gtk_tree_model_get (GTK_TREE_MODEL (panel->themes_liststore), &iter,
                        COL_NAME, &icon_name,
                        COL_ICON, &icon,
                        COL_THUMB_PATH, &thumb_path, -1);
  
    if (g_strcmp0 (panel->icon, icon) == 0)
    {
      set_thumbnail (panel, thumb_path); // /usr/.../.png
      //gtk_label_set_text (WID ("current-theme-label"), icon_name);
      gtk_label_set_text (GTK_LABEL (panel->current_theme_label), icon_name);

      break;
    }
  
  }
  while (ret && gtk_tree_model_iter_next (GTK_TREE_MODEL (panel->themes_liststore), &iter));

  /* create icon view from list store */
  panel->icon_view = GTK_ICON_VIEW (gtk_icon_view_new_with_model (GTK_TREE_MODEL (panel->themes_liststore)));
  gtk_icon_view_set_selection_mode (panel->icon_view,
                                    GTK_SELECTION_SINGLE);

  gtk_icon_view_set_text_column (panel->icon_view, COL_NAME);

  gtk_icon_view_set_pixbuf_column (panel->icon_view, COL_THUMBNAIL);
  gtk_icon_view_set_columns (panel->icon_view, 3);
  gtk_icon_view_set_item_width (panel->icon_view, 120);
  gtk_icon_view_set_column_spacing (panel->icon_view, 10);
  gtk_icon_view_set_margin (panel->icon_view, 10);

  g_signal_connect (WID ("current-theme-drawingarea"),
                    "draw",
                    G_CALLBACK(cc_themes_panel_draw_theme),
                    panel);
  g_signal_connect (panel->icon_view,
                    "selection-changed",
                    G_CALLBACK (cc_themes_panel_selected_theme),
                    panel);

  themes_list_sw = WID ("themes-list-sw");

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (themes_list_sw), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (themes_list_sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (themes_list_sw), GTK_WIDGET (panel->icon_view));
  //gtk_widget_grab_focus (panel->icon_view);

  w = WID ("themes-main-scrolled-window");

  gtk_container_add (GTK_CONTAINER (panel), w);
  gtk_widget_show_all (GTK_WIDGET (panel));
}
