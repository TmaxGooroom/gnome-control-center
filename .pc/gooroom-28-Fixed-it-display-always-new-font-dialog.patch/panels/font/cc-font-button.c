/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998 David Abilleira Freijeiro <odaf@nexo.es>
 * All rights reserved.
 * Copyright (C) 2020 gooroom <gooroom@gooroom.kr>
 *
 * Based on gnome-color-picker by Federico Mena <federico@nuclecu.unam.mx>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
 /*
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "cc-font-button.h"
#include "cc-font-chooser.h"
#include "cc-font-define.h"
#include "cc-font-chooser-dialog.h"

/**
 * SECTION:gtkfontbutton
 * @Short_description: A button to launch a font chooser dialog
 * @Title: CcFontButton
 * @See_also: #CcFontChooserDialog, #GtkColorButton.
 *
 * The #CcFontButton is a button which displays the currently selected
 * font an allows to open a font chooser dialog to change the font.
 * It is suitable widget for selecting a font in a preference dialog.
 *
 * # CSS nodes
 *
 * CcFontButton has a single CSS node with name button and style class .font.
 */


struct _CcFontButtonPrivate
{
  gchar         *title;

  gchar         *fontname;

  guint         use_font : 1;
  guint         use_size : 1;
  guint         show_style : 1;
  guint         show_size : 1;
  guint         show_preview_entry : 1;

  GtkWidget     *font_dialog;
  GtkWidget     *font_label;
  GtkWidget     *size_label;
  GtkWidget     *font_size_box;

  PangoFontDescription *font_desc;
  PangoFontFamily      *font_family;
  PangoFontFace        *font_face;
  PangoFontMap         *font_map;
  gint                  font_size;
  char                 *font_features;
  PangoLanguage        *language;
  gchar                *preview_text;
  CcFontFilterFunc     font_filter;
  gpointer              font_filter_data;
  GDestroyNotify        font_filter_data_destroy;
  GtkCssProvider       *provider;

  CcFontChooserLevel   level;
};

/* Signals */
enum
{
  FONT_SET,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_TITLE,
  PROP_FONT_NAME,
  PROP_USE_FONT,
  PROP_USE_SIZE,
  PROP_SHOW_STYLE,
  PROP_SHOW_SIZE
};

/* Prototypes */
static void cc_font_button_finalize               (GObject            *object);
static void cc_font_button_get_property           (GObject            *object,
                                                    guint               param_id,
                                                    GValue             *value,
                                                    GParamSpec         *pspec);
static void cc_font_button_set_property           (GObject            *object,
                                                    guint               param_id,
                                                    const GValue       *value,
                                                    GParamSpec         *pspec);

static void cc_font_button_clicked                 (GtkButton         *button);

/* Dialog response functions */
static void response_cb                             (GtkDialog         *dialog,
                                                     gint               response_id,
                                                     gpointer           data);
static void dialog_destroy                          (GtkWidget         *widget,
                                                     gpointer           data);

/* Auxiliary functions */
static void cc_font_button_label_use_font          (CcFontButton     *gfs);
static void cc_font_button_update_font_info        (CcFontButton     *gfs);

static void        font_button_set_font_name (CcFontButton *button,
                                              const char    *fontname);
static void        cc_font_button_set_level     (CcFontButton       *font_button,
                                                  CcFontChooserLevel  level);
static void        cc_font_button_set_language  (CcFontButton *button,
                                                  const char    *language);

static guint font_button_signals[LAST_SIGNAL] = { 0 };

static void
clear_font_data (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_family)
    g_object_unref (priv->font_family);
  priv->font_family = NULL;

  if (priv->font_face)
    g_object_unref (priv->font_face);
  priv->font_face = NULL;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);
  priv->font_desc = NULL;

  g_free (priv->fontname);
  priv->fontname = NULL;

  g_free (priv->font_features);
  priv->font_features = NULL;
}

static void
clear_font_filter_data (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_filter_data_destroy)
    priv->font_filter_data_destroy (priv->font_filter_data);
  priv->font_filter = NULL;
  priv->font_filter_data = NULL;
  priv->font_filter_data_destroy = NULL;
}

static gboolean
font_description_style_equal (const PangoFontDescription *a,
                              const PangoFontDescription *b)
{
  return (pango_font_description_get_weight (a) == pango_font_description_get_weight (b) &&
          pango_font_description_get_style (a) == pango_font_description_get_style (b) &&
          pango_font_description_get_stretch (a) == pango_font_description_get_stretch (b) &&
          pango_font_description_get_variant (a) == pango_font_description_get_variant (b));
}

static void
cc_font_button_update_font_data (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;
  PangoFontFamily **families;
  PangoFontFace **faces;
  gint n_families, n_faces, i;
  const gchar *family;

  g_assert (priv->font_desc != NULL);

  priv->fontname = pango_font_description_to_string (priv->font_desc);

  family = pango_font_description_get_family (priv->font_desc);
  if (family == NULL)
    return;

  n_families = 0;
  families = NULL;
  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (font_button)),
                               &families, &n_families);
  n_faces = 0;
  faces = NULL;
  for (i = 0; i < n_families; i++)
    {
      const gchar *name = pango_font_family_get_name (families[i]);

      if (!g_ascii_strcasecmp (name, family))
        {
          priv->font_family = g_object_ref (families[i]);

          pango_font_family_list_faces (families[i], &faces, &n_faces);
          break;
        }
    }
  g_free (families);

  for (i = 0; i < n_faces; i++)
    {
      PangoFontDescription *tmp_desc = pango_font_face_describe (faces[i]);

      if (font_description_style_equal (tmp_desc, priv->font_desc))
        {
          priv->font_face = g_object_ref (faces[i]);

          pango_font_description_free (tmp_desc);
          break;
        }
      else
        pango_font_description_free (tmp_desc);
    }

  g_free (faces);
}

static gchar *
cc_font_button_get_preview_text (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    return cc_font_chooser_get_preview_text (CC_FONT_CHOOSER (priv->font_dialog));

  return g_strdup (priv->preview_text);
}

static void
cc_font_button_set_preview_text (CcFontButton *font_button,
                                  const gchar   *preview_text)
{
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    {
      cc_font_chooser_set_preview_text (CC_FONT_CHOOSER (priv->font_dialog),
                                         preview_text);
      return;
    }

  g_free (priv->preview_text);
  priv->preview_text = g_strdup (preview_text);
}


static gboolean
cc_font_button_get_show_preview_entry (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    return cc_font_chooser_get_show_preview_entry (CC_FONT_CHOOSER (priv->font_dialog));

  return priv->show_preview_entry;
}

static void
cc_font_button_set_show_preview_entry (CcFontButton *font_button,
                                        gboolean       show)
{
  CcFontButtonPrivate *priv = font_button->priv;

  show = show != FALSE;

  if (priv->show_preview_entry != show)
    {
      priv->show_preview_entry = show;
      if (priv->font_dialog)
        cc_font_chooser_set_show_preview_entry (CC_FONT_CHOOSER (priv->font_dialog), show);
      g_object_notify (G_OBJECT (font_button), "show-preview-entry");
    }
}

static PangoFontFamily *
cc_font_button_font_chooser_get_font_family (CcFontChooser *chooser)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);
  CcFontButtonPrivate *priv = font_button->priv;

  return priv->font_family;
}

static PangoFontFace *
cc_font_button_font_chooser_get_font_face (CcFontChooser *chooser)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);
  CcFontButtonPrivate *priv = font_button->priv;

  return priv->font_face;
}

static int
cc_font_button_font_chooser_get_font_size (CcFontChooser *chooser)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);
  CcFontButtonPrivate *priv = font_button->priv;

  return priv->font_size;
}

static void
cc_font_button_font_chooser_set_filter_func (CcFontChooser    *chooser,
                                              CcFontFilterFunc  filter_func,
                                              gpointer           filter_data,
                                              GDestroyNotify     data_destroy)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog)
    {
      cc_font_chooser_set_filter_func (CC_FONT_CHOOSER (priv->font_dialog),
                                        filter_func,
                                        filter_data,
                                        data_destroy);
      return;
    }

  clear_font_filter_data (font_button);
  priv->font_filter = filter_func;
  priv->font_filter_data = filter_data;
  priv->font_filter_data_destroy = data_destroy;
}

static void
cc_font_button_take_font_desc (CcFontButton        *font_button,
                                PangoFontDescription *font_desc)
{
  CcFontButtonPrivate *priv = font_button->priv;
  GObject *object = G_OBJECT (font_button);

  if (priv->font_desc && font_desc &&
      pango_font_description_equal (priv->font_desc, font_desc))
    {
      pango_font_description_free (font_desc);
      return;
    }

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  if (font_desc)
    priv->font_desc = font_desc; /* adopted */
  else
    priv->font_desc = pango_font_description_from_string (_("Sans 12"));

  if (pango_font_description_get_size_is_absolute (priv->font_desc))
    priv->font_size = pango_font_description_get_size (priv->font_desc);
  else
    priv->font_size = pango_font_description_get_size (priv->font_desc) / PANGO_SCALE;

  cc_font_button_update_font_data (font_button);
  cc_font_button_update_font_info (font_button);

  if (priv->font_dialog)
    cc_font_chooser_set_font_desc (CC_FONT_CHOOSER (priv->font_dialog),
                                    priv->font_desc);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");
  g_object_notify (G_OBJECT (font_button), "font-name");

  g_object_thaw_notify (object);
}

static const PangoFontDescription *
cc_font_button_get_font_desc (CcFontButton *font_button)
{
  return font_button->priv->font_desc;
}

static void
cc_font_button_font_chooser_set_font_map (CcFontChooser *chooser,
                                           PangoFontMap   *font_map)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);

  if (g_set_object (&font_button->priv->font_map, font_map))
    {
      PangoContext *context;

      if (!font_map)
        font_map = pango_cairo_font_map_get_default ();

      context = gtk_widget_get_pango_context (font_button->priv->font_label);
      pango_context_set_font_map (context, font_map);
    }
}

static PangoFontMap *
cc_font_button_font_chooser_get_font_map (CcFontChooser *chooser)
{
  CcFontButton *font_button = CC_FONT_BUTTON (chooser);

  return font_button->priv->font_map;
}

static void
cc_font_button_font_chooser_notify (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
  /* We do not forward the notification of the "font" property to the dialog! */
  if (pspec->name == "preview-text" ||
      pspec->name == "show-preview-entry")
    g_object_notify_by_pspec (user_data, pspec);
}

static void
cc_font_button_font_chooser_iface_init (CcFontChooserIface *iface)
{
  iface->get_font_family = cc_font_button_font_chooser_get_font_family;
  iface->get_font_face = cc_font_button_font_chooser_get_font_face;
  iface->get_font_size = cc_font_button_font_chooser_get_font_size;
  iface->set_filter_func = cc_font_button_font_chooser_set_filter_func;
  iface->set_font_map = cc_font_button_font_chooser_set_font_map;
  iface->get_font_map = cc_font_button_font_chooser_get_font_map;
}

G_DEFINE_TYPE_WITH_CODE (CcFontButton, cc_font_button, GTK_TYPE_BUTTON,
                         G_ADD_PRIVATE (CcFontButton)
                         G_IMPLEMENT_INTERFACE (CC_TYPE_FONT_CHOOSER,
                                                cc_font_button_font_chooser_iface_init))

static void
cc_font_button_class_init (CcFontButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkButtonClass *button_class;
  
  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  button_class = (GtkButtonClass *) klass;

  gobject_class->finalize = cc_font_button_finalize;
  gobject_class->set_property = cc_font_button_set_property;
  gobject_class->get_property = cc_font_button_get_property;
  
  button_class->clicked = cc_font_button_clicked;
  
  klass->font_set = NULL;

  _cc_font_chooser_install_properties (gobject_class);

  /**
   * CcFontButton:title:
   * 
   * The title of the font chooser dialog.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The title of the font chooser dialog",
                                                        _("Pick a Font"),
                                                        CC_PARAM_READWRITE));

  /**
   * CcFontButton:font-name:
   * 
   * The name of the currently selected font.
   *
   * Since: 2.4
   *
   * Deprecated: 3.22: Use the #CcFontChooser::font property instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        "Font name",
                                                        "The name of the selected font",
                                                        _("Sans 12"),
                                                        CC_PARAM_READWRITE | G_PARAM_DEPRECATED));

  /**
   * CcFontButton:use-font:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * in the selected font.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FONT,
                                   g_param_spec_boolean ("use-font",
                                                         "Use font in label",
                                                         "Whether the label is drawn in the selected font",
                                                         FALSE,
                                                         CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * CcFontButton:use-size:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * with the selected font size.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_SIZE,
                                   g_param_spec_boolean ("use-size",
                                                         "Use size in label",
                                                         "Whether the label is drawn with the selected font size",
                                                         FALSE,
                                                         CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * CcFontButton:show-style:
   * 
   * If this property is set to %TRUE, the name of the selected font style 
   * will be shown in the label. For a more WYSIWYG way to show the selected 
   * style, see the ::use-font property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_STYLE,
                                   g_param_spec_boolean ("show-style",
                                                         "Show style",
                                                         "Whether the selected font style is shown in the label",
                                                         TRUE,
                                                         CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  /**
   * CcFontButton:show-size:
   * 
   * If this property is set to %TRUE, the selected font size will be shown 
   * in the label. For a more WYSIWYG way to show the selected size, see the 
   * ::use-size property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_SIZE,
                                   g_param_spec_boolean ("show-size",
                                                         "Show size",
                                                         "Whether selected font size is shown in the label",
                                                         TRUE,
                                                         CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * CcFontButton::font-set:
   * @widget: the object which received the signal.
   * 
   * The ::font-set signal is emitted when the user selects a font. 
   * When handling this signal, use cc_font_chooser_get_font()
   * to find out which font was just selected.
   *
   * Note that this signal is only emitted when the user
   * changes the font. If you need to react to programmatic font changes
   * as well, use the notify::font signal.
   *
   * Since: 2.4
   */
  font_button_signals[FONT_SET] = g_signal_new ("font-set",
                                                G_TYPE_FROM_CLASS (gobject_class),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (CcFontButtonClass, font_set),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/font/font-button.ui");
  gtk_widget_class_bind_template_child_private (widget_class, CcFontButton, font_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontButton, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontButton, font_size_box);

  gtk_widget_class_set_css_name (widget_class, "button");
}

static void
cc_font_button_init (CcFontButton *font_button)
{
  GtkStyleContext *context;

  font_button->priv = cc_font_button_get_instance_private (font_button);

  /* Initialize fields */
  font_button->priv->use_font = FALSE;
  font_button->priv->use_size = FALSE;
  font_button->priv->show_style = TRUE;
  font_button->priv->show_size = TRUE;
  font_button->priv->show_preview_entry = TRUE;
  font_button->priv->font_dialog = NULL;
  font_button->priv->font_family = NULL;
  font_button->priv->font_face = NULL;
  font_button->priv->font_size = -1;
  font_button->priv->title = g_strdup (_("Pick a Font"));
  font_button->priv->level = CC_FONT_CHOOSER_LEVEL_FAMILY |
                             CC_FONT_CHOOSER_LEVEL_STYLE |
                             CC_FONT_CHOOSER_LEVEL_SIZE;
  font_button->priv->language = pango_language_get_default ();

  gtk_widget_init_template (GTK_WIDGET (font_button));

  cc_font_button_take_font_desc (font_button, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (font_button));
  gtk_style_context_add_class (context, "font");
}

static void
cc_font_button_finalize (GObject *object)
{
  CcFontButton *font_button = CC_FONT_BUTTON (object);
  CcFontButtonPrivate *priv = font_button->priv;

  if (priv->font_dialog != NULL) 
    gtk_widget_destroy (priv->font_dialog);

  g_free (priv->title);

  clear_font_data (font_button);
  clear_font_filter_data (font_button);

  g_free (priv->preview_text);

  g_clear_object (&priv->provider);

  G_OBJECT_CLASS (cc_font_button_parent_class)->finalize (object);
}

static void
cc_font_button_set_property (GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcFontButton *font_button = CC_FONT_BUTTON (object);

  switch (param_id) 
    {
    case CC_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      cc_font_button_set_preview_text (font_button, g_value_get_string (value));
      break;
    case CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      cc_font_button_set_show_preview_entry (font_button, g_value_get_boolean (value));
      break;
    case PROP_TITLE:
      cc_font_button_set_title (font_button, g_value_get_string (value));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_DESC:
      cc_font_button_take_font_desc (font_button, g_value_dup_boxed (value));
      break;
    case CC_FONT_CHOOSER_PROP_LANGUAGE:
      cc_font_button_set_language (font_button, g_value_get_string (value));
      break;
    case CC_FONT_CHOOSER_PROP_LEVEL:
      cc_font_button_set_level (font_button, g_value_get_flags (value));
      break;
    case CC_FONT_CHOOSER_PROP_FONT:
    case PROP_FONT_NAME:
      font_button_set_font_name (font_button, g_value_get_string (value));
      break;
    case PROP_USE_FONT:
      cc_font_button_set_use_font (font_button, g_value_get_boolean (value));
      break;
    case PROP_USE_SIZE:
      cc_font_button_set_use_size (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_STYLE:
      cc_font_button_set_show_style (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_SIZE:
      cc_font_button_set_show_size (font_button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
cc_font_button_get_property (GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcFontButton *font_button = CC_FONT_BUTTON (object);
  CcFontButtonPrivate *priv = font_button->priv;
  
  switch (param_id) 
    {
    case CC_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, cc_font_button_get_preview_text (font_button));
      break;
    case CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, cc_font_button_get_show_preview_entry (font_button));
      break;
    case PROP_TITLE:
      g_value_set_string (value, cc_font_button_get_title (font_button));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, cc_font_button_get_font_desc (font_button));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_FEATURES:
      g_value_set_string (value, priv->font_features);
      break;
    case CC_FONT_CHOOSER_PROP_LANGUAGE:
      g_value_set_string (value, pango_language_to_string (priv->language));
      break;
    case CC_FONT_CHOOSER_PROP_LEVEL:
      g_value_set_flags (value, priv->level);
      break;
    case CC_FONT_CHOOSER_PROP_FONT:
    case PROP_FONT_NAME:
      g_value_set_string (value, font_button->priv->fontname);
      break;
    case PROP_USE_FONT:
      g_value_set_boolean (value, cc_font_button_get_use_font (font_button));
      break;
    case PROP_USE_SIZE:
      g_value_set_boolean (value, cc_font_button_get_use_size (font_button));
      break;
    case PROP_SHOW_STYLE:
      g_value_set_boolean (value, cc_font_button_get_show_style (font_button));
      break;
    case PROP_SHOW_SIZE:
      g_value_set_boolean (value, cc_font_button_get_show_size (font_button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


/**
 * cc_font_button_new:
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
cc_font_button_new (void)
{
  return g_object_new (CC_TYPE_FONT_BUTTON, NULL);
}

/**
 * cc_font_button_new_with_font:
 * @fontname: Name of font to display in font chooser dialog
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
cc_font_button_new_with_font (const gchar *fontname)
{
  return g_object_new (CC_TYPE_FONT_BUTTON, "font", fontname, NULL);
} 

/**
 * cc_font_button_set_title:
 * @font_button: a #CcFontButton
 * @title: a string containing the font chooser dialog title
 *
 * Sets the title for the font chooser dialog.  
 *
 * Since: 2.4
 */
void
cc_font_button_set_title (CcFontButton *font_button, 
                           const gchar   *title)
{
  gchar *old_title;
  g_return_if_fail (CC_IS_FONT_BUTTON (font_button));
  
  old_title = font_button->priv->title;
  font_button->priv->title = g_strdup (title);
  g_free (old_title);
  
  if (font_button->priv->font_dialog)
    gtk_window_set_title (GTK_WINDOW (font_button->priv->font_dialog),
                          font_button->priv->title);

  g_object_notify (G_OBJECT (font_button), "title");
} 

/**
 * cc_font_button_get_title:
 * @font_button: a #CcFontButton
 *
 * Retrieves the title of the font chooser dialog.
 *
 * Returns: an internal copy of the title string which must not be freed.
 *
 * Since: 2.4
 */
const gchar*
cc_font_button_get_title (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), NULL);

  return font_button->priv->title;
} 

/**
 * cc_font_button_get_use_font:
 * @font_button: a #CcFontButton
 *
 * Returns whether the selected font is used in the label.
 *
 * Returns: whether the selected font is used in the label.
 *
 * Since: 2.4
 */
gboolean
cc_font_button_get_use_font (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_font;
} 

/**
 * cc_font_button_set_use_font:
 * @font_button: a #CcFontButton
 * @use_font: If %TRUE, font name will be written using font chosen.
 *
 * If @use_font is %TRUE, the font name will be written using the selected font.  
 *
 * Since: 2.4
 */
void  
cc_font_button_set_use_font (CcFontButton *font_button,
			      gboolean       use_font)
{
  g_return_if_fail (CC_IS_FONT_BUTTON (font_button));
  
  use_font = (use_font != FALSE);
  
  if (font_button->priv->use_font != use_font) 
    {
      font_button->priv->use_font = use_font;

      cc_font_button_label_use_font (font_button);
 
      g_object_notify (G_OBJECT (font_button), "use-font");
    }
} 


/**
 * cc_font_button_get_use_size:
 * @font_button: a #CcFontButton
 *
 * Returns whether the selected size is used in the label.
 *
 * Returns: whether the selected size is used in the label.
 *
 * Since: 2.4
 */
gboolean
cc_font_button_get_use_size (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_size;
} 

/**
 * cc_font_button_set_use_size:
 * @font_button: a #CcFontButton
 * @use_size: If %TRUE, font name will be written using the selected size.
 *
 * If @use_size is %TRUE, the font name will be written using the selected size.
 *
 * Since: 2.4
 */
void  
cc_font_button_set_use_size (CcFontButton *font_button,
                              gboolean       use_size)
{
  g_return_if_fail (CC_IS_FONT_BUTTON (font_button));
  
  use_size = (use_size != FALSE);
  if (font_button->priv->use_size != use_size) 
    {
      font_button->priv->use_size = use_size;

      cc_font_button_label_use_font (font_button);

      g_object_notify (G_OBJECT (font_button), "use-size");
    }
} 

/**
 * cc_font_button_get_show_style:
 * @font_button: a #CcFontButton
 * 
 * Returns whether the name of the font style will be shown in the label.
 * 
 * Returns: whether the font style will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
cc_font_button_get_show_style (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_style;
}

/**
 * cc_font_button_set_show_style:
 * @font_button: a #CcFontButton
 * @show_style: %TRUE if font style should be displayed in label.
 *
 * If @show_style is %TRUE, the font style will be displayed along with name of the selected font.
 *
 * Since: 2.4
 */
void
cc_font_button_set_show_style (CcFontButton *font_button,
                                gboolean       show_style)
{
  g_return_if_fail (CC_IS_FONT_BUTTON (font_button));
  
  show_style = (show_style != FALSE);
  if (font_button->priv->show_style != show_style) 
    {
      font_button->priv->show_style = show_style;
      
      cc_font_button_update_font_info (font_button);
  
      g_object_notify (G_OBJECT (font_button), "show-style");
    }
} 


/**
 * cc_font_button_get_show_size:
 * @font_button: a #CcFontButton
 * 
 * Returns whether the font size will be shown in the label.
 * 
 * Returns: whether the font size will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
cc_font_button_get_show_size (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_size;
}

/**
 * cc_font_button_set_show_size:
 * @font_button: a #CcFontButton
 * @show_size: %TRUE if font size should be displayed in dialog.
 *
 * If @show_size is %TRUE, the font size will be displayed along with the name of the selected font.
 *
 * Since: 2.4
 */
void
cc_font_button_set_show_size (CcFontButton *font_button,
                               gboolean       show_size)
{
  g_return_if_fail (CC_IS_FONT_BUTTON (font_button));
  
  show_size = (show_size != FALSE);

  if (font_button->priv->show_size != show_size) 
    {
      font_button->priv->show_size = show_size;

      if (font_button->priv->show_size)
	gtk_widget_show (font_button->priv->font_size_box);
      else
	gtk_widget_hide (font_button->priv->font_size_box);
      
      cc_font_button_update_font_info (font_button);

      g_object_notify (G_OBJECT (font_button), "show-size");
    }
} 


/**
 * cc_font_button_get_font_name:
 * @font_button: a #CcFontButton
 *
 * Retrieves the name of the currently selected font. This name includes
 * style and size information as well. If you want to render something
 * with the font, use this string with pango_font_description_from_string() .
 * If you’re interested in peeking certain values (family name,
 * style, size, weight) just query these properties from the
 * #PangoFontDescription object.
 *
 * Returns: an internal copy of the font name which must not be freed.
 *
 * Since: 2.4
 * Deprecated: 3.22: Use cc_font_chooser_get_font() instead
 */
const gchar *
cc_font_button_get_font_name (CcFontButton *font_button)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), NULL);

  return font_button->priv->fontname;
}

static void
font_button_set_font_name (CcFontButton *font_button,
                           const char    *fontname)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (fontname);
  cc_font_button_take_font_desc (font_button, font_desc);
}

/**
 * cc_font_button_set_font_name:
 * @font_button: a #CcFontButton
 * @fontname: Name of font to display in font chooser dialog
 *
 * Sets or updates the currently-displayed font in font picker dialog.
 *
 * Returns: %TRUE
 *
 * Since: 2.4
 * Deprecated: 3.22: Use cc_font_chooser_set_font() instead
 */
gboolean 
cc_font_button_set_font_name (CcFontButton *font_button,
                               const gchar    *fontname)
{
  g_return_val_if_fail (CC_IS_FONT_BUTTON (font_button), FALSE);
  g_return_val_if_fail (fontname != NULL, FALSE);

  font_button_set_font_name (font_button, fontname);

  return TRUE;
}

static void
cc_font_button_clicked (GtkButton *button)
{
  CcFontChooser *font_dialog;
  CcFontButton  *font_button = CC_FONT_BUTTON (button);
  CcFontButtonPrivate *priv = font_button->priv;
  gboolean test;
  
  if (!font_button->priv->font_dialog) 
    {
      GtkWidget *parent;
      
      parent = gtk_widget_get_toplevel (GTK_WIDGET (font_button));

      priv->font_dialog = cc_font_chooser_dialog_new (priv->title, NULL);
      font_dialog = CC_FONT_CHOOSER (font_button->priv->font_dialog);

      if (priv->font_map)
        cc_font_chooser_set_font_map (font_dialog, priv->font_map);
      cc_font_chooser_set_show_preview_entry (font_dialog, priv->show_preview_entry);
      cc_font_chooser_set_level (CC_FONT_CHOOSER (font_dialog), priv->level);
      cc_font_chooser_set_language (CC_FONT_CHOOSER (font_dialog), pango_language_to_string 
(priv->language));

      if (priv->preview_text)
        {
          cc_font_chooser_set_preview_text (font_dialog, priv->preview_text);
          g_free (priv->preview_text);
          priv->preview_text = NULL;
        }

      if (priv->font_filter)
        {
          cc_font_chooser_set_filter_func (font_dialog,
                                            priv->font_filter,
                                            priv->font_filter_data,
                                            priv->font_filter_data_destroy);
          priv->font_filter = NULL;
          priv->font_filter_data = NULL;
          priv->font_filter_data_destroy = NULL;
        }

      if (gtk_widget_is_toplevel (parent) && GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (font_dialog)))
            gtk_window_set_transient_for (GTK_WINDOW (font_dialog), GTK_WINDOW (parent));

          gtk_window_set_modal (GTK_WINDOW (font_dialog),
                                gtk_window_get_modal (GTK_WINDOW (parent)));
        }

      g_signal_connect (font_dialog, "notify",
                        G_CALLBACK (cc_font_button_font_chooser_notify), button);

      g_signal_connect (font_dialog, "response",
                        G_CALLBACK (response_cb), font_button);

      g_signal_connect (font_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), font_button);

      g_signal_connect (font_dialog, "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }
  
  test = gtk_widget_get_visible (font_button->priv->font_dialog);
  if (!gtk_widget_get_visible (font_button->priv->font_dialog))
    {
      font_dialog = CC_FONT_CHOOSER (font_button->priv->font_dialog);
      cc_font_chooser_set_font_desc (font_dialog, font_button->priv->font_desc);
    } 

  gtk_window_present (GTK_WINDOW (font_button->priv->font_dialog));
}


static void
response_cb (GtkDialog *dialog,
             gint       response_id,
             gpointer   data)
{
  CcFontButton *font_button = CC_FONT_BUTTON (data);
  CcFontButtonPrivate *priv = font_button->priv;
  CcFontChooser *font_chooser;
  GObject *object;

  gtk_widget_hide (font_button->priv->font_dialog);

  if (response_id != GTK_RESPONSE_OK)
    return;

  font_chooser = CC_FONT_CHOOSER (priv->font_dialog);
  object = G_OBJECT (font_chooser);

  g_object_freeze_notify (object);

  clear_font_data (font_button);

  priv->font_desc = cc_font_chooser_get_font_desc (font_chooser);
  if (priv->font_desc)
    priv->fontname = pango_font_description_to_string (priv->font_desc);
  priv->font_family = cc_font_chooser_get_font_family (font_chooser);
  if (priv->font_family)
    g_object_ref (priv->font_family);
  priv->font_face = cc_font_chooser_get_font_face (font_chooser);
  if (priv->font_face)
    g_object_ref (priv->font_face);
  priv->font_size = cc_font_chooser_get_font_size (font_chooser);
  g_free (priv->font_features);
  priv->font_features = cc_font_chooser_get_font_features (font_chooser);
  priv->language = pango_language_from_string (cc_font_chooser_get_language (font_chooser));

  /* Set label font */
  cc_font_button_update_font_info (font_button);

  g_object_notify (G_OBJECT (font_button), "font");
  g_object_notify (G_OBJECT (font_button), "font-desc");
  g_object_notify (G_OBJECT (font_button), "font-name");
  g_object_notify (G_OBJECT (font_button), "font-features");

  g_object_thaw_notify (object);

  /* Emit font_set signal */
  g_signal_emit (font_button, font_button_signals[FONT_SET], 0);
}

static void
dialog_destroy (GtkWidget *widget,
                gpointer   data)
{
  CcFontButton *font_button = CC_FONT_BUTTON (data);
    
  /* Dialog will get destroyed so reference is not valid now */
  font_button->priv->font_dialog = NULL;
} 

static gchar *
pango_font_description_to_css (PangoFontDescription *desc)
{
  GString *s;
  PangoFontMask set;

  s = g_string_new ("* { ");

  set = pango_font_description_get_set_fields (desc);
  if (set & PANGO_FONT_MASK_FAMILY)
    {
      g_string_append (s, "font-family: ");
      g_string_append (s, pango_font_description_get_family (desc));
      g_string_append (s, "; ");
    }
  if (set & PANGO_FONT_MASK_STYLE)
    {
      switch (pango_font_description_get_style (desc))
        {
        case PANGO_STYLE_NORMAL:
          g_string_append (s, "font-style: normal; ");
          break;
        case PANGO_STYLE_OBLIQUE:
          g_string_append (s, "font-style: oblique; ");
          break;
        case PANGO_STYLE_ITALIC:
          g_string_append (s, "font-style: italic; ");
          break;
        }
    }
  if (set & PANGO_FONT_MASK_VARIANT)
    {
      switch (pango_font_description_get_variant (desc))
        {
        case PANGO_VARIANT_NORMAL:
          g_string_append (s, "font-variant: normal; ");
          break;
        case PANGO_VARIANT_SMALL_CAPS:
          g_string_append (s, "font-variant: small-caps; ");
          break;
        }
    }
  if (set & PANGO_FONT_MASK_WEIGHT)
    {
      switch (pango_font_description_get_weight (desc))
        {
        case PANGO_WEIGHT_THIN:
          g_string_append (s, "font-weight: 100; ");
          break;
        case PANGO_WEIGHT_ULTRALIGHT:
          g_string_append (s, "font-weight: 200; ");
          break;
        case PANGO_WEIGHT_LIGHT:
        case PANGO_WEIGHT_SEMILIGHT:
          g_string_append (s, "font-weight: 300; ");
          break;
        case PANGO_WEIGHT_BOOK:
        case PANGO_WEIGHT_NORMAL:
          g_string_append (s, "font-weight: 400; ");
          break;
        case PANGO_WEIGHT_MEDIUM:
          g_string_append (s, "font-weight: 500; ");
          break;
        case PANGO_WEIGHT_SEMIBOLD:
          g_string_append (s, "font-weight: 600; ");
          break;
        case PANGO_WEIGHT_BOLD:
          g_string_append (s, "font-weight: 700; ");
          break;
        case PANGO_WEIGHT_ULTRABOLD:
          g_string_append (s, "font-weight: 800; ");
          break;
        case PANGO_WEIGHT_HEAVY:
        case PANGO_WEIGHT_ULTRAHEAVY:
          g_string_append (s, "font-weight: 900; ");
          break;
        }
    }
  if (set & PANGO_FONT_MASK_STRETCH)
    {
      switch (pango_font_description_get_stretch (desc))
        {
        case PANGO_STRETCH_ULTRA_CONDENSED:
          g_string_append (s, "font-stretch: ultra-condensed; ");
          break;
        case PANGO_STRETCH_EXTRA_CONDENSED:
          g_string_append (s, "font-stretch: extra-condensed; ");
          break;
        case PANGO_STRETCH_CONDENSED:
          g_string_append (s, "font-stretch: condensed; ");
          break;
        case PANGO_STRETCH_SEMI_CONDENSED:
          g_string_append (s, "font-stretch: semi-condensed; ");
          break;
        case PANGO_STRETCH_NORMAL:
          g_string_append (s, "font-stretch: normal; ");
          break;
        case PANGO_STRETCH_SEMI_EXPANDED:
          g_string_append (s, "font-stretch: semi-expanded; ");
          break;
        case PANGO_STRETCH_EXPANDED:
          g_string_append (s, "font-stretch: expanded; ");
          break;
        case PANGO_STRETCH_EXTRA_EXPANDED:
          g_string_append (s, "font-stretch: extra-expanded; ");
          break;
        case PANGO_STRETCH_ULTRA_EXPANDED:
          g_string_append (s, "font-stretch: ultra-expanded; ");
          break;
        }
    }
  if (set & PANGO_FONT_MASK_SIZE)
    {
      g_string_append_printf (s, "font-size: %dpt", pango_font_description_get_size (desc) / PANGO_SCALE);
    }

  g_string_append (s, "}");

  return g_string_free (s, FALSE);
}

static void
cc_font_button_label_use_font (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (priv->font_label);

  if (!priv->use_font)
    {
      if (priv->provider)
        {
          gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (priv->provider));
          g_clear_object (&priv->provider);
        }
    }
  else
    {
      PangoFontDescription *desc;
      gchar *data;

      if (!priv->provider)
        {
          priv->provider = gtk_css_provider_new ();
          gtk_style_context_add_provider (context,
                                          GTK_STYLE_PROVIDER (priv->provider),
                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

      desc = pango_font_description_copy (priv->font_desc);

      if (!priv->use_size)
        pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);

      data = pango_font_description_to_css (desc);
      gtk_css_provider_load_from_data (priv->provider, data, -1, NULL);

      g_free (data);
      pango_font_description_free (desc);
    }
}

static void
cc_font_button_update_font_info (CcFontButton *font_button)
{
  CcFontButtonPrivate *priv = font_button->priv;
  const gchar *fam_name;
  const gchar *face_name;
  gchar *family_style;

  if (priv->font_family)
    fam_name = pango_font_family_get_name (priv->font_family);
  else
    fam_name = C_("font", "None");
  if (priv->font_face)
    face_name = pango_font_face_get_face_name (priv->font_face);
  else
    face_name = "";

  if (priv->show_style)
    family_style = g_strconcat (fam_name, " ", face_name, NULL);
  else
    family_style = g_strdup (fam_name);

  gtk_label_set_text (GTK_LABEL (font_button->priv->font_label), family_style);
  g_free (family_style);

  if (font_button->priv->show_size) 
    {
      /* mirror Pango, which doesn't translate this either */
      gchar *size = g_strdup_printf ("%2.4g%s",
                                     pango_font_description_get_size (priv->font_desc) / (double)PANGO_SCALE,
                                     pango_font_description_get_size_is_absolute (priv->font_desc) ? "px" : "");
      
      gtk_label_set_text (GTK_LABEL (font_button->priv->size_label), size);
      
      g_free (size);
    }

  cc_font_button_label_use_font (font_button);
} 

static void
cc_font_button_set_level (CcFontButton       *button,
                           CcFontChooserLevel  level)
{
  CcFontButtonPrivate *priv = button->priv;

  if (priv->level == level)
    return;

  priv->level = level;

  if (priv->font_dialog)
    g_object_set (priv->font_dialog, "level", level, NULL);

  g_object_set (button,
                "show-size", (level & CC_FONT_CHOOSER_LEVEL_SIZE) != 0,
                "show-style", (level & CC_FONT_CHOOSER_LEVEL_STYLE) != 0,
                NULL);

  g_object_notify (G_OBJECT (button), "level");
}

static void
cc_font_button_set_language (CcFontButton *button,
                              const char    *language)
{
  CcFontButtonPrivate *priv = button->priv;

  priv->language = pango_language_from_string (language);

  if (priv->font_dialog)
    cc_font_chooser_set_language (CC_FONT_CHOOSER (priv->font_dialog), language);

  g_object_notify (G_OBJECT (button), "language");
}
