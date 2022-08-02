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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */



#pragma

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_FONT_CHOOSER_DIALOG               (cc_font_chooser_dialog_get_type ())
#define CC_FONT_CHOOSER_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_FONT_CHOOSER_DIALOG, CcFontChooserDialog))
#define CC_FONT_CHOOSER_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_FONT_CHOOSER_DIALOG, CcFontChooserDialogClass))
#define CC_IS_FONT_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_FONT_CHOOSER_DIALOG))
#define CC_IS_FONT_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_FONT_CHOOSER_DIALOG))
#define CC_FONT_CHOOSER_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_FONT_CHOOSER_DIALOG, CcFontChooserDialogClass))

typedef struct _CcFontChooserDialog               CcFontChooserDialog;
typedef struct _CcFontChooserDialogClass          CcFontChooserDialogClass;
typedef struct _CcFontChooserDialogPrivate        CcFontChooserDialogPrivate;

struct _CcFontChooserDialog
{
  GtkDialog                   parent_instance;

  CcFontChooserDialogPrivate *priv;
};

struct _CcFontChooserDialogClass
{
  GtkDialogClass parent_class;
};

GtkWidget *
cc_font_chooser_dialog_new        (gchar *title, GtkWindow *parent);

G_END_DECLS
