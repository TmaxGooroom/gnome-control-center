#ifndef __CC_BACKGROUND_CHOOSER_H__
#define __CC_BACKGROUND_CHOOSER_H__

#include <gtk/gtk.h>
G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_CHOOSER (cc_background_chooser_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundChooser, cc_background_chooser, CC, BACKGROUND_CHOOSER, GtkBox)

G_END_DECLS

#endif /* __CC_BACKGROUND_CHOOSER_H__ */
