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

#include <config.h>
#include <gtk/gtk.h>

#include <locale.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "cc-themes-resources.h"
#include "cc-themes-panel.h"
#include "cc-themes-theme.h"

#define DEFAULT_GTK_THEME   "Default"
#define DEFAULT_WM_THEME    "Default"
#define DEFAULT_ICON_THEME  "Rodent"

#define DEFAULT_THEME_DIR    "/usr/share/icons"
#define DEFAULT_THEMES_INI   "/usr/share/gnome-control-center/themes/gooroom-themes.ini"
#define ROW_LENGTH            2

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

  GtkWidget         *themes_box;

  CcThemesTheme     *current_theme;

  GSettings         *interface_settings;
  GSettings         *bg_settings;
  GSettings         *screensaver_settings;
};

CC_PANEL_REGISTER (CcThemesPanel, cc_themes_panel)

static const char *
cc_themes_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/themes";
}

static void
cc_themes_panel_selected_theme (CcThemesTheme *theme,
                                GdkEvent *event,
                                CcThemesPanel *panel)
{
  gchar *icon;
  gchar *background;

  if (panel->current_theme == theme)
    return;

  g_object_get (G_OBJECT (theme), "icon", &icon, NULL);
  g_object_get (G_OBJECT (theme), "background", &background, NULL);

  g_settings_set_string (panel->bg_settings, "picture-uri", background);
  g_settings_set_string (panel->screensaver_settings, "picture-uri", background);
  g_settings_set_string (panel->interface_settings, "icon-theme", icon);

  if (panel->current_theme)
    g_object_set (G_OBJECT (panel->current_theme), "active", FALSE, NULL);

  g_object_set (G_OBJECT (theme), "active", TRUE, NULL);

  panel->current_theme = theme;
}

static CcThemesTheme*
create_theme (CcThemesPanel *panel, GKeyFile *keyfile, gchar *group)
{
  gchar *name;
  gchar *background;
  gchar *icon;
  gchar *thumbnail;
  g_autoptr(GError) error = NULL;
  GtkTreeIter iter;
  CcThemesTheme *theme;

  name = g_key_file_get_locale_string (keyfile, group, "Name", NULL, &error);
  if (error) {
    g_warning ("Empty Name in ini file: %s", error->message);
    return NULL;
  }

  background = g_key_file_get_value (keyfile, group, "Background", &error);
  if (error) {
    g_warning ("Empty Background in ini file: %s", error->message);
    return NULL;
  }

  icon = g_key_file_get_value (keyfile, group, "Icon", &error);
  if (error) {
    g_warning ("Empty Icon in ini file: %s", error->message);
    return NULL;
  }

  thumbnail = g_strdup_printf ("%s/%s/thumbnail.png", DEFAULT_THEME_DIR, icon);

  theme = cc_themes_theme_new ();
  g_object_set (G_OBJECT (theme),
                "label-text", name,
                "icon", icon,
                "background", background,
                "thumbnail", thumbnail, NULL);

  g_signal_connect (theme, "button-press-event", G_CALLBACK (cc_themes_panel_selected_theme), panel);

  return theme;
}

static GtkWidget *
create_child_box ()
{
  GtkWidget *child_box;

  child_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (child_box, GTK_ALIGN_START);

  gtk_widget_show (child_box);

  return child_box;
}

static void
load_themes (CcThemesPanel *panel)
{
  GtkWidget *child_box;
  g_autofree gchar *icon;
  gchar **groups = NULL;
  gsize group_length;
  gint row = ROW_LENGTH;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();

  if (!g_key_file_load_from_file (keyfile, DEFAULT_THEMES_INI, G_KEY_FILE_KEEP_TRANSLATIONS, &error))
  {
    g_warning ("Error loading default-themes.ini: %s", error->message);
    return;
  }

  icon = g_settings_get_string (panel->interface_settings, "icon-theme");

  child_box = create_child_box ();
  groups = g_key_file_get_groups (keyfile, &group_length);
  for (int i = 1; i <= group_length; i++)
  {
    CcThemesTheme *theme= create_theme (panel, keyfile, groups[i - 1]);
    if (!theme)
      continue;

    gtk_box_pack_start (GTK_BOX (child_box), GTK_WIDGET (theme), TRUE, FALSE, 0);
    if (i %  row == 0) {
      gtk_box_pack_start (GTK_BOX (panel->themes_box), child_box, TRUE, FALSE, 0);

      child_box = create_child_box ();
      gtk_widget_set_margin_top (child_box, 20);
    }

    if (g_strcmp0 (icon, cc_themes_theme_get_icon (theme)) == 0) {
      g_object_set (G_OBJECT (theme), "active", TRUE, NULL);
      panel->current_theme = theme;
    }
  }

  gtk_box_pack_start (GTK_BOX (panel->themes_box), child_box, TRUE, FALSE, 0);

  g_strfreev (groups);
}

static GtkWidget *
theme_item_widget_new (CcThemesPanel *panel, gchar *name, gchar *path)
{
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GdkPixbuf *pixbuf;
  g_autoptr(GError)  error = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_size (path, 300, 188, &error);
  if (error) {
    g_warning ("Error pixbuf new: %s", error->message);
    return NULL;
  }

  image = gtk_image_new_from_pixbuf (pixbuf);
  label = gtk_label_new (name);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

  gtk_box_pack_start (GTK_BOX (box), image, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

  return box;
}

static void
cc_themes_panel_dispose (GObject *object)
{
  CcThemesPanel *panel = CC_THEMES_PANEL (object);

  g_clear_object (&panel->interface_settings);
  g_clear_object (&panel->bg_settings);
  g_clear_object (&panel->screensaver_settings);

  G_OBJECT_CLASS (cc_themes_panel_parent_class)->dispose (object);
}

static void
cc_themes_panel_constructed (GObject *object)
{
  CcThemesPanel *panel = CC_THEMES_PANEL (object);

  load_themes (panel);
}

static void
cc_themes_panel_class_init (CcThemesPanelClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass   *panel_class  = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  panel_class->get_help_uri = cc_themes_panel_get_help_uri;

  object_class->dispose = cc_themes_panel_dispose;
  object_class->constructed = cc_themes_panel_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/themes/themes.ui");

  gtk_widget_class_bind_template_child (widget_class, CcThemesPanel, themes_box);
}

static void
cc_themes_panel_init (CcThemesPanel *panel)
{
  g_resources_register (cc_themes_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  panel->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  panel->bg_settings = g_settings_new ("org.gnome.desktop.background");
  panel->screensaver_settings = g_settings_new ("org.gnome.desktop.screensaver");

  panel->current_theme = NULL;
}
