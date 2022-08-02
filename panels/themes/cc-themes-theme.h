#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define  CC_TYPE_THEMES_THEME          (cc_themes_theme_get_type ())
#define  CC_THEMES_THEME(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_THEMES_THEME, CcThemesTheme))
#define  CC_IS_THEME_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_THEMES_THEME))

//G_DECLARE_FINAL_TYPE (CcThemesTheme, cc_themes_theme, CC, THEME_BOX, GtkToggleButton)
typedef struct _CcThemesTheme          CcThemesTheme;
typedef struct _CcThemesThemeClass     CcThemesThemeClass;

struct _CcThemesThemeClass {
    GtkBoxClass  parent_class;
};

CcThemesTheme       *cc_themes_theme_new ();
gchar               *cc_themes_theme_get_icon       (CcThemesTheme *self);

G_END_DECLS
