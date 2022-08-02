/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 * Copyright (C) 2020 gooroom <gooroom@gooroom.kr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <gtk/gtk.h>

#include "cc-font-chooser-dialog.h"
#include "cc-font-chooser-widget.h"
#include "cc-font-chooser-utils.h"

struct _CcFontChooserDialogPrivate
{
  GtkWidget     *dialogbox;
  GtkWidget     *fontchooser;

  GtkWidget     *select_button;
  GtkWidget     *cancel_button;
  GtkWidget     *tweak_button;
};

static void cc_font_chooser_dialog_buildable_interface_init           (GtkBuildableIface *iface);
static GObject *cc_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                                      GtkBuilder   *builder,
                                                                      const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (CcFontChooserDialog, cc_font_chooser_dialog, GTK_TYPE_DIALOG,
                         G_ADD_PRIVATE (CcFontChooserDialog)
                         G_IMPLEMENT_INTERFACE (CC_TYPE_FONT_CHOOSER,
                                                _cc_font_chooser_delegate_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                cc_font_chooser_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
update_button (CcFontChooserDialog *dialog)
{
  CcFontChooserDialogPrivate *priv = dialog->priv;
  PangoFontDescription *desc;

  desc = cc_font_chooser_get_font_desc (CC_FONT_CHOOSER (priv->fontchooser));

  gtk_widget_set_sensitive (priv->select_button, desc != NULL);

  if (desc)
    pango_font_description_free (desc);
}

static void
update_tweak_button (CcFontChooserDialog *dialog)
{
  CcFontChooserLevel level;

  if (!dialog->priv->tweak_button)
    return;

  g_object_get (dialog->priv->fontchooser, "level", &level, NULL);
  if ((level & (CC_FONT_CHOOSER_LEVEL_VARIATIONS | CC_FONT_CHOOSER_LEVEL_FEATURES)) != 0)
    gtk_widget_show (dialog->priv->tweak_button);
  else
    gtk_widget_hide (dialog->priv->tweak_button);
}

static void
setup_tweak_button (CcFontChooserDialog *dialog)
{
  gboolean use_header;

  if (dialog->priv->tweak_button)
    return;

  g_object_get (dialog, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
      GtkWidget *button;
      GtkWidget *image;
      GtkWidget *header;
      GActionGroup *actions;

      actions = G_ACTION_GROUP (g_simple_action_group_new ());
      g_action_map_add_action (G_ACTION_MAP (actions),
                               cc_font_chooser_widget_get_tweak_action (dialog->priv->fontchooser));
      gtk_widget_insert_action_group (GTK_WIDGET (dialog), "font", actions);
      g_object_unref (actions);

      button = gtk_toggle_button_new ();
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "font.tweak");
      gtk_widget_set_focus_on_click (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

      image = gtk_image_new_from_icon_name ("emblem-system-symbolic", GTK_ICON_SIZE_BUTTON);
      gtk_widget_show (image);
      gtk_container_add (GTK_CONTAINER (button), image);

      header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      dialog->priv->tweak_button = button;
      update_tweak_button (dialog);
    }
}

static void
font_activated_cb (CcFontChooser *fontchooser,
                   const gchar    *fontname,
                   gpointer        user_data)
{
  GtkDialog *dialog = user_data;

  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static gboolean
cc_font_chooser_dialog_key_press_event (GtkWidget *dialog,
                                        GdkEventKey *event)
{
  CcFontChooserDialog *font_dialog = CC_FONT_CHOOSER_DIALOG (dialog);
  gboolean handled = FALSE;

  handled = GTK_WIDGET_CLASS (cc_font_chooser_dialog_parent_class)->key_press_event (dialog, event);
  if (!handled)
    handled = cc_font_chooser_widget_handle_event (font_dialog->priv->fontchooser, event);

  return handled;
}

static void
cc_font_chooser_dialog_map (GtkWidget *widget)
{
  CcFontChooserDialog *dialog = CC_FONT_CHOOSER_DIALOG (widget);

  setup_tweak_button (dialog);

  GTK_WIDGET_CLASS (cc_font_chooser_dialog_parent_class)->map (widget);
}

static void
cc_font_chooser_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcFontChooserDialog *dialog = CC_FONT_CHOOSER_DIALOG (object);
  CcFontChooserDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    default:
      g_object_set_property (G_OBJECT (priv->fontchooser), pspec->name, value);
      break;
    }
}

static void
cc_font_chooser_dialog_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  CcFontChooserDialog *dialog = CC_FONT_CHOOSER_DIALOG (object);
  CcFontChooserDialogPrivate *priv = dialog->priv;

  switch (prop_id)
    {
    default:
      g_object_get_property (G_OBJECT (priv->fontchooser), pspec->name, value);
      break;
    }
}

static void
cc_font_chooser_dialog_init (CcFontChooserDialog *fontchooserdialog)
{
  CcFontChooserDialogPrivate *priv;

  priv = fontchooserdialog->priv = cc_font_chooser_dialog_get_instance_private (fontchooserdialog);

  gtk_widget_init_template (GTK_WIDGET (fontchooserdialog));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (fontchooserdialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
G_GNUC_END_IGNORE_DEPRECATIONS
  _cc_font_chooser_set_delegate (CC_FONT_CHOOSER (fontchooserdialog),
                                 CC_FONT_CHOOSER (priv->fontchooser));

  g_signal_connect_swapped (priv->fontchooser, "notify::font-desc",
                            G_CALLBACK (update_button), fontchooserdialog);
  update_button (fontchooserdialog);

  g_signal_connect_swapped (priv->fontchooser, "notify::level",
                            G_CALLBACK (update_tweak_button), fontchooserdialog);
}

static void
cc_font_chooser_dialog_class_init (CcFontChooserDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_font_chooser_dialog_get_property;
  object_class->set_property = cc_font_chooser_dialog_set_property;

  widget_class->key_press_event = cc_font_chooser_dialog_key_press_event;
  widget_class->map = cc_font_chooser_dialog_map;

  _cc_font_chooser_install_properties (object_class);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/font/font-chooser-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserDialog, fontchooser);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserDialog, dialogbox);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserDialog, select_button);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserDialog, cancel_button);
  gtk_widget_class_bind_template_callback (widget_class, font_activated_cb);
}

GtkWidget *
cc_font_chooser_dialog_new (gchar *title, GtkWindow *parent)
{
  return GTK_WIDGET (g_object_new (CC_TYPE_FONT_CHOOSER_DIALOG,
                                  "title", title,
                                   "transient-for", parent,
                                   "use-header-bar", 1,
                                   NULL));
}

static void
cc_font_chooser_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = cc_font_chooser_dialog_buildable_get_internal_child;
}

static GObject *
cc_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                      GtkBuilder   *builder,
                                                      const gchar  *childname)
{
  CcFontChooserDialogPrivate *priv;

  priv = CC_FONT_CHOOSER_DIALOG (buildable)->priv;

  if (g_strcmp0 (childname, "select_button") == 0)
    return G_OBJECT (priv->select_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (priv->cancel_button);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}
