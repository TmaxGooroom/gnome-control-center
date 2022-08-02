/* GTK - The GIMP Toolkit
 * Copyright (C) 1998 David Abilleira Freijeiro <odaf@nexo.es>
 * All rights reserved
 * Based on gnome-color-picker by Federico Mena <federico@nuclecu.unam.mx>
 * Copyright (C) 2020 gooroom <gooroom@gooroom.kr>
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

 #pragma

 #include <gtk/gtk.h>

 G_BEGIN_DECLS

#define CC_TYPE_FONT_BUTTON             (cc_font_button_get_type ())
#define CC_FONT_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_FONT_BUTTON, CcFontButton))
#define CC_FONT_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_FONT_BUTTON, CcFontButtonClass))
#define CC_IS_FONT_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_FONT_BUTTON))
#define CC_IS_FONT_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_FONT_BUTTON))
#define CC_FONT_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_FONT_BUTTON, CcFontButtonClass))

typedef struct _CcFontButton        CcFontButton;
typedef struct _CcFontButtonClass   CcFontButtonClass;
typedef struct _CcFontButtonPrivate CcFontButtonPrivate;

struct _CcFontButton {
  GtkButton button;

  /*< private >*/
  CcFontButtonPrivate *priv;
};

struct _CcFontButtonClass {
  GtkButtonClass parent_class;

  /* font_set signal is emitted when font is chosen */
  void (* font_set) (CcFontButton *gfp);

  /* Padding for future expansion */
  //void (*_gtk_reserved1) (void);
  //void (*_gtk_reserved2) (void);
  //void (*_gtk_reserved3) (void);
  //void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_ALL
GType                 cc_font_button_get_type       (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget            *cc_font_button_new            (void);
GDK_AVAILABLE_IN_ALL
GtkWidget            *cc_font_button_new_with_font  (const gchar   *fontname);

GDK_AVAILABLE_IN_ALL
const gchar *         cc_font_button_get_title      (CcFontButton *font_button);
GDK_AVAILABLE_IN_ALL
void                  cc_font_button_set_title      (CcFontButton *font_button,
                                                      const gchar   *title);
GDK_AVAILABLE_IN_ALL
gboolean              cc_font_button_get_use_font   (CcFontButton *font_button);
GDK_AVAILABLE_IN_ALL
void                  cc_font_button_set_use_font   (CcFontButton *font_button,
                                                      gboolean       use_font);
GDK_AVAILABLE_IN_ALL
gboolean              cc_font_button_get_use_size   (CcFontButton *font_button);
GDK_AVAILABLE_IN_ALL
void                  cc_font_button_set_use_size   (CcFontButton *font_button,
                                                      gboolean       use_size);

GDK_DEPRECATED_IN_3_22
const gchar *         cc_font_button_get_font_name  (CcFontButton *font_button);
GDK_DEPRECATED_IN_3_22
gboolean              cc_font_button_set_font_name  (CcFontButton *font_button,
                                                      const gchar   *fontname);
GDK_AVAILABLE_IN_ALL
gboolean              cc_font_button_get_show_style (CcFontButton *font_button);
GDK_AVAILABLE_IN_ALL
void                  cc_font_button_set_show_style (CcFontButton *font_button,
                                                      gboolean       show_style);
GDK_AVAILABLE_IN_ALL
gboolean              cc_font_button_get_show_size  (CcFontButton *font_button);
GDK_AVAILABLE_IN_ALL
void                  cc_font_button_set_show_size  (CcFontButton *font_button,
                                                      gboolean       show_size);
 G_END_DECLS
