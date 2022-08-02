/*
 * Copyright (C) 2010 Intel, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-chooser.h"
#include "cc-background-item.h"
//#include "cc-background-preview.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#include "bg-colors-source.h"
//#include "bg-pictures-source.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_LOCK_PATH_ID "org.gnome.desktop.screensaver"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

#define CHECK_ICON    "/org/gnome/control-center/background/check_icon"

enum {
  COMBO_BOX_MODEL_TEXT,
  COMBO_BOX_MODEL_VALUE,
  N_COLUMNS
};

typedef enum {
  MODE_EACH = 0,
  MODE_ALL,
}BackgroundMode;

typedef enum {
  APPLY_DESKTOP,
  APPLY_LOCK,
  APPLY_ALL,
}BackgroundApply;

struct _CcBackgroundPanel
{
  CcPanel parent_instance;

  GDBusConnection *connection;

  GSettings *settings;
  GSettings *lock_settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;
  CcBackgroundItem *current_lock_background;
  gchar            *prior_lock_background_uri;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;
  GtkWidget *chooser;

//  CcBackgroundChooser *background_chooser;
//  GtkButton *add_picture_button;
//  CcBackgroundPreview *desktop_preview;

  GtkCssProvider  *provider;
  GtkCssProvider  *selected_prov;
  GtkCssProvider  *default_prov;

  /* from ui */
  GtkWidget       *scrolled_view;
  GtkWidget       *type_label;
  GtkWidget       *type_select_combobox;

  GtkWidget       *desktop_thumbnail_box;
  GtkWidget       *desktop_eventbox;
  GtkWidget       *desktop_overlay;
  GtkWidget       *desktop_image;
  GtkWidget       *desktop_label;

  GtkWidget       *lock_box;            // need for vislble,disable
  GtkWidget       *lock_thumbnail_box;
  GtkWidget       *lock_eventbox;
  GtkWidget       *lock_overlay;
  GtkWidget       *lock_image;
  GtkWidget       *lock_label;

  BackgroundMode      background_mode;
  BackgroundApply     applied_background;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

//static void
//update_preview (CcBackgroundPanel *panel)
//{
//  CcBackgroundItem *current_background;
//
//  current_background = panel->current_background;
//  cc_background_preview_set_item (panel->desktop_preview, current_background);
//}

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static CcBackgroundItem *
get_current_background (CcBackgroundPanel *panel, GSettings *settings)
{
  if (settings == panel->settings)
    return panel->current_background;
  else
    return panel->current_lock_background;
}

static gchar *
get_save_path (CcBackgroundPanel *panel, GSettings *settings)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           settings == panel->settings ? "last-edited.xml" : "last-edited-lock.xml",
                           NULL);
}

static GdkPixbuf*
get_or_create_cached_pixbuf (CcBackgroundPanel *panel,
                             GtkWidget         *widget,
                             CcBackgroundItem  *background)
{
  GtkAllocation allocation;
  const gint preview_width = 300;
  const gint preview_height = 188;
  gint scale_factor;
  GdkPixbuf *pixbuf;

  pixbuf = g_object_get_data (G_OBJECT (background), "pixbuf");
  if (pixbuf == NULL)
    {
      gtk_widget_get_allocation (widget, &allocation);
      scale_factor = gtk_widget_get_scale_factor (widget);
      pixbuf = cc_background_item_get_frame_thumbnail (background,
                                                       panel->thumb_factory,
                                                       preview_width,
                                                       preview_height,
                                                       scale_factor,
                                                       -2, TRUE);
      g_object_set_data_full (G_OBJECT (background), "pixbuf", pixbuf, g_object_unref);
    }

  return pixbuf;
}

static void
set_preview_image (CcBackgroundPanel *panel,
                   GtkWidget         *widget,
                   CcBackgroundItem  *item)
{
  GdkPixbuf *pixbuf;

  pixbuf = get_or_create_cached_pixbuf (panel, widget, item);
  gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);
}

static void
update_preview (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  gboolean changes_with_time;
  CcBackgroundItem *current_background;

  current_background = get_current_background (panel, settings);

  if (item && current_background)
    {
      g_object_unref (current_background);
      current_background = cc_background_item_copy (item);
      if (settings == panel->settings)
        panel->current_background = current_background;
      else
        panel->current_lock_background = current_background;
      cc_background_item_load (current_background, NULL);
    }

  changes_with_time = FALSE;

  if (current_background)
    {
      changes_with_time = cc_background_item_changes_with_time (current_background);
    }

  if (settings == panel->settings)
    {
      set_preview_image (panel, panel->desktop_image,
                         item == NULL ? panel->current_background : item);
    }
  else
    {
      set_preview_image (panel, panel->lock_image,
                         item == NULL ? panel->current_lock_background : item);
    }
}

static void
update_display_preview (CcBackgroundPanel *panel,
                        GtkWidget         *widget,
                        CcBackgroundItem  *background)
{
  GdkPixbuf *pixbuf;
  cairo_t *cr;
  GdkRGBA color;
  cairo_region_t *cairo_region;
  GdkDrawingContext *drawing_context;
  GdkWindow *window;

  pixbuf = get_or_create_cached_pixbuf (panel, widget, background);

  cr = gdk_cairo_create (gtk_widget_get_window (widget));
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static void
reload_current_bg (CcBackgroundPanel *panel,
                   GSettings         *settings)
{
  g_autoptr(CcBackgroundItem) saved = NULL;
  CcBackgroundItem *configured;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;

  /* Load the saved configuration */
  uri = get_save_path (panel, settings);
  saved = cc_background_xml_get_item (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    g_clear_pointer (&uri, g_free);


  configured = cc_background_item_new (uri);

  pcolor = g_settings_get_string (settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "placement", g_settings_get_enum (settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);

  if (saved != NULL && cc_background_item_compare (saved, configured))
    {
      CcBackgroundItemFlags flags;
      flags = cc_background_item_get_flags (saved);
      /* Special case for colours */
      if (cc_background_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_BACKGROUND_ITEM_HAS_PCOLOR | CC_BACKGROUND_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
		    "name", cc_background_item_get_name (saved),
		    "flags", flags,
		    "source-url", cc_background_item_get_source_url (saved),
		    "source-xml", cc_background_item_get_source_xml (saved),
		    NULL);
    }

  if (settings == panel->settings)
    {
      g_clear_object (&panel->current_background);
      panel->current_background = configured;
    }
  else
    {
      g_clear_object (&panel->current_lock_background);
      panel->current_lock_background = configured;
    }
  cc_background_item_load (configured, NULL);
}

static gboolean
create_save_dir (void)
{
  g_autofree char *path = NULL;

  path = g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   NULL);
  if (g_mkdir_with_parents (path, USER_DIR_MODE) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      return FALSE;
    }
  return TRUE;
}

static void
set_background (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  GDesktopBackgroundStyle style;
  CcBackgroundItemFlags flags;
  g_autofree gchar *filename = NULL;
  const char *uri;

  if (item == NULL)
    return;

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  if ((flags & CC_BACKGROUND_ITEM_HAS_URI) && uri == NULL)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (settings, WP_URI_KEY, "");
    }
  else
    {
      g_settings_set_string (settings, WP_URI_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      style = g_settings_get_enum (settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    g_settings_set_enum (settings, WP_SHADING_KEY, cc_background_item_get_shading (item));

  g_settings_set_string (settings, WP_PCOLOR_KEY, cc_background_item_get_pcolor (item));
  g_settings_set_string (settings, WP_SCOLOR_KEY, cc_background_item_get_scolor (item));

  /* Apply all changes */
  g_settings_apply (settings);

  /* Save the source XML if there is one */
  filename = get_save_path (panel, settings);
  if (create_save_dir ())
    cc_background_xml_save (panel->current_background, filename);
}

static gboolean
on_preview_draw (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcBackgroundPanel *panel)
{
  update_display_preview (panel, widget, panel->current_background);

  return TRUE;
}

static gboolean
on_lock_preview_draw (GtkWidget         *widget,
                      cairo_t           *cr,
                      CcBackgroundPanel *panel)
{
  update_display_preview (panel, widget, panel->current_lock_background);
  return TRUE;
}

static void
on_select_background (CcBackgroundChooser     *chooser,
                      CcBackgroundItem        *item,
                      CcBackgroundPanel       *panel)
{
  switch (panel->applied_background) {
    case APPLY_DESKTOP:
      set_background (panel, panel->settings, item);
    break;
    case APPLY_LOCK:
      set_background (panel, panel->lock_settings, item);
    break;
    default: // APPLY_ALL
      set_background (panel, panel->settings, item);
      set_background (panel, panel->lock_settings, item);
  }
}

static void
set_visible_checked_icon (GtkWidget *thumbnail_box, GtkCssProvider *css, GtkWidget *overlay, gboolean visible)
{
  GtkWidget *icon;

  gtk_style_context_add_provider (gtk_widget_get_style_context (thumbnail_box),
                                  GTK_STYLE_PROVIDER (css),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  icon = g_object_get_data (G_OBJECT (overlay), "selected-icon");
  gtk_widget_set_visible (icon, visible);
}

static void
on_type_changed (GtkComboBox       *combo_box,
                 CcBackgroundPanel *panel)
{
  GtkWidget *stack, *box;

  panel->background_mode = gtk_combo_box_get_active (combo_box);

  box = panel->lock_box;
  if (panel->background_mode == MODE_EACH) {
    gtk_widget_set_visible (box, TRUE);

    set_visible_checked_icon (panel->desktop_thumbnail_box,
                              panel->selected_prov,
                              panel->desktop_overlay,
                              TRUE);

    set_visible_checked_icon (panel->lock_thumbnail_box,
                              panel->default_prov,
                              panel->lock_overlay,
                              FALSE);

    panel->background_mode = MODE_EACH;
    panel->applied_background = APPLY_DESKTOP;

    if (panel->prior_lock_background_uri) { 
      CcBackgroundItem *prior_lock_background = NULL;

      prior_lock_background = cc_background_item_new (panel->prior_lock_background_uri);
      set_background (panel, panel->lock_settings, prior_lock_background);

      panel->prior_lock_background_uri = NULL;
      g_clear_object (&prior_lock_background);
    }
    else
      set_background (panel, panel->lock_settings, panel->current_lock_background);
  }
  else if (panel->background_mode == MODE_ALL) {
    set_visible_checked_icon (panel->desktop_thumbnail_box,
                              panel->default_prov,
                              panel->desktop_overlay,
                              FALSE);
    set_visible_checked_icon (panel->lock_thumbnail_box,
                              panel->default_prov,
                              panel->lock_overlay,
                              FALSE);

    gtk_widget_set_visible (box, FALSE);

    panel->applied_background = APPLY_ALL;
    panel->prior_lock_background_uri = g_strdup (cc_background_item_get_uri (panel->current_lock_background));
    set_background (panel, panel->lock_settings, panel->current_background);
  }
}

static gboolean
on_desktop_press (GtkWidget         *widget,
                  GdkEvent          *event,
                  CcBackgroundPanel *panel)
{ 
  if (panel->background_mode == MODE_EACH) {
    set_visible_checked_icon (panel->desktop_thumbnail_box,
                              panel->selected_prov,
                              panel->desktop_overlay,
                              TRUE);
    set_visible_checked_icon (panel->lock_thumbnail_box,
                              panel->default_prov,
                              panel->lock_overlay,
                              FALSE);
    
    panel->applied_background = APPLY_DESKTOP;
  }
  
  return FALSE;
}

static gboolean
on_lock_press (GtkWidget         *widget,
               GdkEvent          *event,
               CcBackgroundPanel *panel)
{
  set_visible_checked_icon (panel->desktop_thumbnail_box,
                            panel->default_prov,
                            panel->desktop_overlay,
                            FALSE);
  set_visible_checked_icon (panel->lock_thumbnail_box,
                            panel->selected_prov,
                            panel->lock_overlay,
                            TRUE);

    gtk_style_context_add_provider (gtk_widget_get_style_context (panel->lock_thumbnail_box),
                                    GTK_STYLE_PROVIDER (panel->selected_prov),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  panel->applied_background = APPLY_LOCK;

  return FALSE;
}

static void
on_settings_changed (GSettings         *settings,
                     gchar             *key,
                     CcBackgroundPanel *panel)
{
  reload_current_bg (panel, settings);
  update_preview (panel, settings, NULL);
}

static GtkWidget*
create_selected_icon_in_overlay (GtkOverlay *overlay, const gchar *icon_name)
{
  GtkWidget* icon;
  icon = g_object_new (GTK_TYPE_IMAGE,
                       "resource", CHECK_ICON,
                       "pixel-size", 32,
                       "halign", GTK_ALIGN_END,
                       "valign", GTK_ALIGN_START,
                       "visible", FALSE, NULL);

  gtk_overlay_add_overlay (overlay, icon);
  g_object_set_data_full (G_OBJECT (overlay), "selected-icon", g_object_ref (icon), g_object_unref);

  return icon;
}

//static void
//on_settings_changed (CcBackgroundPanel *panel)
//{
//  reload_current_bg (panel);
//  update_preview (panel);
//}

//static void
//on_chooser_background_chosen_cb (CcBackgroundPanel          *self,
//                                 CcBackgroundItem           *item)
//{
//  set_background (self, self->settings, item);
//  set_background (self, self->lock_settings, item);
//}

//static void
//on_add_picture_button_clicked_cb (CcBackgroundPanel *self)
//{
//  cc_background_chooser_select_file (self->background_chooser);
//}

static void
cc_background_panel_dispose (GObject *object)
{ 
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);
  
  /* destroying the builder object will also destroy the spinner */
  panel->spinner = NULL;
  
  g_clear_object (&panel->settings);
  g_clear_object (&panel->lock_settings);
  
  if (panel->copy_cancellable)
    { 
      /* cancel any copy operation */
      g_cancellable_cancel (panel->copy_cancellable);

      g_clear_object (&panel->copy_cancellable);
    }

  if (panel->chooser)
    {
      gtk_widget_destroy (panel->chooser);
      panel->chooser = NULL;
    }

  g_clear_object (&panel->thumb_factory);

  g_clear_object (&panel->provider);
  g_clear_object (&panel->selected_prov);
  g_clear_object (&panel->default_prov);

  g_clear_pointer (&panel->prior_lock_background_uri, g_free);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->current_background);
  g_clear_object (&panel->current_lock_background);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_constructed (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);
  //shell = cc_panel_get_shell (CC_PANEL (self));

  //cc_shell_embed_widget_in_header (shell, GTK_WIDGET (self->add_picture_button), GTK_POS_RIGHT);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->constructed (object);

  /* Add check icon */
  create_selected_icon_in_overlay (GTK_OVERLAY (panel->desktop_overlay),
                                   "control-center-checked");

  create_selected_icon_in_overlay (GTK_OVERLAY (panel->lock_overlay),
                                   "control-center-checked");

//  gtk_style_context_add_class (gtk_widget_get_style_context (panel->desktop_label), "mode-label");

  /* Add press event to overlay */
  gtk_widget_add_events (panel->desktop_eventbox, GDK_BUTTON_PRESS_MASK);
  gtk_widget_add_events (panel->lock_eventbox, GDK_BUTTON_PRESS_MASK);

  g_signal_connect (panel->desktop_eventbox, "button-press-event", G_CALLBACK (on_desktop_press), panel);
  g_signal_connect (panel->lock_eventbox, "button-press-event", G_CALLBACK (on_lock_press), panel);

  gtk_combo_box_set_active (GTK_COMBO_BOX (panel->type_select_combobox), MODE_ALL);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_BACKGROUND_CHOOSER);
  //g_type_ensure (CC_TYPE_BACKGROUND_PREVIEW);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->constructed = cc_background_panel_constructed;
  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-panel.ui");

  //gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, add_picture_button);
  //gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, background_chooser);
  //gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_preview);

  //gtk_widget_class_bind_template_callback (widget_class, on_chooser_background_chosen_cb);
  //gtk_widget_class_bind_template_callback (widget_class, on_add_picture_button_clicked_cb);

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, scrolled_view);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, type_label);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, type_select_combobox);
  
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_thumbnail_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_eventbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_image);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_label);
  
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_thumbnail_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_eventbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_image);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_label);
  
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, chooser);
  
  gtk_widget_class_bind_template_callback (widget_class, on_type_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_select_background);
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  GFile *file = NULL;

  g_resources_register (cc_background_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  /* set provider */
  panel->provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                  GTK_STYLE_PROVIDER (panel->provider),
                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  panel->selected_prov = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (panel->selected_prov,
".thumbnail-box {"
"background-color: #3986e1;"
"}", -1, NULL);

  panel->default_prov = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (panel->default_prov,
".thumbnail-box {"
"background-color: rgba(201,201,201,0.0);"
"}", -1, NULL);

  file = g_file_new_for_uri ("resource:///org/gnome/control-center/background/style.css");
  gtk_css_provider_load_from_file (panel->provider, file, NULL);
  g_object_unref (file);

  panel->background_mode = MODE_ALL;
  panel->applied_background = APPLY_DESKTOP;

  panel->connection = g_application_get_dbus_connection (g_application_get_default ());
  g_resources_register (cc_background_get_resource());

  panel->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (panel->settings);

  panel->lock_settings = g_settings_new (WP_LOCK_PATH_ID);
  g_settings_delay (panel->lock_settings);

  panel->copy_cancellable = g_cancellable_new ();
  panel->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  /* Background settings */
  g_signal_connect (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel);
  g_signal_connect (panel->lock_settings, "changed", G_CALLBACK (on_settings_changed), panel);

  /* Load the backgrounds */
  reload_current_bg (panel, panel->settings);
  update_preview (panel, panel->settings, NULL);
  reload_current_bg (panel, panel->lock_settings);
  update_preview (panel, panel->lock_settings, NULL);
}
