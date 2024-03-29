/* gtkfontchooserutils.h - Private utility functions for implementing a
 *                           CcFontChooser interface
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
 * Based on gtkfilechooserutils.c:
 *	Copyright (C) 2003 Red Hat, Inc.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "cc-font-chooser-utils.h"
#include "cc-font-define.h"

static CcFontChooser *
get_delegate (CcFontChooser *receiver)
{
  return g_object_get_qdata (G_OBJECT (receiver),
                             CC_FONT_CHOOSER_DELEGATE_QUARK);
}

static PangoFontFamily *
delegate_get_font_family (CcFontChooser *chooser)
{
  return cc_font_chooser_get_font_family (get_delegate (chooser));
}

static PangoFontFace *
delegate_get_font_face (CcFontChooser *chooser)
{
  return cc_font_chooser_get_font_face (get_delegate (chooser));
}

static int
delegate_get_font_size (CcFontChooser *chooser)
{
  return cc_font_chooser_get_font_size (get_delegate (chooser));
}

static void
delegate_set_filter_func (CcFontChooser    *chooser,
                          GtkFontFilterFunc  filter_func,
                          gpointer           filter_data,
                          GDestroyNotify     data_destroy)
{
  cc_font_chooser_set_filter_func (get_delegate (chooser),
                                    filter_func,
                                    filter_data,
                                    data_destroy);
}

static void
delegate_set_font_map (CcFontChooser *chooser,
                       PangoFontMap   *map)
{
  cc_font_chooser_set_font_map (get_delegate (chooser), map);
}

static PangoFontMap *
delegate_get_font_map (CcFontChooser *chooser)
{
  return cc_font_chooser_get_font_map (get_delegate (chooser));
}

static void
delegate_notify (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  gpointer iface;

  iface = g_type_interface_peek (g_type_class_peek (G_OBJECT_TYPE (object)),
                                 CC_TYPE_FONT_CHOOSER);
  if (g_object_interface_find_property (iface, pspec->name))
    g_object_notify_by_pspec (user_data, pspec);
}

static void
delegate_font_activated (CcFontChooser *receiver,
                         const gchar    *fontname,
                         CcFontChooser *delegate)
{
  _cc_font_chooser_font_activated (delegate, fontname);
}

GQuark
_cc_font_chooser_delegate_get_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gtk-font-chooser-delegate");

  return quark;
}

/**
 * _cc_font_chooser_install_properties:
 * @klass: the class structure for a type deriving from #GObject
 *
 * Installs the necessary properties for a class implementing
 * #CcFontChooser. A #GtkParamSpecOverride property is installed
 * for each property, using the values from the #CcFontChooserProp
 * enumeration. The caller must make sure itself that the enumeration
 * values don’t collide with some other property values they
 * are using.
 */
void
_cc_font_chooser_install_properties (GObjectClass *klass)
{
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_FONT,
                                    "font");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_FONT_DESC,
                                    "font-desc");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_PREVIEW_TEXT,
                                    "preview-text");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY,
                                    "show-preview-entry");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_LEVEL,
                                    "level");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_FONT_FEATURES,
                                    "font-features");
  g_object_class_override_property (klass,
                                    CC_FONT_CHOOSER_PROP_LANGUAGE,
                                    "language");
}

/**
 * _cc_font_chooser_delegate_iface_init:
 * @iface: a #CcFontChooserIface
 *
 * An interface-initialization function for use in cases where
 * an object is simply delegating the methods, signals of
 * the #CcFontChooser interface to another object.
 * _cc_font_chooser_set_delegate() must be called on each
 * instance of the object so that the delegate object can
 * be found.
 */
void
_cc_font_chooser_delegate_iface_init (CcFontChooserIface *iface)
{
  iface->get_font_family = delegate_get_font_family;
  iface->get_font_face = delegate_get_font_face;
  iface->get_font_size = delegate_get_font_size;
  iface->set_filter_func = delegate_set_filter_func;
  iface->set_font_map = delegate_set_font_map;
  iface->get_font_map = delegate_get_font_map;
}

/**
 * _cc_font_chooser_set_delegate:
 * @receiver: a #GObject implementing #CcFontChooser
 * @delegate: another #GObject implementing #CcFontChooser
 *
 * Establishes that calls on @receiver for #CcFontChooser
 * methods should be delegated to @delegate, and that
 * #CcFontChooser signals emitted on @delegate should be
 * forwarded to @receiver. Must be used in conjunction with
 * _cc_font_chooser_delegate_iface_init().
 */
void
_cc_font_chooser_set_delegate (CcFontChooser *receiver,
                               CcFontChooser *delegate)
{
  g_return_if_fail (CC_IS_FONT_CHOOSER (receiver));
  g_return_if_fail (CC_IS_FONT_CHOOSER (delegate));
  
  g_object_set_qdata (G_OBJECT (receiver),
                      CC_FONT_CHOOSER_DELEGATE_QUARK,
  		      delegate);
  
  g_signal_connect (delegate, "notify",
  		    G_CALLBACK (delegate_notify), receiver);
  g_signal_connect (delegate, "font-activated",
  		    G_CALLBACK (delegate_font_activated), receiver);
}
