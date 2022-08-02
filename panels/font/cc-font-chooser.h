/* GTK - The GIMP Toolkit
 * gtkfontchooser.h - Abstract interface for font file selectors GUIs
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

#ifndef __CC_FONT_CHOOSER_H__
#define __CC_FONT_CHOOSER_H__

#include <gtk/gtk.h>
#include "cc-font-define.h"

G_BEGIN_DECLS

/**
 * CcFontFilterFunc:
 * @family: a #PangoFontFamily
 * @face: a #PangoFontFace belonging to @family
 * @data: (closure): user data passed to cc_font_chooser_set_filter_func()
 *
 * The type of function that is used for deciding what fonts get
 * shown in a #CcFontChooser. See cc_font_chooser_set_filter_func().
 *
 * Returns: %TRUE if the font should be displayed
 */
typedef gboolean (*CcFontFilterFunc) (const PangoFontFamily *family,
                                       const PangoFontFace   *face,
                                       gpointer               data);

#define CC_TYPE_FONT_CHOOSER			(cc_font_chooser_get_type ())
#define CC_FONT_CHOOSER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_FONT_CHOOSER, CcFontChooser))
#define CC_IS_FONT_CHOOSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_FONT_CHOOSER))
#define CC_FONT_CHOOSER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), CC_TYPE_FONT_CHOOSER, CcFontChooserIface))

typedef struct _CcFontChooser      CcFontChooser; /* dummy */
typedef struct _CcFontChooserIface CcFontChooserIface;

/* font chooser level type */
GType cc_font_chooser_level_get_type (void) G_GNUC_CONST;
#define CC_TYPE_FONT_CHOOSER_LEVEL (cc_font_chooser_level_get_type ())

struct _CcFontChooserIface
{
  GTypeInterface base_iface;

  /* Methods */
  PangoFontFamily * (* get_font_family)         (CcFontChooser  *fontchooser);
  PangoFontFace *   (* get_font_face)           (CcFontChooser  *fontchooser);
  gint              (* get_font_size)           (CcFontChooser  *fontchooser);

  void              (* set_filter_func)         (CcFontChooser   *fontchooser,
                                                 CcFontFilterFunc filter,
                                                 gpointer          user_data,
                                                 GDestroyNotify    destroy);

  /* Signals */
  void (* font_activated) (CcFontChooser *chooser,
                           const gchar    *fontname);

  /* More methods */
  void              (* set_font_map)            (CcFontChooser   *fontchooser,
                                                 PangoFontMap     *fontmap);
  PangoFontMap *    (* get_font_map)            (CcFontChooser   *fontchooser);

   /* Padding */
  gpointer padding[10];
};

GDK_AVAILABLE_IN_3_2
GType            cc_font_chooser_get_type                 (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_2
PangoFontFamily *cc_font_chooser_get_font_family          (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_2
PangoFontFace   *cc_font_chooser_get_font_face            (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_2
gint             cc_font_chooser_get_font_size            (CcFontChooser   *fontchooser);

GDK_AVAILABLE_IN_3_2
PangoFontDescription *
                 cc_font_chooser_get_font_desc            (CcFontChooser             *fontchooser);
GDK_AVAILABLE_IN_3_2
void             cc_font_chooser_set_font_desc            (CcFontChooser             *fontchooser,
                                                            const PangoFontDescription *font_desc);

GDK_AVAILABLE_IN_3_2
gchar*           cc_font_chooser_get_font                 (CcFontChooser   *fontchooser);

GDK_AVAILABLE_IN_3_2
void             cc_font_chooser_set_font                 (CcFontChooser   *fontchooser,
                                                            const gchar      *fontname);
GDK_AVAILABLE_IN_3_2
gchar*           cc_font_chooser_get_preview_text         (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_2
void             cc_font_chooser_set_preview_text         (CcFontChooser   *fontchooser,
                                                            const gchar      *text);
GDK_AVAILABLE_IN_3_2
gboolean         cc_font_chooser_get_show_preview_entry   (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_2
void             cc_font_chooser_set_show_preview_entry   (CcFontChooser   *fontchooser,
                                                            gboolean          show_preview_entry);
GDK_AVAILABLE_IN_3_2
void             cc_font_chooser_set_filter_func          (CcFontChooser   *fontchooser,
                                                            CcFontFilterFunc filter,
                                                            gpointer          user_data,
                                                            GDestroyNotify    destroy);
GDK_AVAILABLE_IN_3_18
void             cc_font_chooser_set_font_map             (CcFontChooser   *fontchooser,
                                                            PangoFontMap     *fontmap);
GDK_AVAILABLE_IN_3_18
PangoFontMap *   cc_font_chooser_get_font_map             (CcFontChooser   *fontchooser);

GDK_AVAILABLE_IN_3_24
void             cc_font_chooser_set_level                (CcFontChooser   *fontchooser,
                                                            CcFontChooserLevel level);
GDK_AVAILABLE_IN_3_24
CcFontChooserLevel
                 cc_font_chooser_get_level                (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_24
char *           cc_font_chooser_get_font_features        (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_24
char *           cc_font_chooser_get_language             (CcFontChooser   *fontchooser);
GDK_AVAILABLE_IN_3_24
void             cc_font_chooser_set_language             (CcFontChooser   *fontchooser,
                                                            const char       *language);

G_END_DECLS

#endif /* __CC_FONT_CHOOSER_H__ */
