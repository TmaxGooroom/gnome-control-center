/* gtkfontprivatechooser.h - Interface definitions for font selectors UI
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
 */

#ifndef __CC_FONT_CHOOSER_PRIVATE_H__
#define __CC_FONT_CHOOSER_PRIVATE_H__

#include "cc-font-chooser.h"

#define CC_FONT_CHOOSER_DEFAULT_FONT_NAME "Sans 10"

G_BEGIN_DECLS

void            _cc_font_chooser_font_activated        (CcFontChooser *chooser,
                                                         const gchar    *fontname);

G_END_DECLS

#endif /* ! __CC_FONT_CHOOSER_PRIVATE_H__ */
