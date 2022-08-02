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

#include "cc-font-panel.h"
//#include "font-dialog.h"
#include "cc-font-resources.h"

#include <config.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>

#define WID(y) (GtkWidget *) gtk_builder_get_object (panel->builder, y)

enum {
  COMBO_BOX_MODEL_TEXT,
  COMBO_BOX_MODEL_VALUE,
  N_COLUMNS
};

typedef enum {
  GSD_HINTING_ACTION_NONE,
  GSD_HINTING_ACTION_SLIGHT,
  GSD_HINTING_ACTION_MEDIUM,
  GSD_HINTING_ACTION_FULL,
} GsdHintingActionType;

typedef enum {
  GSD_ANTIALIAS_ACTION_NONE,
  GSD_ANTIALIAS_ACTION_STANDARD,
  GSD_ANTIALIAS_ACTION_SUBPIXEL,
} GsdAntialiasActionType;

typedef enum {
  GSD_SCALE_ACTION_SMALL,
  GSD_SCALE_ACTION_MEDIUM,
  GSD_SCALE_ACTION_LARGE,
}GsdScaleActionType;

struct _CcFontPanel
{
  CcPanel parent_instance;

  GtkBuilder        *builder;
  GSettings         *interface_settings;
  GSettings         *wm_settings;
  GSettings         *gsd_settings;

  GtkAdjustment     *focus_adjustment;

  GtkWidget         *interface_text_btn;
  GtkWidget         *document_text_btn;
  GtkWidget         *mono_text_btn;
  GtkWidget         *titlebar_text_btn;

  GtkWidget         *hinting_cb;
  GtkWidget         *antialias_cb;

  // remove feature. by ryong
  //GtkWidget         *scale_adjustment;

  GtkWidget         *font_list;
  GtkWidget         *render_list;

  GtkListStore      *liststore_hinting;
  GtkListStore      *liststore_antialias;

  gboolean           is_updating;
};

CC_PANEL_REGISTER (CcFontPanel, cc_font_panel)

static const char *
cc_font_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/font";
}

static void 
set_value_for_combo (GtkComboBox *combo_box, gint value)
{
  GtkTreeIter iter;
  g_autoptr(GtkTreeIter) insert = NULL;
  GtkTreeIter new; 
  GtkTreeModel *model;
  gint value_tmp;
  //gint value_last = 0; 
  g_autofree gchar *text = NULL;
  gboolean ret; 

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  /* try to make the UI match the setting */
  do
    {    
      gtk_tree_model_get (model, &iter,
                          COMBO_BOX_MODEL_VALUE, &value_tmp,
                          -1); 
      if (value_tmp == value)
      {    
        gtk_combo_box_set_active_iter (combo_box, &iter);
        return;
      }    

    } while (gtk_tree_model_iter_next (model, &iter));
}

static void
call_list_box_separator (GtkListBoxRow *row,
                         GtkListBoxRow *before,
                         gpointer user_data)
{
  GtkWidget *current;
  if (before == NULL)
  {
    gtk_list_box_row_set_header (row, NULL);
    return;
  }

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
  {
    current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show (current);
    gtk_list_box_row_set_header (row, current);
  }
}

static void
scale_buttons_active (CcFontPanel *panel,
                     GParamSpec  *pspec,
                     GtkWidget   *button)
{
  if (panel->is_updating)
    return;

  double value = 1.0;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
  {
    value = *(double*) g_object_get_data (G_OBJECT (button), "scale");
    g_settings_set_double (panel->interface_settings, "text-scaling-factor", value);
  }
}

static void
scale_buttons_sync (GtkWidget *bbox,
                    CcFontPanel *panel)
{
  g_autoptr (GList) children;
  GList *l;
  double value;
  value = g_settings_get_double (panel->interface_settings, "text-scaling-factor");

  children = gtk_container_get_children (GTK_CONTAINER (bbox));
  for (l = children; l; l = l->next)
  {
    GtkWidget *button = l->data;
    gdouble scale = *(double*) g_object_get_data (G_OBJECT (button), "scale");
    if (scale == value)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

  }
}

static void
cc_font_panel_dispose (GObject *object)
{
  CcFontPanel *panel = CC_FONT_PANEL (object);

  g_clear_object (&panel->builder);

  g_clear_object (&panel->interface_settings);
  g_clear_object (&panel->wm_settings);
  g_clear_object (&panel->gsd_settings);

  G_OBJECT_CLASS (cc_font_panel_parent_class)->dispose (object);
}

static void
cc_font_panel_finalize (GObject *object)
{
  CcFontPanel *panel = CC_FONT_PANEL (object);

  g_clear_object (&panel->liststore_hinting);
  g_clear_object (&panel->liststore_antialias);

  G_OBJECT_CLASS (cc_font_panel_parent_class)->finalize (object);
}

static void
changed_hinting_cb (GtkWidget *widget, CcFontPanel *panel)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
  if (!ret)
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_tree_model_get (model, &iter, COMBO_BOX_MODEL_VALUE, &value, -1);

  g_settings_set_enum (panel->gsd_settings, "hinting", value);
}

static void
changed_antialias_cb (GtkWidget *widget, CcFontPanel *panel)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
  if (!ret)
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_tree_model_get (model, &iter, COMBO_BOX_MODEL_VALUE, &value, -1);

  g_settings_set_enum (panel->gsd_settings, "antialiasing", value);
}

static void
cc_font_panel_class_init (CcFontPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_font_panel_dispose;

  panel_class->get_help_uri = cc_font_panel_get_help_uri;
}

static GtkWidget *
make_label_for_scale (char *text)
{
  g_autofree *label = g_strdup_printf ("  %s  ",text);
  return gtk_label_new (label);
}

static void
init_gsd_combo_box_model (CcFontPanel *panel)
{
  struct {
    char *name;
    GsdHintingActionType value;
  } hinting_actions[] = {
    { N_("none"), GSD_HINTING_ACTION_NONE },
    { N_("slight"), GSD_HINTING_ACTION_SLIGHT },
    { N_("medium"), GSD_HINTING_ACTION_MEDIUM },
    { N_("full"), GSD_HINTING_ACTION_FULL },
  };

  struct {
    char *name;
    GsdAntialiasActionType value;
  } antialias_actions[] = {
    { N_("none"), GSD_ANTIALIAS_ACTION_NONE },
    { N_("grayscale"), GSD_ANTIALIAS_ACTION_STANDARD },
    { N_("rgba"), GSD_ANTIALIAS_ACTION_SUBPIXEL },
  };

  guint i;
  GtkListStore *store;
  GtkTreeModel *model = NULL;

  panel->liststore_hinting = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
  model = GTK_TREE_MODEL (panel->liststore_hinting);
  for (i = 0; i < G_N_ELEMENTS (hinting_actions); i++)
  {
    gtk_list_store_insert_with_values (panel->liststore_hinting,
                                       NULL, -1,
                                       COMBO_BOX_MODEL_TEXT, _(hinting_actions[i].name),
                                       COMBO_BOX_MODEL_VALUE, hinting_actions[i].value,
                                       -1);
  }

  gtk_combo_box_set_model (GTK_COMBO_BOX (panel->hinting_cb), model);

  panel->liststore_antialias = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
  model = GTK_TREE_MODEL (panel->liststore_antialias);
  for (i = 0; i < G_N_ELEMENTS (antialias_actions); i++)
  {
    gtk_list_store_insert_with_values (panel->liststore_antialias,
                                       NULL, -1,
                                       COMBO_BOX_MODEL_TEXT, _(antialias_actions[i].name),
                                       COMBO_BOX_MODEL_VALUE, antialias_actions[i].value,
                                       -1);
  }

  gtk_combo_box_set_model (GTK_COMBO_BOX (panel->antialias_cb), model);
}

static void
init_scale_buttons (CcFontPanel *panel)
{
  GtkWidget *w;
  double scale;
  GtkRadioButton *group = NULL;
  struct {
    char *name;
    GsdScaleActionType value;
  } scale_actions[] = {
    { N_("_small"), GSD_SCALE_ACTION_SMALL },
    { N_("_medium"), GSD_SCALE_ACTION_MEDIUM },
    { N_("_large"), GSD_SCALE_ACTION_LARGE },
  };

  scale = 0.75;
  w = WID ("scale-bbox");
  gtk_button_box_set_layout (GTK_BUTTON_BOX (w), GTK_BUTTONBOX_EXPAND);
  for (int i = 0; i < G_N_ELEMENTS (scale_actions); i++)
  {
    GtkWidget *button;
    button = gtk_radio_button_new_from_widget (group);
    gtk_button_set_image (GTK_BUTTON (button), make_label_for_scale(_(scale_actions[i].name)));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

    // 숫자로 변경
    g_object_set_data_full (G_OBJECT (button), "scale", g_memdup (&scale, sizeof (double)), g_free);

    g_signal_connect_object (button, "notify::active", G_CALLBACK (scale_buttons_active),
                             panel, G_CONNECT_SWAPPED);
    gtk_container_add (GTK_CONTAINER (w), button);
    group = GTK_RADIO_BUTTON (button);

    scale += 0.25;
  }

  panel->is_updating = TRUE;
  scale_buttons_sync (w, panel);
  panel->is_updating = FALSE;
}

static void
cc_font_panel_init (CcFontPanel *panel)
{
  gchar *objects[] = {"font-main-scrolled-window", NULL };
  g_autoptr(GError) err = NULL;
  GtkWidget *w;
  gint button_val;

  g_resources_register (cc_font_get_resource ());

  panel->builder = gtk_builder_new ();
  gtk_builder_add_objects_from_resource (panel->builder,
                                         "/org/gnome/control-center/font/font.ui",
                                         objects, &err);
  if (err)
  {
    g_warning ("Could not load ui: %s", err->message);
    return;
  }

  panel->hinting_cb = WID ("hint-combobox");
  panel->antialias_cb = WID ("antialiasing-combobox");
  panel->font_list = WID ("font-listbox");
  panel->render_list = WID ("render-listbox");

  // remove feature. by ryong
  //panel->scale_adjustment = gtk_adjustment_new (1, 0.5, 3, 0.01, 0, 0);

  // set header separator on listbox row
  gtk_list_box_set_header_func (panel->font_list, call_list_box_separator, NULL, NULL);
  gtk_list_box_set_header_func (panel->render_list, call_list_box_separator, NULL, NULL);

  panel->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  panel->wm_settings = g_settings_new ("org.gnome.desktop.wm.preferences");
  panel->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.xsettings");

  // font data binding
  g_settings_bind (panel->interface_settings, "font-name", WID("interface-fontbutton"),"font", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (panel->interface_settings, "document-font-name", WID("doc-text-fontbutton"),"font", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (panel->interface_settings, "monospace-font-name", WID("mono-text-fontbutton"),"font", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (panel->wm_settings, "titlebar-font", WID("windowtitle-text-fontbutton"),"font", G_SETTINGS_BIND_DEFAULT);

  // initialize gsd combo box
  init_gsd_combo_box_model (panel);

  // remove feature. by ryong
  //init_scale_buttons (panel);

  button_val = g_settings_get_enum (panel->gsd_settings, "hinting");
  set_value_for_combo (GTK_COMBO_BOX (panel->hinting_cb), button_val);

  button_val = g_settings_get_enum (panel->gsd_settings, "antialiasing");
  set_value_for_combo (GTK_COMBO_BOX (panel->antialias_cb), button_val);

  g_signal_connect (panel->hinting_cb, "changed",
                    G_CALLBACK (changed_hinting_cb), panel);
  g_signal_connect (panel->antialias_cb, "changed",
                    G_CALLBACK (changed_antialias_cb), panel);

  w = WID ("font-main-scrolled-window");
  gtk_container_add (GTK_CONTAINER (panel), w);
  gtk_widget_show_all (GTK_WIDGET (panel));
}
