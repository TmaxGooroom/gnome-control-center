/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2019 gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _CC_THEMES_PANEL_H
#define _CC_THEMES_PANEL_H

#include <shell/cc-panel.h>

#define CC_TYPE_THEMES_PANEL (cc_themes_panel_get_type ())

G_DECLARE_FINAL_TYPE (CcThemesPanel, cc_themes_panel, CC, THEMES_PANEL, CcPanel)

G_END_DECLS

#endif /* _CC_THEMES_PANEL_H */
