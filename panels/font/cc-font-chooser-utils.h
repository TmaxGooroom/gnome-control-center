/* gtkfontchooserutils.h - Private utility functions for implementing a
 *                           GtkFontChooser interface
 *
 * Copyright (C) 2006 Emmanuele Bassi
 * Copyright (C) 2020 gooroom <gooroom@gooroom.kr>
 *
 * All rights reserved
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
 *
 * Based on gtkfilechooserutils.h:
 *	Copyright (C) 2003 Red Hat, Inc.
 */
 
#ifndef __CC_FONT_CHOOSER_UTILS_H__
#define __CC_FONT_CHOOSER_UTILS_H__

#include "cc-font-chooser-private.h"
G_BEGIN_DECLS

#define CC_FONT_CHOOSER_DELEGATE_QUARK	(_cc_font_chooser_delegate_get_quark ())

void   _cc_font_chooser_install_properties  (GObjectClass       *klass);
void   _cc_font_chooser_delegate_iface_init (CcFontChooserIface *iface);
void   _cc_font_chooser_set_delegate        (CcFontChooser      *receiver,
                                             CcFontChooser      *delegate);

GQuark _cc_font_chooser_delegate_get_quark  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CC_FONT_CHOOSER_UTILS_H__ */
