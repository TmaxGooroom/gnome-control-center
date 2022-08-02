 /* GTK - The GIMP Toolkit
 * Copyright (C) 2011      Alberto Ruiz <aruiz@gnome.org>
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

#pragma

G_BEGIN_DECLS

#define CC_TYPE_FONT_CHOOSER_WIDGET           (cc_font_chooser_widget_get_type ())
#define CC_FONT_CHOOSER_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_FONT_CHOOSER_WIDGET, CcFontChooserWidget))
#define CC_FONT_CHOOSER_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_FONT_CHOOSER_WIDGET, CcFontChooserWidgetClass))
#define CC_IS_FONT_CHOOSER_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_FONT_CHOOSER_WIDGET))
#define CC_IS_FONT_CHOOSER_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_FONT_CHOOSER_WIDGET))
#define CC_FONT_CHOOSER_WIDGET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_FONT_CHOOSER_WIDGET, CcFontChooserWidgetClass))

typedef struct _CcFontChooserWidget              CcFontChooserWidget;
typedef struct _CcFontChooserWidgetPrivate       CcFontChooserWidgetPrivate;
typedef struct _CcFontChooserWidgetClass         CcFontChooserWidgetClass;

struct _CcFontChooserWidget
{
  GtkBox                      parent_instance;
  CcFontChooserWidgetPrivate *priv;
};

struct _CcFontChooserWidgetClass
{
  GtkBoxClass parent_class;
};

GtkWidget*    cc_font_chooser_widget_new      (void);
/* private */
GAction*      cc_font_chooser_widget_get_tweak_action (GtkWidget *widget);

G_END_DECLS
