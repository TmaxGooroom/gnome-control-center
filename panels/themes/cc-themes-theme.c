#include <config.h>

#include "cc-themes-resources.h"
#include "cc-themes-theme.h"

#define CHECK_ICON  "/org/gnome/control-center/themes/check_icon"
#define THUMBNAIL_WIDTH 300
#define THUMBNAIL_HEIGHT 188

struct _CcThemesTheme
{
  GtkBox       parent;

  GtkCssProvider *provider;
  GtkCssProvider *selected_prov;
  GtkCssProvider *default_prov;

  GtkWidget   *overlay;
  GtkWidget   *thumbnail_box;
  GtkWidget   *thumbnail_image;
  GtkWidget   *label;

  gchar       *icon;
  gchar       *background;
  gchar       *thumbnail_path;
};

enum {
  PROP_LABEL = 1,
  PROP_ICON,
  PROP_BACKGROUND,
  PROP_THUMBNAIL,
  PROP_ACTIVE,
};

G_DEFINE_TYPE (CcThemesTheme, cc_themes_theme, GTK_TYPE_BOX)

gchar *
cc_themes_theme_get_icon (CcThemesTheme *self)
{
  return self->icon;
}

static void
set_active (CcThemesTheme *self, gboolean active)
{
  GtkWidget *icon;
  icon = g_object_get_data (G_OBJECT (self->overlay), "selected-icon");

  if (active) {
    gtk_style_context_add_provider (gtk_widget_get_style_context (self->thumbnail_box),
                                    GTK_STYLE_PROVIDER (self->selected_prov),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  else {
    gtk_style_context_add_provider (gtk_widget_get_style_context (self->thumbnail_box),
                                    GTK_STYLE_PROVIDER (self->default_prov),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }

  gtk_widget_set_visible (icon, active);
}

static void
set_name (CcThemesTheme *self, const gchar *value)
{
  gtk_label_set_text (GTK_LABEL (self->label), value);
}

static void
set_icon (CcThemesTheme *self, const gchar *value)
{
  self->icon = g_strdup (value);
}

static void
set_background_path (CcThemesTheme *self, const gchar *value)
{
  self->background = g_strdup (value);
}

static void
set_thumbnail (CcThemesTheme *self, const gchar *value)
{
  GdkPixbuf *pixbuf;
  GtkWidget *icon;
  g_autoptr(GError)  error = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_size (value, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, &error);
  if (error) {
    g_warning ("Error pixbuf new: %s", error->message);
    return;
  }

  gtk_image_set_from_pixbuf (GTK_IMAGE (self->thumbnail_image), pixbuf);
  self->thumbnail_path = g_strdup (value);

  /* selection icon */
  icon = g_object_new (GTK_TYPE_IMAGE,
                       "resource", CHECK_ICON,
                       "pixel-size", 32,
                       "halign", GTK_ALIGN_END,
                       "valign", GTK_ALIGN_START,
                       "visible", FALSE, NULL);

  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), icon);
  g_object_set_data_full (G_OBJECT (self->overlay), "selected-icon", g_object_ref (icon), g_object_unref);
}

static void
cc_themes_theme_set_property (GObject       *object,
                            guint          prop_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
  CcThemesTheme *self = CC_THEMES_THEME (object);

  switch (prop_id) {
    case PROP_LABEL:
      set_name (self, g_value_get_string (value));
    break;
    case PROP_ICON:
      set_icon (self, g_value_get_string (value));
    break;
    case PROP_BACKGROUND:
      set_background_path (self, g_value_get_string (value));
    break;
    case PROP_THUMBNAIL:
      set_thumbnail (self, g_value_get_string (value));
    break;
    case PROP_ACTIVE:
      set_active (self, g_value_get_boolean (value));
    break;
  }
}

static void
cc_themes_theme_get_property (GObject       *object,
                              guint          prop_id,
                              GValue  *value,
                              GParamSpec    *pspec)
{
  CcThemesTheme *self = CC_THEMES_THEME (object);

  switch (prop_id) {
    case PROP_ICON:
      g_value_set_string (value, self->icon);
    break;
    case PROP_BACKGROUND:
      g_value_set_string (value, self->background);
    break;
    case PROP_THUMBNAIL:
      g_value_set_string (value, self->thumbnail_path);
    break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_themes_theme_finalize (GObject *object)
{
  CcThemesTheme *self = CC_THEMES_THEME (object);

  g_free (self->icon);
  g_free (self->background);
  g_free (self->thumbnail_path);

  g_clear_object (&self->provider);
  g_clear_object (&self->selected_prov);
  g_clear_object (&self->default_prov);
}

static void
cc_themes_theme_constructed (GObject *object)
{
  CcThemesTheme *self = CC_THEMES_THEME (object);
}

static void
cc_themes_theme_class_init (CcThemesThemeClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_themes_theme_get_property;
  object_class->set_property = cc_themes_theme_set_property;
  object_class->finalize = cc_themes_theme_finalize;
  object_class->constructed = cc_themes_theme_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/themes/themes-theme.ui");

  gtk_widget_class_bind_template_child (widget_class, CcThemesTheme, thumbnail_box);
  gtk_widget_class_bind_template_child (widget_class, CcThemesTheme, overlay);
  gtk_widget_class_bind_template_child (widget_class, CcThemesTheme, thumbnail_image);
  gtk_widget_class_bind_template_child (widget_class, CcThemesTheme, label);

  g_object_class_install_property (object_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label-text",
                                                        "label-text",
                                                        "label-text",
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        "icon",
                                                        "icon",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        "background",
                                                        "background",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_THUMBNAIL,
                                   g_param_spec_string ("thumbnail",
                                                        "thumbnail",
                                                        "thumbnail",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "active",
                                                         "active",
                                                         FALSE,
                                                         G_PARAM_WRITABLE));
}

static void
cc_themes_theme_init (CcThemesTheme *self)
{
  GFile *file = NULL;
  g_resources_register (cc_themes_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->icon = NULL;
  self->background = NULL;
  self->provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (self->provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  self->selected_prov = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (self->selected_prov,
".theme-box {"
"background-color: #3986e1;"
"}", -1, NULL);

  self->default_prov = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (self->default_prov,
".theme-box {"
"background-color: rgba(201,201,201,0.0);"
"}", -1, NULL);

  file = g_file_new_for_uri ("resource://org/gnome/control-center/themes/style.css");
  gtk_css_provider_load_from_file (self->provider, file, NULL);
  g_object_unref (file);
}

CcThemesTheme *
cc_themes_theme_new ()
{
  return CC_THEMES_THEME (g_object_new (CC_TYPE_THEMES_THEME, NULL));
}
