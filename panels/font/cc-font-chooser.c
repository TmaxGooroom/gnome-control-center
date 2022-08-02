/* GTK - The GIMP Toolkit
 * gtkfontchooser.c - Abstract interface for font file selectors GUIs
 *
 * Copyright (C) 2006, Emmanuele Bassi
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-font-chooser.h"
#include "cc-font-chooser-private.h"
#include "cc-font-define.h"

enum
{
  SIGNAL_FONT_ACTIVATED,
  LAST_SIGNAL
};

static guint chooser_signals[LAST_SIGNAL];

/* enumerations from "gtkfontchooser.h" */
GType
cc_font_chooser_level_get_type (void)
{
    static GType etype = 0;
    if (G_UNLIKELY(etype == 0)) {
        static const GFlagsValue values[] = {
            { CC_FONT_CHOOSER_LEVEL_FAMILY, "CC_FONT_CHOOSER_LEVEL_FAMILY", "family" },
            { CC_FONT_CHOOSER_LEVEL_STYLE, "CC_FONT_CHOOSER_LEVEL_STYLE", "style" },
            { CC_FONT_CHOOSER_LEVEL_SIZE, "CC_FONT_CHOOSER_LEVEL_SIZE", "size" },
            { CC_FONT_CHOOSER_LEVEL_VARIATIONS, "CC_FONT_CHOOSER_LEVEL_VARIATIONS", "variations" },
            { CC_FONT_CHOOSER_LEVEL_FEATURES, "CC_FONT_CHOOSER_LEVEL_FEATURES", "features" },
            { 0, NULL, NULL }
        };
        etype = g_flags_register_static (g_intern_static_string ("CCFontChooserLevel"), values);
    }
    return etype;
}

typedef CcFontChooserIface CcFontChooserInterface;
G_DEFINE_INTERFACE (CcFontChooser, cc_font_chooser, G_TYPE_OBJECT);

static void
cc_font_chooser_default_init (CcFontChooserInterface *iface)
{
  g_object_interface_install_property
     (iface,
      g_param_spec_string ("font",
                          _("Font"),
                          _("Font description as a string, e.g. \"Sans Italic 12\""),
                          CC_FONT_CHOOSER_DEFAULT_FONT_NAME,
                          CC_PARAM_READWRITE));

  g_object_interface_install_property
     (iface,
      g_param_spec_boxed ("font-desc",
                          _("Font description"),
                          _("Font description as a PangoFontDescription struct"),
                          PANGO_TYPE_FONT_DESCRIPTION,
                          CC_PARAM_READWRITE));

  g_object_interface_install_property
     (iface,
      g_param_spec_string ("preview-text",
                          _("Preview text"),
                          _("The text to display in order to demonstrate the selected font"),
                          pango_language_get_sample_string (NULL),
                          CC_PARAM_READWRITE));

  g_object_interface_install_property
     (iface,
      g_param_spec_boolean ("show-preview-entry",
                          _("Show preview text entry"),
                          _("Whether the preview text entry is shown or not"),
                          TRUE,
                          CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property
     (iface,
      g_param_spec_flags ("level",
                          _("Selection level"),
                          _("Whether to select family, face or font"),
                          CC_TYPE_FONT_CHOOSER_LEVEL,
                          CC_FONT_CHOOSER_LEVEL_FAMILY |
                          CC_FONT_CHOOSER_LEVEL_STYLE |
                          CC_FONT_CHOOSER_LEVEL_SIZE,
                          CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property
     (iface,
      g_param_spec_string ("font-features",
                          _("Font features"),
                          _("Font features as a string"),
                          "",
                          CC_PARAM_READABLE));

  g_object_interface_install_property
     (iface,
      g_param_spec_string ("language",
                          _("Language"),
                          _("Language for which features have been selected"),
                          "",
                          CC_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  chooser_signals[SIGNAL_FONT_ACTIVATED] =
    g_signal_new (_("font-activated"),
                  CC_TYPE_FONT_CHOOSER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (CcFontChooserIface, font_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}

/**
 * cc_font_chooser_get_font_family:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the #PangoFontFamily representing the selected font family.
 * Font families are a collection of font faces.
 *
 * If the selected font is not installed, returns %NULL.
 *
 * Returns: (nullable) (transfer none): A #PangoFontFamily representing the
 *     selected font family, or %NULL. The returned object is owned by @fontchooser
 *     and must not be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFamily *
cc_font_chooser_get_font_family (CcFontChooser *fontchooser)
{
  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  return CC_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_family (fontchooser);
}

/**
 * cc_font_chooser_get_font_face:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the #PangoFontFace representing the selected font group
 * details (i.e. family, slant, weight, width, etc).
 *
 * If the selected font is not installed, returns %NULL.
 *
 * Returns: (nullable) (transfer none): A #PangoFontFace representing the
 *     selected font group details, or %NULL. The returned object is owned by
 *     @fontchooser and must not be modified or freed.
 *
 * Since: 3.2
 */
PangoFontFace *
cc_font_chooser_get_font_face (CcFontChooser *fontchooser)
{
  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  return CC_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_face (fontchooser);
}

/**
 * cc_font_chooser_get_font_size:
 * @fontchooser: a #CcFontChooser
 *
 * The selected font size.
 *
 * Returns: A n integer representing the selected font size,
 *     or -1 if no font size is selected.
 *
 * Since: 3.2
 */
gint
cc_font_chooser_get_font_size (CcFontChooser *fontchooser)
{
  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), -1);

  return CC_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_size (fontchooser);
}

/**
 * cc_font_chooser_get_font:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with
 * cc_font_chooser_set_font(), as the font chooser widget may
 * normalize font names and thus return a string with a different
 * structure. For example, “Helvetica Italic Bold 12” could be
 * normalized to “Helvetica Bold Italic 12”.
 *
 * Use pango_font_description_equal() if you want to compare two
 * font descriptions.
 *
 * Returns: (nullable) (transfer full): A string with the name
 *     of the current font, or %NULL if  no font is selected. You must
 *     free this string with g_free().
 *
 * Since: 3.2
 */
gchar *
cc_font_chooser_get_font (CcFontChooser *fontchooser)
{
  gchar *fontname;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font", &fontname, NULL);


  return fontname;
}

/**
 * cc_font_chooser_set_font:
 * @fontchooser: a #CcFontChooser
 * @fontname: a font name like “Helvetica 12” or “Times Bold 18”
 *
 * Sets the currently-selected font.
 *
 * Since: 3.2
 */
void
cc_font_chooser_set_font (CcFontChooser *fontchooser,
                           const gchar    *fontname)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (fontname != NULL);

  g_object_set (fontchooser, "font", fontname, NULL);
}

/**
 * cc_font_chooser_get_font_desc:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the currently-selected font.
 *
 * Note that this can be a different string than what you set with
 * cc_font_chooser_set_font(), as the font chooser widget may
 * normalize font names and thus return a string with a different
 * structure. For example, “Helvetica Italic Bold 12” could be
 * normalized to “Helvetica Bold Italic 12”.
 *
 * Use pango_font_description_equal() if you want to compare two
 * font descriptions.
 *
 * Returns: (nullable) (transfer full): A #PangoFontDescription for the
 *     current font, or %NULL if  no font is selected.
 *
 * Since: 3.2
 */
PangoFontDescription *
cc_font_chooser_get_font_desc (CcFontChooser *fontchooser)
{
  PangoFontDescription *font_desc;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font-desc", &font_desc, NULL);

  return font_desc;
}

/**
 * cc_font_chooser_set_font_desc:
 * @fontchooser: a #CcFontChooser
 * @font_desc: a #PangoFontDescription
 *
 * Sets the currently-selected font from @font_desc.
 *
 * Since: 3.2
 */
void
cc_font_chooser_set_font_desc (CcFontChooser             *fontchooser,
                                const PangoFontDescription *font_desc)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (font_desc != NULL);

  g_object_set (fontchooser, "font-desc", font_desc, NULL);
}

/**
 * cc_font_chooser_get_preview_text:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the text displayed in the preview area.
 *
 * Returns: (transfer full): the text displayed in the
 *     preview area
 *
 * Since: 3.2
 */
gchar *
cc_font_chooser_get_preview_text (CcFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "preview-text", &text, NULL);

  return text;
}

/**
 * cc_font_chooser_set_preview_text:
 * @fontchooser: a #CcFontChooser
 * @text: (transfer none): the text to display in the preview area
 *
 * Sets the text displayed in the preview area.
 * The @text is used to show how the selected font looks.
 *
 * Since: 3.2
 */
void
cc_font_chooser_set_preview_text (CcFontChooser *fontchooser,
                                   const gchar    *text)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (text != NULL);

  g_object_set (fontchooser, "preview-text", text, NULL);
}

/**
 * cc_font_chooser_get_show_preview_entry:
 * @fontchooser: a #CcFontChooser
 *
 * Returns whether the preview entry is shown or not.
 *
 * Returns: %TRUE if the preview entry is shown
 *     or %FALSE if it is hidden.
 *
 * Since: 3.2
 */
gboolean
cc_font_chooser_get_show_preview_entry (CcFontChooser *fontchooser)
{
  gboolean show;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), FALSE);

  g_object_get (fontchooser, "show-preview-entry", &show, NULL);

  return show;
}

/**
 * cc_font_chooser_set_show_preview_entry:
 * @fontchooser: a #CcFontChooser
 * @show_preview_entry: whether to show the editable preview entry or not
 *
 * Shows or hides the editable preview entry.
 *
 * Since: 3.2
 */
void
cc_font_chooser_set_show_preview_entry (CcFontChooser *fontchooser,
                                         gboolean        show_preview_entry)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));

  show_preview_entry = show_preview_entry != FALSE;
  g_object_set (fontchooser, "show-preview-entry", show_preview_entry, NULL);
}

/**
 * cc_font_chooser_set_filter_func:
 * @fontchooser: a #CcFontChooser
 * @filter: (allow-none): a #CcFontFilterFunc, or %NULL
 * @user_data: data to pass to @filter
 * @destroy: function to call to free @data when it is no longer needed
 *
 * Adds a filter function that decides which fonts to display
 * in the font chooser.
 *
 * Since: 3.2
 */
void
cc_font_chooser_set_filter_func (CcFontChooser   *fontchooser,
                                  CcFontFilterFunc filter,
                                  gpointer          user_data,
                                  GDestroyNotify    destroy)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));

  CC_FONT_CHOOSER_GET_IFACE (fontchooser)->set_filter_func (fontchooser,
                                                             filter,
                                                             user_data,
                                                             destroy);
}

void
_cc_font_chooser_font_activated (CcFontChooser *chooser,
                                  const gchar    *fontname)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (chooser));

  g_signal_emit (chooser, chooser_signals[SIGNAL_FONT_ACTIVATED], 0, fontname);
}

/**
 * cc_font_chooser_set_font_map:
 * @fontchooser: a #CcFontChooser
 * @fontmap: (allow-none): a #PangoFontMap
 *
 * Sets a custom font map to use for this font chooser widget.
 * A custom font map can be used to present application-specific
 * fonts instead of or in addition to the normal system fonts.
 *
 * |[<!-- language="C" -->
 * FcConfig *config;
 * PangoFontMap *fontmap;
 *
 * config = FcInitLoadConfigAndFonts ();
 * FcConfigAppFontAddFile (config, my_app_font_file);
 *
 * fontmap = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
 * pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (fontmap), config);
 *
 * cc_font_chooser_set_font_map (font_chooser, fontmap);
 * ]|
 *
 * Note that other GTK+ widgets will only be able to use the application-specific
 * font if it is present in the font map they use:
 *
 * |[
 * context = gtk_widget_get_pango_context (label);
 * pango_context_set_font_map (context, fontmap);
 * ]|
 *
 * Since: 3.18
 */
void
cc_font_chooser_set_font_map (CcFontChooser *fontchooser,
                               PangoFontMap   *fontmap)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));
  g_return_if_fail (fontmap == NULL || PANGO_IS_FONT_MAP (fontmap));

  if (CC_FONT_CHOOSER_GET_IFACE (fontchooser)->set_font_map)
    CC_FONT_CHOOSER_GET_IFACE (fontchooser)->set_font_map (fontchooser, fontmap);
}

/**
 * cc_font_chooser_get_font_map:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the custom font map of this font chooser widget,
 * or %NULL if it does not have one.
 *
 * Returns: (nullable) (transfer full): a #PangoFontMap, or %NULL
 *
 * Since: 3.18
 */
PangoFontMap *
cc_font_chooser_get_font_map (CcFontChooser *fontchooser)
{
  PangoFontMap *fontmap = NULL;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  if (CC_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_map)
    fontmap = CC_FONT_CHOOSER_GET_IFACE (fontchooser)->get_font_map (fontchooser);

  return fontmap;
}

/**
 * cc_font_chooser_set_level:
 * @fontchooser: a #CcFontChooser
 * @level: the desired level of granularity
 *
 * Sets the desired level of granularity for selecting fonts.
 *
 * Since: 3.24
 */
void
cc_font_chooser_set_level (CcFontChooser      *fontchooser,
                            CcFontChooserLevel  level)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));

  g_object_set (fontchooser, "level", level, NULL);
}

/**
 * cc_font_chooser_get_level:
 * @fontchooser: a #CcFontChooser
 *
 * Returns the current level of granularity for selecting fonts.
 *
 * Returns: the current granularity level
 *
 * Since: 3.24
 */
CcFontChooserLevel
cc_font_chooser_get_level (CcFontChooser *fontchooser)
{
  CcFontChooserLevel level;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), 0);

  g_object_get (fontchooser, "level", &level, NULL);

  return level;
}

/**
 * cc_font_chooser_get_font_features:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the currently-selected font features.
 *
 * Returns: the currently selected font features
 *
 * Since: 3.24
 */
char *
cc_font_chooser_get_font_features (CcFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "font-features", &text, NULL);

  return text;
}

/**
 * cc_font_chooser_get_language:
 * @fontchooser: a #CcFontChooser
 *
 * Gets the language that is used for font features.
 *
 * Returns: the currently selected language
 *
 * Since: 3.24
 */
char *
cc_font_chooser_get_language (CcFontChooser *fontchooser)
{
  char *text;

  g_return_val_if_fail (CC_IS_FONT_CHOOSER (fontchooser), NULL);

  g_object_get (fontchooser, "language", &text, NULL);

  return text;
}

/**
 * cc_font_chooser_set_language:
 * @fontchooser: a #CcFontChooser
 * @language: a language
 *
 * Sets the language to use for font features.
 *
 * Since: 3.24
 */
void
cc_font_chooser_set_language (CcFontChooser *fontchooser,
                               const char     *language)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (fontchooser));

  g_object_set (fontchooser, "language", language, NULL);
}
