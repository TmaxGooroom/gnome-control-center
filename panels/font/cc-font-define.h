/* GTK - The GIMP Toolkit
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __CC_FONT_DEFINE_H_
#define __CC_FONT_DEFINE_H_

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* This library is copy from gtkprivate.h */
#define CC_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB 
#define CC_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB 
#define CC_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

/* This library is copy from gtkfontchooser.h */
typedef enum {
  CC_FONT_CHOOSER_LEVEL_FAMILY     = 0,
  CC_FONT_CHOOSER_LEVEL_STYLE      = 1 << 0,
  CC_FONT_CHOOSER_LEVEL_SIZE       = 1 << 1,
  CC_FONT_CHOOSER_LEVEL_VARIATIONS = 1 << 2,
  CC_FONT_CHOOSER_LEVEL_FEATURES   = 1 << 3
} CcFontChooserLevel;

/* This library is copy from gtkfontchooserutils.h */
typedef enum {
  CC_FONT_CHOOSER_PROP_FIRST           = 0x4000,
  CC_FONT_CHOOSER_PROP_FONT,
  CC_FONT_CHOOSER_PROP_FONT_DESC,
  CC_FONT_CHOOSER_PROP_PREVIEW_TEXT,
  CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY,
  CC_FONT_CHOOSER_PROP_LEVEL,
  CC_FONT_CHOOSER_PROP_FONT_FEATURES,
  CC_FONT_CHOOSER_PROP_LANGUAGE,
  CC_FONT_CHOOSER_PROP_LAST
} CcFontChooserProp;

G_END_DECLS

#endif /* __CC_FONT_DEFINE_H_ */
