/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-font-chooser-widget.h"
#include "cc-font-chooser.h"
#include "cc-font-define.h"
#include "cc-font-chooser-utils.h"

struct _CcFontChooserWidgetPrivate
{
  GtkWidget         *stack;
  GtkWidget         *search_entry;
  GtkWidget         *family_face_list;
  GtkTreeViewColumn *family_face_column;
  GtkCellRenderer   *family_face_cell;
  GtkWidget         *list_scrolled_window;
  GtkWidget         *list_stack;
  GtkTreeModel      *model;
  GtkTreeModel      *filter_model;

  GtkWidget       *preview;
  GtkWidget       *preview2;
  GtkWidget       *font_name_label;
  gchar           *preview_text;
  gboolean         show_preview_entry;

  GtkWidget       *size_spin;
  GtkWidget       *size_slider;
  GtkWidget       *size_slider2;
  GtkWidget       *size_label;
  GtkWidget       *no_font_label;

  GtkWidget       *axis_grid;
  GtkWidget       *feature_box;

  PangoFontMap    *font_map;

  PangoFontDescription *font_desc;
  char                 *font_features;
  PangoLanguage        *language;
  GtkTreeIter           font_iter;      /* invalid if font not available or pointer into model
                                           (not filter_model) to the row containing font */
  GtkFontFilterFunc     filter_func;
  gpointer              filter_data;
  GDestroyNotify        filter_data_destroy;

  guint                 last_fontconfig_timestamp;
  CcFontChooserLevel    level;

  GHashTable           *axes;
  gboolean              updating_variations;

  GList                *feature_items;
  GAction              *tweak_action;
};

/* This is the initial fixed height and the top padding of the preview entry */
#define PREVIEW_HEIGHT 20
#define PREVIEW_TOP_PADDING 6

/* These are the sizes of the font, style & size lists. */
#define FONT_LIST_HEIGHT  136
#define FONT_LIST_WIDTH   190
#define FONT_STYLE_LIST_WIDTH 170
#define FONT_SIZE_LIST_WIDTH  60
#define FONT_SIZE_MAX 20

enum {
  PROP_ZERO,
  PROP_TWEAK_ACTION
};

/* Keep in line with GtkTreeStore defined in gtkfontchooserwidget.ui */
enum {
  FAMILY_COLUMN,
  FACE_COLUMN,
  FONT_DESC_COLUMN,
  PREVIEW_TITLE_COLUMN
};

static void cc_font_chooser_widget_set_property         (GObject         *object,
                                                          guint            prop_id,
                                                          const GValue    *value,
                                                          GParamSpec      *pspec);
static void cc_font_chooser_widget_get_property         (GObject         *object,
                                                          guint            prop_id,
                                                          GValue          *value,
                                                          GParamSpec      *pspec);
static void cc_font_chooser_widget_finalize             (GObject         *object);

static void cc_font_chooser_widget_screen_changed       (GtkWidget       *widget,
                                                          GdkScreen       *previous_screen);

static gboolean cc_font_chooser_widget_find_font        (CcFontChooserWidget *fontchooser,
                                                          const PangoFontDescription *font_desc,
                                                          GtkTreeIter          *iter);
static void     cc_font_chooser_widget_ensure_selection (CcFontChooserWidget *fontchooser);

static gchar   *cc_font_chooser_widget_get_font         (CcFontChooserWidget *fontchooser);
static void     cc_font_chooser_widget_set_font         (CcFontChooserWidget *fontchooser,
                                                          const gchar          *fontname);

static PangoFontDescription *cc_font_chooser_widget_get_font_desc  (CcFontChooserWidget *fontchooser);
static void                  cc_font_chooser_widget_merge_font_desc(CcFontChooserWidget       *fontchooser,
                                                                     const PangoFontDescription *font_desc,
                                                                     GtkTreeIter                *iter);
static void                  cc_font_chooser_widget_take_font_desc (CcFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc);


static const gchar *cc_font_chooser_widget_get_preview_text (CcFontChooserWidget *fontchooser);
static void         cc_font_chooser_widget_set_preview_text (CcFontChooserWidget *fontchooser,
                                                              const gchar          *text);

static gboolean cc_font_chooser_widget_get_show_preview_entry (CcFontChooserWidget *fontchooser);
static void     cc_font_chooser_widget_set_show_preview_entry (CcFontChooserWidget *fontchooser,
                                                                gboolean              show_preview_entry);

static void     cc_font_chooser_widget_set_cell_size          (CcFontChooserWidget *fontchooser);
static void     cc_font_chooser_widget_load_fonts             (CcFontChooserWidget *fontchooser,
                                                                gboolean              force);

static void     cc_font_chooser_widget_populate_features      (CcFontChooserWidget *fontchooser);
static gboolean visible_func                                   (GtkTreeModel *model,
								GtkTreeIter  *iter,
								gpointer      user_data);
static void     cc_font_chooser_widget_cell_data_func         (GtkTreeViewColumn *column,
								GtkCellRenderer   *cell,
								GtkTreeModel      *tree_model,
								GtkTreeIter       *iter,
								gpointer           user_data);

static void selection_changed (GtkTreeSelection *selection,
                               CcFontChooserWidget *fontchooser);
static void update_font_features (CcFontChooserWidget *fontchooser);


static void                cc_font_chooser_widget_set_level (CcFontChooserWidget *fontchooser,
                                                              CcFontChooserLevel   level);
static CcFontChooserLevel cc_font_chooser_widget_get_level (CcFontChooserWidget *fontchooser);
static void                cc_font_chooser_widget_set_language (CcFontChooserWidget *fontchooser,
                                                                 const char           *language);
static void selection_changed (GtkTreeSelection *selection,
                               CcFontChooserWidget *fontchooser);
static void update_font_features (CcFontChooserWidget *fontchooser);

static void cc_font_chooser_widget_iface_init (CcFontChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcFontChooserWidget, cc_font_chooser_widget, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (CcFontChooserWidget)
                         G_IMPLEMENT_INTERFACE (CC_TYPE_FONT_CHOOSER,
                                                cc_font_chooser_widget_iface_init))

typedef struct _GtkDelayedFontDescription GtkDelayedFontDescription;
struct _GtkDelayedFontDescription {
  PangoFontFace        *face;
  PangoFontDescription *desc;
  guint                 ref_count;
};

static GtkDelayedFontDescription *
gtk_delayed_font_description_new (PangoFontFace *face)
{
  GtkDelayedFontDescription *result;
  
  result = g_slice_new0 (GtkDelayedFontDescription);

  result->face = g_object_ref (face);
  result->desc = NULL;
  result->ref_count = 1;

  return result;
}

static GtkDelayedFontDescription *
gtk_delayed_font_description_ref (GtkDelayedFontDescription *desc)
{
  desc->ref_count++;

  return desc;
}

static void
gtk_delayed_font_description_unref (GtkDelayedFontDescription *desc)
{
  desc->ref_count--;

  if (desc->ref_count > 0)
    return;

  g_object_unref (desc->face);
  if (desc->desc)
    pango_font_description_free (desc->desc);

  g_slice_free (GtkDelayedFontDescription, desc);
}

static const PangoFontDescription *
gtk_delayed_font_description_get (GtkDelayedFontDescription *desc)
{
  if (desc->desc == NULL)
    desc->desc = pango_font_face_describe (desc->face);

  return desc->desc;
}

#define GTK_TYPE_DELAYED_FONT_DESCRIPTION (gtk_delayed_font_description_get_type ())
G_DEFINE_BOXED_TYPE (GtkDelayedFontDescription, gtk_delayed_font_description,
                     gtk_delayed_font_description_ref,
                     gtk_delayed_font_description_unref)
static void
cc_font_chooser_widget_set_property (GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case CC_FONT_CHOOSER_PROP_FONT:
      cc_font_chooser_widget_set_font (fontchooser, g_value_get_string (value));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_DESC:
      cc_font_chooser_widget_take_font_desc (fontchooser, g_value_dup_boxed (value));
      break;
    case CC_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      cc_font_chooser_widget_set_preview_text (fontchooser, g_value_get_string (value));
      break;
    case CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      cc_font_chooser_widget_set_show_preview_entry (fontchooser, g_value_get_boolean (value));
      break;
    case CC_FONT_CHOOSER_PROP_LEVEL:
      cc_font_chooser_widget_set_level (fontchooser, g_value_get_flags (value));
      break;
    case CC_FONT_CHOOSER_PROP_LANGUAGE:
      cc_font_chooser_widget_set_language (fontchooser, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_font_chooser_widget_get_property (GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_TWEAK_ACTION:
      g_value_set_object (value, G_OBJECT (fontchooser->priv->tweak_action));
      break;
    case CC_FONT_CHOOSER_PROP_FONT:
      g_value_take_string (value, cc_font_chooser_widget_get_font (fontchooser));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, cc_font_chooser_widget_get_font_desc (fontchooser));
      break;
    case CC_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, cc_font_chooser_widget_get_preview_text (fontchooser));
      break;
    case CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, cc_font_chooser_widget_get_show_preview_entry (fontchooser));
      break;
    case CC_FONT_CHOOSER_PROP_LEVEL:
      g_value_set_flags (value, cc_font_chooser_widget_get_level (fontchooser));
      break;
    case CC_FONT_CHOOSER_PROP_FONT_FEATURES:
      g_value_set_string (value, fontchooser->priv->font_features);
      break;
    case CC_FONT_CHOOSER_PROP_LANGUAGE:
      g_value_set_string (value, pango_language_to_string (fontchooser->priv->language));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_font_chooser_widget_refilter_font_list (CcFontChooserWidget *fontchooser)
{
  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (fontchooser->priv->filter_model));
  cc_font_chooser_widget_ensure_selection (fontchooser);
}

static void
text_changed_cb (GtkEntry             *entry,
                 CcFontChooserWidget *fc)
{
  cc_font_chooser_widget_refilter_font_list (fc);
}

static void
stop_search_cb (GtkEntry             *entry,
                CcFontChooserWidget *fc)
{
  if (gtk_entry_get_text (entry)[0] != 0)
    gtk_entry_set_text (entry, "");
  else
    {
      GtkWidget *dialog;
      GtkWidget *button = NULL;

      dialog = gtk_widget_get_ancestor (GTK_WIDGET (fc), GTK_TYPE_DIALOG);
      if (dialog)
        button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

      if (button)
        gtk_widget_activate (button);
    }
}

static void
size_change_cb (GtkAdjustment *adjustment,
                gpointer       user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *font_desc;
  gdouble size = gtk_adjustment_get_value (adjustment);

  font_desc = pango_font_description_new ();
  if (pango_font_description_get_size_is_absolute (priv->font_desc))
    pango_font_description_set_absolute_size (font_desc, size * PANGO_SCALE);
  else
    pango_font_description_set_size (font_desc, size * PANGO_SCALE);

  cc_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

static gboolean
output_cb (GtkSpinButton *spin,
           gpointer       data)
{
  GtkAdjustment *adjustment;
  gchar *text;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%2.4g", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  g_free (text);

  return TRUE;
}

static void
cc_font_chooser_widget_update_marks (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkAdjustment *adj, *spin_adj;
  const int *sizes;
  gint *font_sizes;
  gint i, n_sizes;
  gdouble value, spin_value;

  if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    {
      PangoFontFace *face;

      gtk_tree_model_get (priv->model, &priv->font_iter,
                          FACE_COLUMN, &face,
                          -1);

      pango_font_face_list_sizes (face, &font_sizes, &n_sizes);

      /* It seems not many fonts actually have a sane set of sizes */
      for (i = 0; i < n_sizes; i++)
        font_sizes[i] = font_sizes[i] / PANGO_SCALE;

      g_object_unref (face);
    }
  else
    {
      font_sizes = NULL;
      n_sizes = 0;
    }

  if (n_sizes < 2)
    {
      static const gint fallback_sizes[] = {
        6, 8, 9, 10, 11, 12, 13, 14, 15, 17, FONT_SIZE_MAX
      };

      sizes = fallback_sizes;
      n_sizes = G_N_ELEMENTS (fallback_sizes);
    }
  else
    {
      sizes = font_sizes;
    }

  gtk_scale_clear_marks (GTK_SCALE (priv->size_slider));
  gtk_scale_clear_marks (GTK_SCALE (priv->size_slider2));

  adj        = gtk_range_get_adjustment (GTK_RANGE (priv->size_slider));
  spin_adj   = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin));
  spin_value = gtk_adjustment_get_value (spin_adj);

  if (spin_value < sizes[0])
    value = (gdouble) sizes[0];
  else if (spin_value > sizes[n_sizes - 1])
    value = (gdouble)sizes[n_sizes - 1];
  else
    value = (gdouble)spin_value;

  /* ensure clamping doesn't callback into font resizing code */
  g_signal_handlers_block_by_func (adj, size_change_cb, fontchooser);
  gtk_adjustment_configure (adj,
                            value,
                            sizes[0],
                            sizes[n_sizes - 1],
                            gtk_adjustment_get_step_increment (adj),
                            gtk_adjustment_get_page_increment (adj),
                            gtk_adjustment_get_page_size (adj));
  g_signal_handlers_unblock_by_func (adj, size_change_cb, fontchooser);

  for (i = 0; i < n_sizes; i++)
    {
      gtk_scale_add_mark (GTK_SCALE (priv->size_slider),
                          sizes[i],
                          GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (priv->size_slider2),
                          sizes[i],
                          GTK_POS_BOTTOM, NULL);
    }

  g_free (font_sizes);
}

static void
row_activated_cb (GtkTreeView       *view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  gchar *fontname;

  fontname = cc_font_chooser_widget_get_font (fontchooser);
  _cc_font_chooser_font_activated (CC_FONT_CHOOSER (fontchooser), fontname);
  g_free (fontname);
}

static void
cursor_changed_cb (GtkTreeView *treeview,
                   gpointer     user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkDelayedFontDescription *desc;
  GtkTreeIter filter_iter, iter;
  GtkTreePath *path = NULL;

  gtk_tree_view_get_cursor (treeview, &path, NULL);

  if (!path)
    return;

  if (!gtk_tree_model_get_iter (priv->filter_model, &filter_iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_path_free (path);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                                    &iter,
                                                    &filter_iter);
  gtk_tree_model_get (priv->model, &iter,
                      FONT_DESC_COLUMN, &desc,
                      -1);

  pango_font_description_set_variations (priv->font_desc, NULL);
  cc_font_chooser_widget_merge_font_desc (fontchooser,
                                           gtk_delayed_font_description_get (desc),
                                           &iter);

  gtk_delayed_font_description_unref (desc);
}

static gboolean
resize_by_scroll_cb (GtkWidget      *scrolled_window,
                     GdkEventScroll *event,
                     gpointer        user_data)
{
  CcFontChooserWidget *fc = user_data;
  CcFontChooserWidgetPrivate *priv = fc->priv;
  GtkAdjustment *adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (priv->size_spin));

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_RIGHT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) +
                              gtk_adjustment_get_step_increment (adj));
  else if (event->direction == GDK_SCROLL_DOWN || event->direction == GDK_SCROLL_LEFT)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) -
                              gtk_adjustment_get_step_increment (adj));
  else if (event->direction == GDK_SCROLL_SMOOTH && event->delta_x != 0.0)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) +
                              gtk_adjustment_get_step_increment (adj) * event->delta_x);
  else if (event->direction == GDK_SCROLL_SMOOTH && event->delta_y != 0.0)
    gtk_adjustment_set_value (adj,
                              gtk_adjustment_get_value (adj) -
                              gtk_adjustment_get_step_increment (adj) * event->delta_y);

  return TRUE;
}

static void
cc_font_chooser_widget_update_preview_attributes (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv;
  PangoAttrList *attrs;

  priv = fontchooser->priv;

  attrs = pango_attr_list_new ();

  /* Prevent font fallback */
  pango_attr_list_insert (attrs, pango_attr_fallback_new (FALSE));

  /* Force current font and features */
  pango_attr_list_insert (attrs, pango_attr_font_desc_new (priv->font_desc));
  if (priv->font_features)
    pango_attr_list_insert (attrs, pango_attr_font_features_new (priv->font_features));
  if (priv->language)
    pango_attr_list_insert (attrs, pango_attr_language_new (priv->language));

  gtk_entry_set_attributes (GTK_ENTRY (priv->preview), attrs);
  pango_attr_list_unref (attrs);
}

static void
row_inserted_cb (GtkTreeModel *model,
                 GtkTreePath  *path,
                 GtkTreeIter  *iter,
                 gpointer      user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_stack_set_visible_child_name (GTK_STACK (priv->list_stack), "list");
}

static void
row_deleted_cb  (GtkTreeModel *model,
                 GtkTreePath  *path,
                 gpointer      user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (gtk_tree_model_iter_n_children (model, NULL) == 0)
    gtk_stack_set_visible_child_name (GTK_STACK (priv->list_stack), "empty");
}

static void
cc_font_chooser_widget_map (GtkWidget *widget)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (widget);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "list");
  g_simple_action_set_state (G_SIMPLE_ACTION (priv->tweak_action), g_variant_new_boolean (FALSE));

  GTK_WIDGET_CLASS (cc_font_chooser_widget_parent_class)->map (widget);
}

static void
cc_font_chooser_widget_class_init (CcFontChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GParamSpec *pspec;

  g_type_ensure (GTK_TYPE_DELAYED_FONT_DESCRIPTION);
  g_type_ensure (G_TYPE_THEMED_ICON);

  widget_class->screen_changed = cc_font_chooser_widget_screen_changed;
  widget_class->map = cc_font_chooser_widget_map;

  gobject_class->finalize = cc_font_chooser_widget_finalize;
  gobject_class->set_property = cc_font_chooser_widget_set_property;
  gobject_class->get_property = cc_font_chooser_widget_get_property;

  /**
   * CcFontChooserWidget:tweak-action:
   *
   * A toggle action that can be used to switch to the tweak page
   * of the font chooser widget, which lets the user tweak the
   * OpenType features and variation axes of the selected font.
   *
   * The action will be enabled or disabled depending on whether
   * the selected font has any features or axes.
   */
  pspec = g_param_spec_object ("tweak-action",
                              ("The tweak action"),
                              ("The toggle action to switch to the tweak page"),
                              G_TYPE_ACTION,
                              G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_TWEAK_ACTION, pspec);

  _cc_font_chooser_install_properties (gobject_class);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gnome/control-center/font/font-chooser-widget.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, family_face_list);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, family_face_column);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, family_face_cell);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, list_scrolled_window);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, list_stack);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, model);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, filter_model);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, preview);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, preview2);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, size_spin);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, size_slider);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, size_slider2);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, stack);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, font_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, feature_box);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, axis_grid);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, no_font_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcFontChooserWidget, size_label);

  gtk_widget_class_bind_template_callback (widget_class, text_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, cursor_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_font_chooser_widget_set_cell_size);
  gtk_widget_class_bind_template_callback (widget_class, resize_by_scroll_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_deleted_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_inserted_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_deleted_cb);
  gtk_widget_class_bind_template_callback (widget_class, size_change_cb);
  gtk_widget_class_bind_template_callback (widget_class, output_cb);
  gtk_widget_class_bind_template_callback (widget_class, selection_changed);

  gtk_widget_class_set_css_name (widget_class, ("fontchooser"));
}

static void
change_tweak (GSimpleAction *action,
              GVariant      *state,
              gpointer       data)
{
  CcFontChooserWidget *fontchooser = data;
  gboolean tweak = g_variant_get_boolean (state);

  if (tweak)
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (fontchooser->priv->preview2));
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->priv->stack), "tweaks");
    }
  else
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (fontchooser->priv->search_entry));
      gtk_stack_set_visible_child_name (GTK_STACK (fontchooser->priv->stack), "list");
    }

  g_simple_action_set_state (action, state);
}

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)

typedef struct {
  guint32 tag;
  GtkAdjustment *adjustment;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *spin;
  GtkWidget *fontchooser;
} Axis;

static guint
axis_hash (gconstpointer v)
{
  const Axis *a = v;

  return a->tag;
}

static gboolean
axis_equal (gconstpointer v1, gconstpointer v2)
{
  const Axis *a1 = v1;
  const Axis *a2 = v2;

  return a1->tag == a2->tag;
}

static void
axis_remove (gpointer key,
             gpointer value,
             gpointer data)
{
  Axis *a = value;

  gtk_widget_destroy (a->label);
  gtk_widget_destroy (a->scale);
  gtk_widget_destroy (a->spin);
}

static void
axis_free (gpointer v)
{
  Axis *a = v;

  g_free (a);
}

#endif

static void
cc_font_chooser_widget_init (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv;
  gchar *test;

  fontchooser->priv = cc_font_chooser_widget_get_instance_private (fontchooser);
  priv = fontchooser->priv;

  gtk_widget_init_template (GTK_WIDGET (fontchooser));

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
  priv->axes = g_hash_table_new_full (axis_hash, axis_equal, NULL, axis_free);
#endif

  /* Default preview string  */
  priv->preview_text = g_strdup (pango_language_get_sample_string (NULL));
  priv->show_preview_entry = TRUE;
  priv->font_desc = pango_font_description_new ();
  priv->level = CC_FONT_CHOOSER_LEVEL_FAMILY |
                CC_FONT_CHOOSER_LEVEL_STYLE |
                CC_FONT_CHOOSER_LEVEL_SIZE;
  priv->language = pango_language_get_default ();

  /* Set default preview text */
  gtk_entry_set_text (GTK_ENTRY (priv->preview), priv->preview_text);

  /* Set translate text because don't translate text from .ui */
  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->preview), _("Preview text"));
  gtk_label_set_text (GTK_LABEL (priv->no_font_label), _("No Fonts Found"));
  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), _("Search font name"));
  gtk_label_set_text (GTK_LABEL (priv->size_label), _("Size"));

  cc_font_chooser_widget_update_preview_attributes (fontchooser);

  gtk_widget_add_events (priv->preview, GDK_SCROLL_MASK);

  /* Set the upper values of the spin/scale with G_MAXINT / PANGO_SCALE */
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->size_spin),
                             6.0, (gdouble)FONT_SIZE_MAX);
                             //1.0, (gdouble)(G_MAXINT / PANGO_SCALE));
  gtk_adjustment_set_upper (gtk_range_get_adjustment (GTK_RANGE (priv->size_slider)),
                            (gdouble)FONT_SIZE_MAX);
                            //(gdouble)(G_MAXINT / PANGO_SCALE));

  /* Setup treeview/model auxilary functions */
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                          visible_func, (gpointer)priv, NULL);

  gtk_tree_view_column_set_cell_data_func (priv->family_face_column,
                                           priv->family_face_cell,
                                           cc_font_chooser_widget_cell_data_func,
                                           fontchooser,
                                           NULL);

  priv->tweak_action = G_ACTION (g_simple_action_new_stateful ("tweak", NULL, g_variant_new_boolean (FALSE)));
  g_signal_connect (priv->tweak_action, "change-state", G_CALLBACK (change_tweak), fontchooser);

  /* Load data and set initial style-dependent parameters */
  cc_font_chooser_widget_load_fonts (fontchooser, TRUE);
#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
  cc_font_chooser_widget_populate_features (fontchooser);
#endif
  cc_font_chooser_widget_set_cell_size (fontchooser);
  cc_font_chooser_widget_take_font_desc (fontchooser, NULL);
}

/**
 * cc_font_chooser_widget_new:
 *
 * Creates a new #CcFontChooserWidget.
 *
 * Returns: a new #CcFontChooserWidget
 *
 * Since: 3.2
 */
GtkWidget *
cc_font_chooser_widget_new (void)
{
  return g_object_new (CC_TYPE_FONT_CHOOSER_WIDGET, NULL);
}

static int
cmp_families (const void *a,
              const void *b)
{
  const char *a_name = pango_font_family_get_name (*(PangoFontFamily **)a);
  const char *b_name = pango_font_family_get_name (*(PangoFontFamily **)b);

  return g_utf8_collate (a_name, b_name);
}

static void
cc_font_chooser_widget_load_fonts (CcFontChooserWidget *fontchooser,
                                    gboolean              force)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkListStore *list_store;
  gint n_families, i;
  PangoFontFamily **families;
  guint fontconfig_timestamp;
  gboolean need_reload;
  PangoFontMap *font_map;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (fontchooser)),
                "gtk-fontconfig-timestamp", &fontconfig_timestamp,
                NULL);

  /* The fontconfig timestamp is only set on systems with fontconfig; every
   * other platform will set it to 0. For those systems, we fall back to
   * reloading the fonts every time.
   */
  need_reload = fontconfig_timestamp == 0 ||
                fontconfig_timestamp != priv->last_fontconfig_timestamp;

  priv->last_fontconfig_timestamp = fontconfig_timestamp;

  if (!need_reload && !force)
    return;

  list_store = GTK_LIST_STORE (priv->model);

  if (priv->font_map)
    font_map = priv->font_map;
  else
    font_map = pango_cairo_font_map_get_default ();
  pango_font_map_list_families (font_map, &families, &n_families);

  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  g_signal_handlers_block_by_func (priv->family_face_list, cursor_changed_cb, fontchooser);
  gtk_list_store_clear (list_store);
  g_signal_handlers_unblock_by_func (priv->family_face_list, cursor_changed_cb, fontchooser);

  /* Iterate over families and faces */
  for (i = 0; i < n_families; i++)
    {
      GtkTreeIter     iter;
      PangoFontFace **faces;
      int             j, n_faces;
      const gchar    *fam_name = pango_font_family_get_name (families[i]);

      pango_font_family_list_faces (families[i], &faces, &n_faces);

      for (j = 0; j < n_faces; j++)
        {
          GtkDelayedFontDescription *desc;
          const gchar *face_name;
          char *title;

          face_name = pango_font_face_get_face_name (faces[j]);

          if ((priv->level & CC_FONT_CHOOSER_LEVEL_STYLE) != 0)
            title = g_strconcat (fam_name, " ", face_name, NULL);
          else
            title = g_strdup (fam_name);

          desc = gtk_delayed_font_description_new (faces[j]);

          gtk_list_store_insert_with_values (list_store, &iter, -1,
                                             FAMILY_COLUMN, families[i],
                                             FACE_COLUMN, faces[j],
                                             FONT_DESC_COLUMN, desc,
                                             PREVIEW_TITLE_COLUMN, title,
                                             -1);

          g_free (title);
          gtk_delayed_font_description_unref (desc);

          if ((priv->level & CC_FONT_CHOOSER_LEVEL_STYLE) == 0)
            break;
        }

      g_free (faces);
    }

  g_free (families);

  /* now make sure the font list looks right */
  if (!cc_font_chooser_widget_find_font (fontchooser, priv->font_desc, &priv->font_iter))
    memset (&priv->font_iter, 0, sizeof (GtkTreeIter));

  cc_font_chooser_widget_ensure_selection (fontchooser);
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      user_data)
{
  CcFontChooserWidgetPrivate *priv = user_data;
  gboolean result = TRUE;
  const gchar *search_text;
  gchar **split_terms;
  gchar *font_name, *font_name_casefold;
  guint i;

  if (priv->filter_func != NULL)
    {
      PangoFontFamily *family;
      PangoFontFace *face;

      gtk_tree_model_get (model, iter,
                          FAMILY_COLUMN, &family,
                          FACE_COLUMN, &face,
                          -1);

      result = priv->filter_func (family, face, priv->filter_data);

      g_object_unref (family);
      g_object_unref (face);
      
      if (!result)
        return FALSE;
    }

  /* If there's no filter string we show the item */
  search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
  if (strlen (search_text) == 0)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      PREVIEW_TITLE_COLUMN, &font_name,
                      -1);

  if (font_name == NULL)
    return FALSE;

  split_terms = g_strsplit (search_text, " ", 0);
  font_name_casefold = g_utf8_casefold (font_name, -1);

  for (i = 0; split_terms[i] && result; i++)
    {
      gchar* term_casefold = g_utf8_casefold (split_terms[i], -1);

      if (!strstr (font_name_casefold, term_casefold))
        result = FALSE;

      g_free (term_casefold);
    }

  g_free (font_name_casefold);
  g_free (font_name);
  g_strfreev (split_terms);

  return result;
}

/* in pango units */
static int
cc_font_chooser_widget_get_preview_text_height (CcFontChooserWidget *fontchooser)
{
  GtkWidget *treeview = fontchooser->priv->family_face_list;
  double dpi, font_size;

  dpi = gdk_screen_get_resolution (gtk_widget_get_screen (treeview));
  gtk_style_context_get (gtk_widget_get_style_context (treeview),
                         gtk_widget_get_state_flags (treeview),
                         "font-size", &font_size,
                         NULL);

  return (dpi < 0.0 ? 96.0 : dpi) / 72.0 * PANGO_SCALE_X_LARGE * font_size * PANGO_SCALE;
}

static PangoAttrList *
cc_font_chooser_widget_get_preview_attributes (CcFontChooserWidget       *fontchooser,
                                                const PangoFontDescription *font_desc)
{
  PangoAttribute *attribute;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  if (font_desc)
    {
      attribute = pango_attr_font_desc_new (font_desc);
      pango_attr_list_insert (attrs, attribute);
    }

  attribute = pango_attr_size_new_absolute (cc_font_chooser_widget_get_preview_text_height (fontchooser));
  pango_attr_list_insert (attrs, attribute);

  return attrs;
}

static void
cc_font_chooser_widget_cell_data_func (GtkTreeViewColumn *column,
                                        GtkCellRenderer   *cell,
                                        GtkTreeModel      *tree_model,
                                        GtkTreeIter       *iter,
                                        gpointer           user_data)
{
  CcFontChooserWidget *fontchooser = user_data;
  GtkDelayedFontDescription *desc;
  PangoAttrList *attrs;
  char *preview_title;

  gtk_tree_model_get (tree_model, iter,
                      PREVIEW_TITLE_COLUMN, &preview_title,
                      FONT_DESC_COLUMN, &desc,
                      -1);

  attrs = cc_font_chooser_widget_get_preview_attributes (fontchooser,
                                                          gtk_delayed_font_description_get (desc));

  g_object_set (cell,
                "xpad", 20,
                "ypad", 10,
                "attributes", attrs,
                "text", preview_title,
                NULL);

  gtk_delayed_font_description_unref (desc);
  pango_attr_list_unref (attrs);
  g_free (preview_title);
}

static void
cc_font_chooser_widget_set_cell_size (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoAttrList *attrs;
  GtkRequisition size;

  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, -1, -1);

  attrs = cc_font_chooser_widget_get_preview_attributes (fontchooser, NULL);

  g_object_set (priv->family_face_cell,
                "xpad", 20,
                "ypad", 10,
                "attributes", attrs,
                "text", "x",
                NULL);

  pango_attr_list_unref (attrs);

  gtk_cell_renderer_get_preferred_size (priv->family_face_cell,
                                        priv->family_face_list,
                                        &size,
                                        NULL);
  gtk_cell_renderer_set_fixed_size (priv->family_face_cell, size.width, size.height);
}

static void
cc_font_chooser_widget_finalize (GObject *object)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (object);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->font_desc)
    pango_font_description_free (priv->font_desc);

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  g_free (priv->preview_text);

  g_clear_object (&priv->font_map);

  g_object_unref (priv->tweak_action);

  g_list_free_full (priv->feature_items, g_free);

  if (priv->axes)
    g_hash_table_unref (priv->axes);

  g_free (priv->font_features);

  G_OBJECT_CLASS (cc_font_chooser_widget_parent_class)->finalize (object);
}

static gboolean
my_pango_font_family_equal (const char *familya,
                            const char *familyb)
{
  return g_ascii_strcasecmp (familya, familyb) == 0;
}

static gboolean
cc_font_chooser_widget_find_font (CcFontChooserWidget        *fontchooser,
                                   const PangoFontDescription  *font_desc,
                                   /* out arguments */
                                   GtkTreeIter                 *iter)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  gboolean valid;

  if (pango_font_description_get_family (font_desc) == NULL)
    return FALSE;

  for (valid = gtk_tree_model_get_iter_first (priv->model, iter);
       valid;
       valid = gtk_tree_model_iter_next (priv->model, iter))
    {
      GtkDelayedFontDescription *desc;
      PangoFontDescription *merged;
      PangoFontFamily *family;

      gtk_tree_model_get (priv->model, iter,
                          FAMILY_COLUMN, &family,
                          FONT_DESC_COLUMN, &desc,
                          -1);

      if (!my_pango_font_family_equal (pango_font_description_get_family (font_desc),
                                       pango_font_family_get_name (family)))
        {
          gtk_delayed_font_description_unref (desc);
          g_object_unref (family);
          continue;
        }

      merged = pango_font_description_copy_static (gtk_delayed_font_description_get (desc));

      pango_font_description_merge_static (merged, font_desc, FALSE);
      if (pango_font_description_equal (merged, font_desc))
        {
          gtk_delayed_font_description_unref (desc);
          pango_font_description_free (merged);
          g_object_unref (family);
          break;
        }

      gtk_delayed_font_description_unref (desc);
      pango_font_description_free (merged);
      g_object_unref (family);
    }
  
  return valid;
}

static void
fontconfig_changed (CcFontChooserWidget *fontchooser)
{
  cc_font_chooser_widget_load_fonts (fontchooser, TRUE);
}

static void
cc_font_chooser_widget_screen_changed (GtkWidget *widget,
                                        GdkScreen *previous_screen)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (widget);
  GtkSettings *settings;

  if (GTK_WIDGET_CLASS (cc_font_chooser_widget_parent_class)->screen_changed)
    GTK_WIDGET_CLASS (cc_font_chooser_widget_parent_class)->screen_changed (widget, previous_screen);

  if (previous_screen)
    {
      settings = gtk_settings_get_for_screen (previous_screen);
      g_signal_handlers_disconnect_by_func (settings, fontconfig_changed, widget);
    }
  settings = gtk_widget_get_settings (widget);
  g_signal_connect_object (settings, "notify::gtk-fontconfig-timestamp",
                           G_CALLBACK (fontconfig_changed), widget, G_CONNECT_SWAPPED);

  if (previous_screen == NULL)
    previous_screen = gdk_screen_get_default ();

  if (previous_screen == gtk_widget_get_screen (widget))
    return;

  cc_font_chooser_widget_load_fonts (fontchooser, FALSE);
}

static PangoFontFamily *
cc_font_chooser_widget_get_family (CcFontChooser *chooser)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontFamily *family;

  if (!gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    return NULL;

  gtk_tree_model_get (priv->model, &priv->font_iter,
                      FAMILY_COLUMN, &family,
                      -1);
  g_object_unref (family);

  return family;
}

static PangoFontFace *
cc_font_chooser_widget_get_face (CcFontChooser *chooser)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontFace *face;

  if (!gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter))
    return NULL;

  gtk_tree_model_get (priv->model, &priv->font_iter,
                      FACE_COLUMN, &face,
                      -1);
  g_object_unref (face);

  return face;
}

static gint
cc_font_chooser_widget_get_size (CcFontChooser *chooser)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  PangoFontDescription *desc = cc_font_chooser_widget_get_font_desc (fontchooser);

  if (desc)
    return pango_font_description_get_size (desc);

  return -1;
}

static gchar *
cc_font_chooser_widget_get_font (CcFontChooserWidget *fontchooser)
{
  PangoFontDescription *desc = cc_font_chooser_widget_get_font_desc (fontchooser);

  if (desc)
    return pango_font_description_to_string (desc);

  return NULL;
}

static PangoFontDescription *
cc_font_chooser_widget_get_font_desc (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->family_face_list));
  if (gtk_tree_selection_count_selected_rows (selection) > 0)
    return fontchooser->priv->font_desc;

  return NULL;
}

static void
cc_font_chooser_widget_set_font (CcFontChooserWidget *fontchooser,
                                  const gchar          *fontname)
{
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string (fontname);
  cc_font_chooser_widget_take_font_desc (fontchooser, font_desc);
}

static void
cc_font_chooser_widget_update_font_name (CcFontChooserWidget *fontchooser,
                                          GtkTreeSelection     *selection)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  PangoFontFamily *family;
  PangoFontFace *face;
  GtkDelayedFontDescription *desc;
  const PangoFontDescription *font_desc;
  PangoAttrList *attrs;
  const char *fam_name;
  const char *face_name;
  char *title;

  gtk_tree_selection_get_selected (selection, &model, &iter);
  gtk_tree_model_get (model, &iter,
                      FAMILY_COLUMN, &family,
                      FACE_COLUMN, &face,
                      FONT_DESC_COLUMN, &desc,
                      -1);

  fam_name = pango_font_family_get_name (family);
  face_name = pango_font_face_get_face_name (face);
  font_desc = gtk_delayed_font_description_get (desc);

  g_object_unref (family);
  g_object_unref (face);
  gtk_delayed_font_description_unref (desc);

  if (priv->level == CC_FONT_CHOOSER_LEVEL_FAMILY)
    title = g_strdup (fam_name);
  else
    title = g_strconcat (fam_name, " ", face_name, NULL);

  attrs = cc_font_chooser_widget_get_preview_attributes (fontchooser, font_desc);
  gtk_label_set_attributes (GTK_LABEL (priv->font_name_label), attrs);
  pango_attr_list_unref (attrs);

  gtk_label_set_label (GTK_LABEL (priv->font_name_label), title);
  g_free (title);
}

static void
selection_changed (GtkTreeSelection     *selection,
                   CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");

  if (gtk_tree_selection_count_selected_rows (selection) > 0)
    {
      cc_font_chooser_widget_update_font_name (fontchooser, selection);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->tweak_action), TRUE);
    }
  else
    {
      g_simple_action_set_state (G_SIMPLE_ACTION (priv->tweak_action), g_variant_new_boolean (FALSE));
      g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->tweak_action), FALSE);
    }
}

static void
cc_font_chooser_widget_ensure_selection (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkTreeSelection *selection;
  GtkTreeIter filter_iter;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->family_face_list));

  if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (priv->model), &priv->font_iter) &&
      gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (priv->filter_model),
                                                        &filter_iter,
                                                        &priv->font_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->filter_model),
                                                   &filter_iter);

      gtk_tree_selection_select_iter (selection, &filter_iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->family_face_list),
                                    path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
    }
}

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)

/* OpenType variations */

#define FixedToFloat(f) (((float)(f))/65536.0)

static void
add_font_variations (CcFontChooserWidget *fontchooser,
                     GString              *s)
{
  GHashTableIter iter;
  Axis *axis;
  const char *sep = "";
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_hash_table_iter_init (&iter, fontchooser->priv->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)NULL, (gpointer *)&axis))
    {
      char tag[5];
      double value;

      tag[0] = (axis->tag >> 24) & 0xff;
      tag[1] = (axis->tag >> 16) & 0xff;
      tag[2] = (axis->tag >> 8) & 0xff;
      tag[3] = (axis->tag >> 0) & 0xff;
      tag[4] = '\0';
      value = gtk_adjustment_get_value (axis->adjustment);
      g_string_append_printf (s, "%s%s=%s", sep, tag, g_ascii_dtostr (buf, sizeof(buf), value));
      sep = ",";
    }
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    Axis          *axis)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (axis->fontchooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontDescription *font_desc;
  GString *s;

  priv->updating_variations = TRUE;

  s = g_string_new ("");
  add_font_variations (fontchooser, s);

  if (s->len > 0)
    {
      font_desc = pango_font_description_new ();
      pango_font_description_set_variations (font_desc, s->str);
      cc_font_chooser_widget_take_font_desc (fontchooser, font_desc);
    }

  g_string_free (s, TRUE);

  priv->updating_variations = FALSE;
}

static gboolean
should_show_axis (FT_Var_Axis *ax)
{
  /* FIXME use FT_Get_Var_Axis_Flags */
  if (ax->tag == FT_MAKE_TAG ('o', 'p', 's', 'z'))
    return FALSE;

  return TRUE;
}

static gboolean
is_named_instance (FT_Face face)
{
  return (face->face_index >> 16) > 0;
}

static struct {
  guint32 tag;
  const char *name;
} axis_names[] = {
  { FT_MAKE_TAG ('w', 'd', 't', 'h'), N_("Width") },
  { FT_MAKE_TAG ('w', 'g', 'h', 't'), N_("Weight") },
  { FT_MAKE_TAG ('i', 't', 'a', 'l'), N_("Italic") },
  { FT_MAKE_TAG ('s', 'l', 'n', 't'), N_("Slant") },
  { FT_MAKE_TAG ('o', 'p', 's', 'z'), N_("Optical Size") },
};

static gboolean
add_axis (CcFontChooserWidget *fontchooser,
          FT_Face               face,
          FT_Var_Axis          *ax,
          FT_Fixed              value,
          int                   row)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  Axis *axis;
  const char *name;
  int i;

  axis = g_new (Axis, 1);
  axis->tag = ax->tag;
  axis->fontchooser = GTK_WIDGET (fontchooser);

  name = ax->name;
  for (i = 0; i < G_N_ELEMENTS (axis_names); i++)
    {
      if (axis_names[i].tag == ax->tag)
        {
          name = _(axis_names[i].name);
          break;
        }
    }
  axis->label = gtk_label_new (name);
  gtk_widget_show (axis->label);
  gtk_widget_set_halign (axis->label, GTK_ALIGN_START);
  gtk_widget_set_valign (axis->label, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->label, 0, row, 1, 1);
  axis->adjustment = gtk_adjustment_new ((double)FixedToFloat(value),
                                         (double)FixedToFloat(ax->minimum),
                                         (double)FixedToFloat(ax->maximum),
                                         1.0, 10.0, 0.0);
  axis->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, axis->adjustment);
  gtk_widget_show (axis->scale);
  gtk_scale_add_mark (GTK_SCALE (axis->scale), (double)FixedToFloat(ax->def), GTK_POS_TOP, NULL);
  gtk_widget_set_valign (axis->scale, GTK_ALIGN_BASELINE);
  gtk_widget_set_hexpand (axis->scale, TRUE);
  gtk_widget_set_size_request (axis->scale, 100, -1);
  gtk_scale_set_draw_value (GTK_SCALE (axis->scale), FALSE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->scale, 1, row, 1, 1);
  axis->spin = gtk_spin_button_new (axis->adjustment, 0, 0);
  gtk_widget_show (axis->spin);
  g_signal_connect (axis->spin, "output", G_CALLBACK (output_cb), fontchooser);
  gtk_widget_set_valign (axis->spin, GTK_ALIGN_BASELINE);
  gtk_grid_attach (GTK_GRID (priv->axis_grid), axis->spin, 2, row, 1, 1);

  g_hash_table_add (priv->axes, axis);

  adjustment_changed (axis->adjustment, axis);
  g_signal_connect (axis->adjustment, "value-changed", G_CALLBACK (adjustment_changed), axis);
  if (is_named_instance (face) || !should_show_axis (ax))
    {
      gtk_widget_hide (axis->label);
      gtk_widget_hide (axis->scale);
      gtk_widget_hide (axis->spin);

      return FALSE;
    }

  return TRUE;
}

static gboolean
cc_font_chooser_widget_update_font_variations (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  FT_MM_Var *ft_mm_var;
  FT_Error ret;
  gboolean has_axis = FALSE;

  if (priv->updating_variations)
    return FALSE;

  g_hash_table_foreach (priv->axes, axis_remove, NULL);
  g_hash_table_remove_all (priv->axes);

  if ((priv->level & CC_FONT_CHOOSER_LEVEL_VARIATIONS) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font));

  ret = FT_Get_MM_Var (ft_face, &ft_mm_var);
  if (ret == 0)
    {
      int i;
      FT_Fixed *coords;

      coords = g_new (FT_Fixed, ft_mm_var->num_axis);
      for (i = 0; i < ft_mm_var->num_axis; i++)
        coords[i] = ft_mm_var->axis[i].def;

      if (ft_face->face_index > 0)
        {
          int instance_id = ft_face->face_index >> 16;
          if (instance_id && instance_id <= ft_mm_var->num_namedstyles)
            {
              FT_Var_Named_Style *instance = &ft_mm_var->namedstyle[instance_id - 1];
              memcpy (coords, instance->coords, ft_mm_var->num_axis * sizeof (*coords));
            }
        }

      for (i = 0; i < ft_mm_var->num_axis; i++)
        {
          if (add_axis (fontchooser, ft_face, &ft_mm_var->axis[i], coords[i], i + 4))
            has_axis = TRUE;
        }

      g_free (coords);
      free (ft_mm_var);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);

  return has_axis;
}

/* OpenType features */

/* look for a lang / script combination that matches the
 * language property and is supported by the hb_face. If
 * none is found, return the default lang / script tags.
 */
static void
find_language_and_script (CcFontChooserWidget *fontchooser,
                          hb_face_t            *hb_face,
                          hb_tag_t             *lang_tag,
                          hb_tag_t             *script_tag)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  gint i, j, k;
  hb_tag_t scripts[80];
  unsigned int n_scripts;
  unsigned int count;
  hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
  hb_language_t lang;
  const char *langname, *p;

  langname = pango_language_to_string (priv->language);
  p = strchr (langname, '-');
  lang = hb_language_from_string (langname, p ? p - langname : -1);

  n_scripts = 0;
  for (i = 0; i < 2; i++)
    {
      count = G_N_ELEMENTS (scripts);
      hb_ot_layout_table_get_script_tags (hb_face, table[i], n_scripts, &count, scripts);
      n_scripts += count;
    }

  for (j = 0; j < n_scripts; j++)
    {
      hb_tag_t languages[80];
      unsigned int n_languages;

      n_languages = 0;
      for (i = 0; i < 2; i++)
        {
          count = G_N_ELEMENTS (languages);
          hb_ot_layout_script_get_language_tags (hb_face, table[i], j, n_languages, &count, languages);
          n_languages += count;
        }

      for (k = 0; k < n_languages; k++)
        {
          if (lang == hb_ot_tag_to_language (languages[k]))
            {
              *script_tag = scripts[j];
              *lang_tag = languages[k];
              return;
            }
        }
    }

  *lang_tag = HB_OT_TAG_DEFAULT_LANGUAGE;
  *script_tag = HB_OT_TAG_DEFAULT_SCRIPT;
}

typedef struct {
  hb_tag_t tag;
  const char *name;
  GtkWidget *top;
  GtkWidget *feat;
  GtkWidget *example;
} FeatureItem;

static const char *
get_feature_display_name (hb_tag_t tag)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (open_type_layout_features); i++)
    {
      if (tag == open_type_layout_features[i].tag)
        return g_dpgettext2 (NULL, "OpenType layout", open_type_layout_features[i].name);
    }

  return NULL;
}

static void
set_inconsistent (GtkCheckButton *button,
                  gboolean        inconsistent)
{
  if (inconsistent)
    gtk_widget_set_state_flags (GTK_WIDGET (button), GTK_STATE_FLAG_INCONSISTENT, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (button), GTK_STATE_FLAG_INCONSISTENT);
//  gtk_widget_set_opacity (gtk_widget_get_first_child (GTK_WIDGET (button)), inconsistent ? 0.0 : 1.0);
}

static void
feat_clicked (GtkWidget *feat,
              gpointer   data)
{
  g_signal_handlers_block_by_func (feat, feat_clicked, NULL);

  if (gtk_widget_get_state_flags (feat) & GTK_STATE_FLAG_INCONSISTENT)
    {
      set_inconsistent (GTK_CHECK_BUTTON (feat), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (feat), TRUE);
    }

  g_signal_handlers_unblock_by_func (feat, feat_clicked, NULL);
}

static void
feat_pressed (GtkGesture *gesture,
              int         n_press,
              double      x,
              double      y,
              GtkWidget  *feat)
{
  gboolean inconsistent;

  inconsistent = (gtk_widget_get_state_flags (feat) & GTK_STATE_FLAG_INCONSISTENT) != 0;
  set_inconsistent (GTK_CHECK_BUTTON (feat), !inconsistent);
}

static char *
find_affected_text (hb_tag_t   feature_tag,
                    hb_face_t *hb_face,
                    hb_tag_t   script_tag,
                    hb_tag_t   lang_tag,
                    int        max_chars)
{
  unsigned int script_index = 0;
  unsigned int lang_index = 0;
  unsigned int feature_index = 0;
  GString *chars;

  chars = g_string_new ("");

  hb_ot_layout_table_find_script (hb_face, HB_OT_TAG_GSUB, script_tag, &script_index);
  hb_ot_layout_script_find_language (hb_face, HB_OT_TAG_GSUB, script_index, lang_tag, &lang_index);
  if (hb_ot_layout_language_find_feature (hb_face, HB_OT_TAG_GSUB, script_index, lang_index, feature_tag, &feature_index))
    {
      unsigned int lookup_indexes[32];
      unsigned int lookup_count = 32;
      int count;
      int n_chars = 0;

      count  = hb_ot_layout_feature_get_lookups (hb_face,
                                                 HB_OT_TAG_GSUB,
                                                 feature_index,
                                                 0,
                                                 &lookup_count,
                                                 lookup_indexes);
      if (count > 0)
        {
          hb_set_t* glyphs_before = NULL;
          hb_set_t* glyphs_input  = NULL;
          hb_set_t* glyphs_after  = NULL;
          hb_set_t* glyphs_output = NULL;
          hb_font_t *hb_font = NULL;
          hb_codepoint_t gid;

          glyphs_input  = hb_set_create ();

          // XXX For now, just look at first index
          hb_ot_layout_lookup_collect_glyphs (hb_face,
                                              HB_OT_TAG_GSUB,
                                              lookup_indexes[0],
                                              glyphs_before,
                                              glyphs_input,
                                              glyphs_after,
                                              glyphs_output);

          hb_font = hb_font_create (hb_face);
          hb_ft_font_set_funcs (hb_font);

          gid = -1;
          while (hb_set_next (glyphs_input, &gid)) {
            hb_codepoint_t ch;
            if (n_chars == max_chars)
              {
                g_string_append (chars, "…");
                break;
              }
            for (ch = 0; ch < 0xffff; ch++) {
              hb_codepoint_t glyph = 0;
              hb_font_get_nominal_glyph (hb_font, ch, &glyph);
              if (glyph == gid) {
                g_string_append_unichar (chars, (gunichar)ch);
                n_chars++;
                break;
              }
            }
          }
          hb_set_destroy (glyphs_input);
          hb_font_destroy (hb_font);
        }
    }

  return g_string_free (chars, FALSE);
}

static void
update_feature_example (FeatureItem          *item,
                        hb_face_t            *hb_face,
                        hb_tag_t              script_tag,
                        hb_tag_t              lang_tag,
                        PangoFontDescription *font_desc)
{
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxxx", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum", NULL };
  const char *number_formatting[] = { "zero", "nalt", NULL };
  const char *char_variants[] = {
    "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
    NULL };

  if (g_strv_contains (number_case, item->name) ||
      g_strv_contains (number_spacing, item->name))
    {
      PangoAttrList *attrs;
      PangoAttribute *attr;
      PangoFontDescription *desc;
      char *str;

      attrs = pango_attr_list_new ();

      desc = pango_font_description_copy (font_desc);
      pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
      pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
      pango_font_description_free (desc);
      str = g_strconcat (item->name, " 1", NULL);
      attr = pango_attr_font_features_new (str);
      pango_attr_list_insert (attrs, attr);

      gtk_label_set_text (GTK_LABEL (item->example), "0123456789");
      gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

      pango_attr_list_unref (attrs);
    }
  else if (g_strv_contains (letter_case, item->name) ||
           g_strv_contains (number_formatting, item->name) ||
           g_strv_contains (char_variants, item->name))
    {
      char *input = NULL;
      char *text;

      if (strcmp (item->name, "case") == 0)
        input = g_strdup ("A-B[Cq]");
      else if (g_strv_contains (letter_case, item->name))
        input = g_strdup ("AaBbCc…");
      else if (strcmp (item->name, "zero") == 0)
        input = g_strdup ("0");
      else if (strcmp (item->name, "nalt") == 0)
        input = find_affected_text (item->tag, hb_face, script_tag, lang_tag, 3);
      else
        input = find_affected_text (item->tag, hb_face, script_tag, lang_tag, 10);

      if (input[0] != '\0')
        {
          PangoAttrList *attrs;
          PangoAttribute *attr;
          PangoFontDescription *desc;
          char *str;

          text = g_strconcat (input, " ⟶ ", input, NULL);

          attrs = pango_attr_list_new ();

          desc = pango_font_description_copy (font_desc);
          pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
          pango_attr_list_insert (attrs, pango_attr_font_desc_new (desc));
          pango_font_description_free (desc);
          str = g_strconcat (item->name, " 0", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = 0;
          attr->end_index = strlen (input);
          pango_attr_list_insert (attrs, attr);
          str = g_strconcat (item->name, " 1", NULL);
          attr = pango_attr_font_features_new (str);
          attr->start_index = strlen (input) + strlen (" ⟶ ");
          attr->end_index = attr->start_index + strlen (input);
          pango_attr_list_insert (attrs, attr);

          gtk_label_set_text (GTK_LABEL (item->example), text);
          gtk_label_set_attributes (GTK_LABEL (item->example), attrs);

          g_free (text);
          pango_attr_list_unref (attrs);
        }
      else
        gtk_label_set_markup (GTK_LABEL (item->example), "");
      g_free (input);
    }
}

static void
add_check_group (CcFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  PangoAttrList *attrs;
  int i;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (group);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_widget_show (label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      GtkGesture *gesture;
      GtkWidget *box;
      GtkWidget *example;

      tag = hb_tag_from_string (tags[i], -1);

      feat = gtk_check_button_new_with_label (get_feature_display_name (tag));
      gtk_widget_show (feat);
      set_inconsistent (GTK_CHECK_BUTTON (feat), TRUE);
      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_font_features), fontchooser);
      g_signal_connect_swapped (feat, "notify::inconsistent", G_CALLBACK (update_font_features), fontchooser);
      g_signal_connect (feat, "clicked", G_CALLBACK (feat_clicked), NULL);

      gesture = gtk_gesture_multi_press_new (feat);
      g_object_set_data_full (G_OBJECT (feat), "press", gesture, g_object_unref);

      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_signal_connect (gesture, "pressed", G_CALLBACK (feat_pressed), feat);

      example = gtk_label_new ("");
      gtk_widget_show (example);
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_show (box);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_container_add (GTK_CONTAINER (box), feat);
      gtk_container_add (GTK_CONTAINER (box), example);
      gtk_container_add (GTK_CONTAINER (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

static void
add_radio_group (CcFontChooserWidget *fontchooser,
                 const char  *title,
                 const char **tags)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GtkWidget *label;
  GtkWidget *group;
  int i;
  GtkWidget *group_button = NULL;
  PangoAttrList *attrs;

  group = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (group);
  gtk_widget_set_halign (group, GTK_ALIGN_FILL);

  label = gtk_label_new (title);
  gtk_widget_show (label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  g_object_set (label, "margin-top", 10, "margin-bottom", 10, NULL);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
  gtk_container_add (GTK_CONTAINER (group), label);

  for (i = 0; tags[i]; i++)
    {
      hb_tag_t tag;
      GtkWidget *feat;
      FeatureItem *item;
      const char *name;
      GtkWidget *box;
      GtkWidget *example;

      tag = hb_tag_from_string (tags[i], -1);
      name = get_feature_display_name (tag);

      feat = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (group_button),
                                                          name ? name : _("Default"));
      gtk_widget_show (feat);
      if (group_button == NULL)
        group_button = feat;

      g_signal_connect_swapped (feat, "notify::active", G_CALLBACK (update_font_features), fontchooser);
      g_object_set_data (G_OBJECT (feat), "default", group_button);

      example = gtk_label_new ("");
      gtk_widget_show (example);
      gtk_label_set_selectable (GTK_LABEL (example), TRUE);
      gtk_widget_set_halign (example, GTK_ALIGN_START);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_show (box);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_container_add (GTK_CONTAINER (box), feat);
      gtk_container_add (GTK_CONTAINER (box), example);
      gtk_container_add (GTK_CONTAINER (group), box);

      item = g_new (FeatureItem, 1);
      item->name = tags[i];
      item->tag = tag;
      item->top = box;
      item->feat = feat;
      item->example = example;

      priv->feature_items = g_list_prepend (priv->feature_items, item);
    }

  gtk_container_add (GTK_CONTAINER (priv->feature_box), group);
}

static void
cc_font_chooser_widget_populate_features (CcFontChooserWidget *fontchooser)
{
  const char *ligatures[] = { "liga", "dlig", "hlig", "clig", NULL };
  const char *letter_case[] = { "smcp", "c2sc", "pcap", "c2pc", "unic", "cpsp", "case", NULL };
  const char *number_case[] = { "xxxx", "lnum", "onum", NULL };
  const char *number_spacing[] = { "xxxx", "pnum", "tnum", NULL };
  const char *number_formatting[] = { "zero", "nalt", NULL };
  const char *char_variants[] = {
    "swsh", "cswh", "calt", "falt", "hist", "salt", "jalt", "titl", "rand",
    "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08", "ss09", "ss10",
    "ss11", "ss12", "ss13", "ss14", "ss15", "ss16", "ss17", "ss18", "ss19", "ss20",
    NULL };

  add_check_group (fontchooser, _("Ligatures"), ligatures);
  add_check_group (fontchooser, _("Letter Case"), letter_case);
  add_radio_group (fontchooser, _("Number Case"), number_case);
  add_radio_group (fontchooser, _("Number Spacing"), number_spacing);
  add_check_group (fontchooser, _("Number Formatting"), number_formatting);
  add_check_group (fontchooser, _("Character Variants"), char_variants);

  update_font_features (fontchooser);
}

static gboolean
cc_font_chooser_widget_update_font_features (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFont *pango_font;
  FT_Face ft_face;
  hb_font_t *hb_font;
  hb_tag_t script_tag;
  hb_tag_t lang_tag;
  guint script_index = 0;
  guint lang_index = 0;
  int i, j;
  GList *l;
  gboolean has_feature = FALSE;

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;
      gtk_widget_hide (item->top);
      gtk_widget_hide (gtk_widget_get_parent (item->top));
    }

  if ((priv->level & CC_FONT_CHOOSER_LEVEL_FEATURES) == 0)
    return FALSE;

  pango_font = pango_context_load_font (gtk_widget_get_pango_context (GTK_WIDGET (fontchooser)),
                                        priv->font_desc);
  ft_face = pango_fc_font_lock_face (PANGO_FC_FONT (pango_font)),
  hb_font = hb_ft_font_create (ft_face, NULL);

  if (hb_font)
    {
      hb_tag_t table[2] = { HB_OT_TAG_GSUB, HB_OT_TAG_GPOS };
      hb_face_t *hb_face;
      hb_tag_t features[80];
      unsigned int count;
      unsigned int n_features;

      hb_face = hb_font_get_face (hb_font);

      find_language_and_script (fontchooser, hb_face, &lang_tag, &script_tag);

      n_features = 0;
      for (i = 0; i < 2; i++)
        {
          hb_ot_layout_table_find_script (hb_face, table[i], script_tag, &script_index);
          hb_ot_layout_script_find_language (hb_face, table[i], script_index, lang_tag, &lang_index);
          count = G_N_ELEMENTS (features);
          hb_ot_layout_language_get_feature_tags (hb_face,
                                                  table[i],
                                                  script_index,
                                                  lang_index,
                                                  n_features,
                                                  &count,
                                                  features);
          n_features += count;
        }

      for (j = 0; j < n_features; j++)
        {
          for (l = priv->feature_items; l; l = l->next)
            {
              FeatureItem *item = l->data;
              if (item->tag != features[j])
                continue;

              has_feature = TRUE;
              gtk_widget_show (item->top);
              gtk_widget_show (gtk_widget_get_parent (item->top));

              update_feature_example (item, hb_face, script_tag, lang_tag, priv->font_desc);

              if (GTK_IS_RADIO_BUTTON (item->feat))
                {
                  GtkWidget *def = GTK_WIDGET (g_object_get_data (G_OBJECT (item->feat), "default"));
                  gtk_widget_show (gtk_widget_get_parent (def));
                }
              else if (GTK_IS_CHECK_BUTTON (item->feat))
                {
                  set_inconsistent (GTK_CHECK_BUTTON (item->feat), TRUE);
                }
            }
        }

      hb_face_destroy (hb_face);
    }

  pango_fc_font_unlock_face (PANGO_FC_FONT (pango_font));
  g_object_unref (pango_font);

  return has_feature;
}

static void
update_font_features (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GString *s;
  GList *l;

  s = g_string_new ("");

  for (l = priv->feature_items; l; l = l->next)
    {
      FeatureItem *item = l->data;

      if (!gtk_widget_is_sensitive (item->feat))
        continue;

      if (GTK_IS_RADIO_BUTTON (item->feat))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)) &&
              strcmp (item->name, "xxxx") != 0)
            {
              g_string_append_printf (s, "%s\"%s\" %d", s->len > 0 ? ", " : "", item->name, 1);
            }
        }
      else if (GTK_IS_CHECK_BUTTON (item->feat))
        {
          if (gtk_widget_get_state_flags (item->feat) & GTK_STATE_FLAG_INCONSISTENT)
            continue;

          g_string_append_printf (s, "%s\"%s\" %d",
                                  s->len > 0 ? ", " : "", item->name,
                                  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->feat)));
        }
    }

  if (g_strcmp0 (priv->font_features, s->str) != 0)
    {
      g_free (priv->font_features);
      priv->font_features = g_string_free (s, FALSE);
      g_object_notify (G_OBJECT (fontchooser), "font-features");
    }
  else
    g_string_free (s, TRUE);

  cc_font_chooser_widget_update_preview_attributes (fontchooser);
}

#endif

static void
cc_font_chooser_widget_merge_font_desc (CcFontChooserWidget       *fontchooser,
                                         const PangoFontDescription *font_desc,
                                         GtkTreeIter                *iter)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontMask mask;

  g_assert (font_desc != NULL);
  /* iter may be NULL if the font doesn't exist on the list */

  mask = pango_font_description_get_set_fields (font_desc);

  /* sucky test, because we can't restrict the comparison to 
   * only the parts that actually do get merged */
  if (pango_font_description_equal (font_desc, priv->font_desc))
    return;

  pango_font_description_merge (priv->font_desc, font_desc, TRUE);
  
  if (mask & PANGO_FONT_MASK_SIZE)
    {
      double font_size = (double) pango_font_description_get_size (priv->font_desc) / PANGO_SCALE;
      /* XXX: This clamps, which can cause it to reloop into here, do we need
       * to block its signal handler? */
      gtk_range_set_value (GTK_RANGE (priv->size_slider), font_size);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->size_spin), font_size);
    }
  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      gboolean has_tweak = FALSE;

      if (&priv->font_iter != iter)
        {
          if (iter == NULL)
            memset (&priv->font_iter, 0, sizeof (GtkTreeIter));
          else
            memcpy (&priv->font_iter, iter, sizeof (GtkTreeIter));
          
          cc_font_chooser_widget_ensure_selection (fontchooser);
        }

      cc_font_chooser_widget_update_marks (fontchooser);

#if defined(HAVE_HARFBUZZ) && defined(HAVE_PANGOFT)
      if (cc_font_chooser_widget_update_font_features (fontchooser))
        has_tweak = TRUE;
      if (cc_font_chooser_widget_update_font_variations (fontchooser))
        has_tweak = TRUE;
#endif

      g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->tweak_action), has_tweak);
    }

  cc_font_chooser_widget_update_preview_attributes (fontchooser);

  g_object_notify (G_OBJECT (fontchooser), "font");
  g_object_notify (G_OBJECT (fontchooser), "font-desc");
}

static void
cc_font_chooser_widget_take_font_desc (CcFontChooserWidget *fontchooser,
                                        PangoFontDescription *font_desc)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoFontMask mask;

  if (font_desc == NULL)
    font_desc = pango_font_description_from_string (CC_FONT_CHOOSER_DEFAULT_FONT_NAME);

  mask = pango_font_description_get_set_fields (font_desc);
  if (mask & (PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_STYLE | PANGO_FONT_MASK_VARIANT |
              PANGO_FONT_MASK_WEIGHT | PANGO_FONT_MASK_STRETCH))
    {
      GtkTreeIter iter;

      if (cc_font_chooser_widget_find_font (fontchooser, font_desc, &iter))
        cc_font_chooser_widget_merge_font_desc (fontchooser, font_desc, &iter);
      else
        cc_font_chooser_widget_merge_font_desc (fontchooser, font_desc, NULL);
    }
  else
    {
      cc_font_chooser_widget_merge_font_desc (fontchooser, font_desc, &priv->font_iter);
    }

  pango_font_description_free (font_desc);
}

static const gchar*
cc_font_chooser_widget_get_preview_text (CcFontChooserWidget *fontchooser)
{
  return fontchooser->priv->preview_text;
}

static void
cc_font_chooser_widget_set_preview_text (CcFontChooserWidget *fontchooser,
                                          const gchar          *text)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  g_free (priv->preview_text);
  priv->preview_text = g_strdup (text);

  gtk_entry_set_text (GTK_ENTRY (priv->preview), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");

  /* XXX: There's no API to tell the treeview that a column has changed,
   * so we just */
  gtk_widget_queue_draw (priv->family_face_list);
}

static gboolean
cc_font_chooser_widget_get_show_preview_entry (CcFontChooserWidget *fontchooser)
{
  return fontchooser->priv->show_preview_entry;
}

static void
cc_font_chooser_widget_set_show_preview_entry (CcFontChooserWidget *fontchooser,
                                                gboolean              show_preview_entry)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->show_preview_entry != show_preview_entry)
    {
      fontchooser->priv->show_preview_entry = show_preview_entry;

      if (show_preview_entry)
        gtk_widget_show (fontchooser->priv->preview);
      else
        gtk_widget_hide (fontchooser->priv->preview);

      g_object_notify (G_OBJECT (fontchooser), "show-preview-entry");
    }
}

static void
cc_font_chooser_widget_set_font_map (CcFontChooser *chooser,
                                      PangoFontMap   *fontmap)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (g_set_object (&priv->font_map, fontmap))
    {
      PangoContext *context;

      if (!fontmap)
        fontmap = pango_cairo_font_map_get_default ();

      context = gtk_widget_get_pango_context (priv->family_face_list);
      pango_context_set_font_map (context, fontmap);

      context = gtk_widget_get_pango_context (priv->preview);
      pango_context_set_font_map (context, fontmap);

      cc_font_chooser_widget_load_fonts (fontchooser, TRUE);
    }
}

static PangoFontMap *
cc_font_chooser_widget_get_font_map (CcFontChooser *chooser)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  return priv->font_map;
}

static void
cc_font_chooser_widget_set_filter_func (CcFontChooser  *chooser,
                                         GtkFontFilterFunc filter,
                                         gpointer          data,
                                         GDestroyNotify    destroy)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (chooser);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->filter_data_destroy)
    priv->filter_data_destroy (priv->filter_data);

  priv->filter_func = filter;
  priv->filter_data = data;
  priv->filter_data_destroy = destroy;

  cc_font_chooser_widget_refilter_font_list (fontchooser);
}

static void
cc_font_chooser_widget_set_level (CcFontChooserWidget *fontchooser,
                                   CcFontChooserLevel   level)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  if (priv->level == level)
    return;

  priv->level = level;

  if ((level & CC_FONT_CHOOSER_LEVEL_SIZE) != 0)
    {
      gtk_widget_show (priv->size_slider);
      gtk_widget_show (priv->size_spin);
    }
  else
   {
      gtk_widget_hide (priv->size_slider);
      gtk_widget_hide (priv->size_spin);
   }

  cc_font_chooser_widget_load_fonts (fontchooser, TRUE);

  g_object_notify (G_OBJECT (fontchooser), "level");
}

static CcFontChooserLevel
cc_font_chooser_widget_get_level (CcFontChooserWidget *fontchooser)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;

  return priv->level;
}

static void
cc_font_chooser_widget_set_language (CcFontChooserWidget *fontchooser,
                                      const char           *language)
{
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  PangoLanguage *lang;

  lang = pango_language_from_string (language);
  if (priv->language == lang)
    return;

  priv->language = lang;
  g_object_notify (G_OBJECT (fontchooser), "language");

  cc_font_chooser_widget_update_preview_attributes (fontchooser);
}

static void
cc_font_chooser_widget_iface_init (CcFontChooserIface *iface)
{
  iface->get_font_family = cc_font_chooser_widget_get_family;
  iface->get_font_face = cc_font_chooser_widget_get_face;
  iface->get_font_size = cc_font_chooser_widget_get_size;
  iface->set_filter_func = cc_font_chooser_widget_set_filter_func;
  iface->set_font_map = cc_font_chooser_widget_set_font_map;
  iface->get_font_map = cc_font_chooser_widget_get_font_map;
}

gboolean
cc_font_chooser_widget_handle_event (GtkWidget   *widget,
                                      GdkEventKey *key_event)
{
  CcFontChooserWidget *fontchooser = CC_FONT_CHOOSER_WIDGET (widget);
  CcFontChooserWidgetPrivate *priv = fontchooser->priv;
  GdkEvent *event = (GdkEvent *)key_event;

  return gtk_search_entry_handle_event (GTK_SEARCH_ENTRY (priv->search_entry), event);
}

GAction *
cc_font_chooser_widget_get_tweak_action (GtkWidget *widget)
{
  return CC_FONT_CHOOSER_WIDGET (widget)->priv->tweak_action;
}
