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
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */

#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#include "list-box-helper.h"
#include "cc-region-panel.h"
#include "cc-region-resources.h"
#include "cc-language-chooser.h"
#include "cc-format-chooser.h"

#include "cc-common-language.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#include <act/act.h>

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_INPUT_SOURCES        "sources"

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define DEFAULT_LOCALE "ko_KR.UTF-8"
#define XSESSIONRC ".xsessionrc"

static const gchar *xsessionrc_template = "export LANG=%s\n"
                                          "export LC_TIME=%s";

typedef enum {
        CHOOSE_LANGUAGE,
        CHOOSE_REGION,
        ADD_INPUT,
        REMOVE_INPUT,
        MOVE_UP_INPUT,
        MOVE_DOWN_INPUT,
} SystemOp;

struct _CcRegionPanel {
        CcPanel          parent_instance;

        GtkLabel        *formats_label;
        GtkListBoxRow   *formats_row;
        GtkToggleButton *login_button;
        GtkLabel        *login_label;
        GtkLabel        *language_label;
        GtkListBox      *language_list;
        GtkListBoxRow   *language_row;
        GtkFrame        *language_section_frame;
        GtkButton       *restart_button;
        GtkRevealer     *restart_revealer;

        gboolean     login;
        GPermission *permission;
        SystemOp     op;
        GDBusProxy  *localed;
        GDBusProxy  *session;

        ActUserManager *user_manager;
        ActUser        *user;
        GSettings      *locale_settings;

        gchar *language;
        gchar *region;
        gchar *system_language;
        gchar *system_region;

};

CC_PANEL_REGISTER (CcRegionPanel, cc_region_panel)

static void update_language_label (CcRegionPanel *self);
static void update_language_from_user (CcRegionPanel *self);

typedef struct
{
        CcRegionPanel *panel;
} RowData;

static void
cc_region_panel_finalize (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);
        GtkWidget *chooser;

        if (self->user_manager) {
                g_signal_handlers_disconnect_by_data (self->user_manager, self);
                self->user_manager = NULL;
        }

        if (self->user) {
                g_signal_handlers_disconnect_by_data (self->user, self);
                self->user = NULL;
        }

        g_clear_object (&self->permission);
        g_clear_object (&self->localed);
        g_clear_object (&self->session);
        g_clear_object (&self->locale_settings);

        g_free (self->language);
        g_free (self->region);
        g_free (self->system_language);
        g_free (self->system_region);

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (chooser)
                gtk_widget_destroy (chooser);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_constructed (GObject *object)
{
        CcRegionPanel *self = CC_REGION_PANEL (object);

        G_OBJECT_CLASS (cc_region_panel_parent_class)->constructed (object);

        if (self->permission)
                cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (object)),
                                                 GTK_WIDGET (self->login_button),
                                                 GTK_POS_RIGHT);
}

static const char *
cc_region_panel_get_help_uri (CcPanel *panel)
{
        return "help:gnome-help/prefs-language";
}

static void
set_account_property (const gchar *prop_name, const gchar *value)
{
        g_return_if_fail (prop_name || value);

        GVariant   *variant;
        GDBusProxy *proxy;
        GError     *error = NULL;
        gchar      *user_path = NULL;

        user_path = g_strdup_printf ("/org/freedesktop/Accounts/User%i", getuid ());

        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_CALL_FLAGS_NONE,
                                               NULL,
                                               "org.freedesktop.Accounts",
                                               user_path,
                                               "org.freedesktop.Accounts.User",
                                               NULL,
                                               &error);

        if (!proxy) {
                g_warning ("%s\n", error->message);
                g_free (user_path);
                g_error_free (error);
                return;
        }

        variant = g_dbus_proxy_call_sync (proxy,
                                          prop_name,
                                          g_variant_new ("(s)", value),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);

        if (!variant) {
                g_warning ("%s\n", error->message);
                g_error_free (error);
        } else {
                g_variant_unref (variant);
        }

        g_free (user_path);
        g_object_unref (proxy);
}

static gchar *
get_account_property (const gchar *prop_name)
{
        GDBusConnection  *bus;
        gchar            *user_path;
        GError           *error = NULL;
        GVariant         *properties;
        GVariantIter     *iter;
        gchar            *key;
        GVariant         *value;
        gchar            *ret = NULL;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        user_path = g_strdup_printf ("/org/freedesktop/Accounts/User%i", getuid ());

        properties = g_dbus_connection_call_sync (bus,
                        "org.freedesktop.Accounts",
                        user_path,
                        "org.freedesktop.DBus.Properties",
                        "GetAll",
                        g_variant_new ("(s)", "org.freedesktop.Accounts.User"),
                        G_VARIANT_TYPE ("(a{sv})"),
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        NULL,
                        &error);
        if (!properties) {
                g_warning ("Error calling GetAll() when retrieving properties for %s: %s", user_path, error->message);
                g_error_free (error);
                goto out;
        }

        g_variant_get (properties, "(a{sv})", &iter);
        while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
                if (g_strcmp0 (key, prop_name) == 0) {
                        g_variant_get (value, "s", &ret);
                        break;
                }
        }

        g_variant_unref (properties);
        g_variant_iter_free (iter);

out:
        g_object_unref (bus);
        g_free (user_path);

        return ret;
}

static char *
get_session_property (CcRegionPanel *self, int category)
{
        gchar *ret = NULL;
        GVariant *variant = NULL;

        if (self->session) {
                variant = g_dbus_proxy_call_sync (self->session,
                                                  "GetLocale",
                                                  g_variant_new ("(i)", category),
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  -1,
                                                  NULL,
                                                  NULL);
        }

        if (variant) {
                const gchar *current_locale;
                g_variant_get (variant, "(&s)", &current_locale);
                ret = g_strdup (current_locale);
                g_variant_unref (variant);
        }

        return ret;
}

static GFile *
get_needs_restart_file (void)
{
        g_autofree gchar *path = NULL;

        path = g_build_filename (g_get_user_runtime_dir (),
                                 "gnome-control-center-region-needs-restart",
                                 NULL);
        return g_file_new_for_path (path);
}

static void
restart_now (CcRegionPanel *self)
{
        g_autoptr(GFile) file = NULL;

        file = get_needs_restart_file ();
        g_file_delete (file, NULL, NULL);

        g_dbus_proxy_call (self->session,
                           "Logout",
                           g_variant_new ("(u)", 1),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
set_restart_notification_visible (CcRegionPanel *self,
                                  const gchar   *locale,
                                  gboolean       visible)
{
        g_autofree gchar *current_locale = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileOutputStream) output_stream = NULL;
        g_autoptr(GError) error = NULL;

        if (locale) {
                current_locale = g_strdup (setlocale (LC_MESSAGES, NULL));
                setlocale (LC_MESSAGES, locale);
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (self->restart_revealer), visible);

        if (locale)
                setlocale (LC_MESSAGES, current_locale);

        file = get_needs_restart_file ();

        if (!visible) {
                g_file_delete (file, NULL, NULL);
                return;
        }

        output_stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (output_stream == NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                        g_warning ("Unable to create %s: %s", g_file_get_path (file), error->message);
        }
}

static void
maybe_notify (CcRegionPanel *self,
              int            category,
              const gchar   *target_locale)
{
        gboolean changed = FALSE;
        gchar **init_language_tokens;
        gchar **init_region_tokens;
        gchar **target_locale_tokens;
        gchar *init_language = NULL;
        gchar *init_region = NULL;

        init_language = get_session_property (self, LC_MESSAGES);
        init_region = get_session_property (self, LC_TIME);

        init_language_tokens = g_strsplit (init_language, ".", -1);
        init_region_tokens = g_strsplit (init_region, ".", -1);

        target_locale_tokens = g_strsplit (target_locale, ".", -1);

        if (category == LC_MESSAGES) {
                gchar **current_region_tokens = NULL;
                if (self->region)
                        current_region_tokens = g_strsplit (self->region, ".", -1);
                if (!g_str_equal (init_language_tokens[0], target_locale_tokens[0]))
                        changed = TRUE;
                if (current_region_tokens &&
                    !g_str_equal (init_region_tokens[0], current_region_tokens[0]))
                        changed = TRUE;
                g_strfreev (current_region_tokens);
        } else {
                gchar **current_language_tokens = NULL;
                if (self->language)
                        current_language_tokens = g_strsplit (self->language, ".", -1);
                if (!g_str_equal (init_region_tokens[0], target_locale_tokens[0]))
                        changed = TRUE;
                if (current_language_tokens &&
                    !g_str_equal (init_language_tokens[0], current_language_tokens[0]))
                        changed = TRUE;
                g_strfreev (current_language_tokens);
        }

        g_strfreev (init_language_tokens);
        g_strfreev (init_region_tokens);
        g_strfreev (target_locale_tokens);
        g_free (init_language);
        g_free (init_region);

        set_restart_notification_visible (self,
                                          category == LC_MESSAGES ? target_locale : NULL,
                                          changed);
}

static void set_localed_locale (CcRegionPanel *self, gboolean lang_changed);

static void
set_system_language (CcRegionPanel *self,
                     const gchar   *language)
{
        if (g_strcmp0 (language, self->system_language) == 0)
                return;

        g_free (self->system_language);
        self->system_language = g_strdup (language);

        set_localed_locale (self, TRUE);
}

static void
update_xsessionrc (CcRegionPanel *self,
                   const gchar   *language,
                   const gchar   *region)
{
        g_autofree gchar *xsessionrc;
        g_autofree gchar *contents;

        xsessionrc = g_build_filename (g_get_home_dir (), XSESSIONRC, NULL);
        contents = g_strdup_printf (xsessionrc_template, language, region);

        g_file_set_contents (xsessionrc, contents, -1, NULL);
}

static void
update_language (CcRegionPanel *self,
                 const gchar   *language)
{
        if (self->login) {
                set_system_language (self, language);
        } else {
                if (g_strcmp0 (language, self->language) == 0)
                        return;
                set_account_property ("SetLanguage", language);
                update_language_from_user (self);
                update_xsessionrc (self, language, self->region);
                maybe_notify (self, LC_MESSAGES, language);
        }
}

static void
language_response (CcRegionPanel     *self,
                   gint               response_id,
                   CcLanguageChooser *chooser)
{
        const gchar *language;

        if (response_id == GTK_RESPONSE_OK) {
                language = cc_language_chooser_get_language (chooser);
                update_language (self, language);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
set_system_region (CcRegionPanel *self,
                   const gchar   *region)
{
        if (g_strcmp0 (region, self->system_region) == 0)
                return;

        g_free (self->system_region);
        self->system_region = g_strdup (region);

        set_localed_locale (self, FALSE);
}

static void
update_region (CcRegionPanel *self,
               const gchar   *region)
{
        if (self->login) {
                set_system_region (self, region);
        } else {
                if (g_strcmp0 (region, self->region) == 0)
                        return;
                g_settings_set_string (self->locale_settings, KEY_REGION, region);
                update_xsessionrc (self, self->language, region);
                maybe_notify (self, LC_TIME, region);
        }
}

static void
format_response (CcRegionPanel   *self,
                 gint             response_id,
                 CcFormatChooser *chooser)
{
        const gchar *region;

        if (response_id == GTK_RESPONSE_OK) {
                region = cc_format_chooser_get_region (chooser);
                update_region (self, region);
        }

        gtk_widget_destroy (GTK_WIDGET (chooser));
}

static const gchar *
get_effective_language (CcRegionPanel *self)
{
        if (self->login)
                return self->system_language;
        else
                return self->language;
}

static void
show_language_chooser (CcRegionPanel *self)
{
        CcLanguageChooser *chooser;

        chooser = cc_language_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
        cc_language_chooser_set_language (chooser, get_effective_language (self));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (language_response), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (chooser));
}

static const gchar *
get_effective_region (CcRegionPanel *self)
{
        const gchar *region;

        if (self->login)
                region = self->system_region;
        else
                region = self->region;

        /* Region setting might be empty - show the language because
         * that's what LC_TIME and others will effectively be when the
         * user logs in again. */
        if (region == NULL || region[0] == '\0')
                region = get_effective_language (self);

        return region;
}

static void
show_region_chooser (CcRegionPanel *self)
{
        CcFormatChooser *chooser;

        chooser = cc_format_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
        cc_format_chooser_set_region (chooser, get_effective_region (self));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (format_response), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (chooser));
}

static void
permission_acquired (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        g_autoptr(GError) error = NULL;
        gboolean allowed;

        allowed = g_permission_acquire_finish (self->permission, res, &error);
        if (error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to acquire permission: %s\n", error->message);
                return;
        }

        if (allowed) {
                switch (self->op) {
                case CHOOSE_LANGUAGE:
                        show_language_chooser (self);
                        break;
                case CHOOSE_REGION:
                        show_region_chooser (self);
                        break;
                default:
                        g_warning ("Unknown privileged operation: %d\n", self->op);
                        break;
                }
        }
}

static void
activate_language_row (CcRegionPanel *self,
                       GtkListBoxRow *row)
{
        if (row == self->language_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_language_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        self->op = CHOOSE_LANGUAGE;
                        g_permission_acquire_async (self->permission,
                                                    cc_panel_get_cancellable (CC_PANEL (self)),
                                                    permission_acquired,
                                                    self);
                }
        } else if (row == self->formats_row) {
                if (!self->login || g_permission_get_allowed (self->permission)) {
                        show_region_chooser (self);
                } else if (g_permission_get_can_acquire (self->permission)) {
                        self->op = CHOOSE_REGION;
                        g_permission_acquire_async (self->permission,
                                                    cc_panel_get_cancellable (CC_PANEL (self)),
                                                    permission_acquired,
                                                    self);
                }
        }
}

static void
update_region_label (CcRegionPanel *self)
{
        const gchar *region = get_effective_region (self);
        g_autofree gchar *name = NULL;

        if (region)
                name = gnome_get_country_from_locale (region, region);

        if (!name)
                name = gnome_get_country_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (self->formats_label, name);
}

static void
update_region_from_setting (CcRegionPanel *self)
{
        g_free (self->region);
        self->region = g_settings_get_string (self->locale_settings, KEY_REGION);
        if (!self->region || g_str_equal (self->region, "")) {
                g_free (self->region);
                self->region = get_session_property (self, LC_TIME);
        }

        update_region_label (self);
        maybe_notify (self, LC_TIME, self->region);
}

static void
update_language_label (CcRegionPanel *self)
{
        const gchar *language = get_effective_language (self);
        g_autofree gchar *name = NULL;

        if (language)
                name = gnome_get_language_from_locale (language, language);

        if (!name)
                name = gnome_get_language_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        gtk_label_set_label (self->language_label, name);
}

static void
update_language_from_user (CcRegionPanel *self)
{
        g_free (self->language);
        self->language = get_account_property ("Language");

        if (!self->language || g_str_equal (self->language, "")) {
                g_free (self->language);
                self->language = get_session_property (self, LC_MESSAGES);
        }

        update_language_label (self);
        maybe_notify (self, LC_MESSAGES, self->language);
}

static void
setup_language_section (CcRegionPanel *self)
{
        self->user = act_user_manager_get_user_by_id (self->user_manager, getuid ());
//        g_signal_connect_object (self->user, "notify::language",
//                                 G_CALLBACK (update_language_from_user), self, G_CONNECT_SWAPPED);

        self->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_object (self->locale_settings, "changed::" KEY_REGION,
                                 G_CALLBACK (update_region_from_setting), self, G_CONNECT_SWAPPED);

        gtk_list_box_set_selection_mode (self->language_list,
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (self->language_list,
                                      cc_list_box_update_header_func,
                                      NULL, NULL);
        g_signal_connect_object (self->language_list, "row-activated",
                                 G_CALLBACK (activate_language_row), self, G_CONNECT_SWAPPED);

        update_language_from_user (self);
        update_region_from_setting (self);
}

static GtkWidget *
find_sibling (GtkContainer *container, GtkWidget *child)
{
        g_autoptr(GList) list = NULL;
        GList *c, *l;
        GtkWidget *sibling;

        list = gtk_container_get_children (container);
        c = g_list_find (list, child);

        for (l = c->next; l; l = l->next) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        return sibling;
        }

        for (l = c->prev; l; l = l->prev) {
                sibling = l->data;
                if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
                        return sibling;
        }

        return NULL;
}

static void
options_response (GtkDialog     *options,
                  gint           response_id,
                  CcRegionPanel *self)
{
        gtk_widget_destroy (GTK_WIDGET (options));
}

static void
update_locale_done_cb (GPid pid, gint status, gpointer data)
{
  g_spawn_close_pid (pid);
}

static void
set_localed_locale (CcRegionPanel *self, gboolean lang_changed)
{
         GPid pid;
         gchar **argv = NULL;
         gchar *cmd = NULL, *args = NULL;
 
         if (lang_changed) {
                 args = g_strdup_printf ("LANG=%s", self->system_language);
        } else {
                 args = g_strdup_printf ("LC_TIME=%s LC_NUMERIC=%s "
                                         "LC_COLLATE=%s LC_MONETARY=%s "
                                         "LC_PAPER=%s LC_NAME=%s "
                                         "LC_ADDRESS=%s LC_TELEPHONE=%s "
                                         "LC_MEASUREMENT=%s LC_IDENTIFICATION=%s",
                                         self->system_region, self->system_region,
                                         self->system_region, self->system_region,
                                         self->system_region, self->system_region,
                                         self->system_region, self->system_region,
                                         self->system_region, self->system_region);
         }
 
         cmd = g_strdup_printf ("/usr/bin/pkexec /usr/sbin/update-locale %s", args);
 
         g_shell_parse_argv (cmd, NULL, &argv, NULL);
 
         if (g_spawn_async (NULL, argv, NULL,
                            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                            NULL, NULL, &pid, NULL)) {
                 g_child_watch_add (pid, (GChildWatchFunc) update_locale_done_cb, NULL);
         }
 
         g_free (args);
         g_free (cmd);
         g_strfreev (argv);
}

static void
update_system_locale (CcRegionPanel *self)
{
        gchar **lines = NULL;
        gchar *contents = NULL;
        gchar *lang = NULL, *messages = NULL, *region = NULL;

        g_file_get_contents ("/etc/default/locale", &contents, NULL, NULL);
        if (contents) {
                guint i = 0;
                lines = g_strsplit (contents, "\n", -1);
                for (i = 0; lines[i] != NULL; i++) {
                        if (g_str_has_prefix (lines[i], "LANG=")) {
                                lang = lines[i] + strlen ("LANG=");
                        } else if (g_str_has_prefix (lines[i], "LC_MESSAGES")) {
                                messages = lines[i] + strlen ("LC_MESSAGES=");
                        } else if (g_str_has_prefix (lines[i], "LC_TIME")) {
                                region = lines[i] + strlen ("LC_TIME=");
                        }/* else if (g_str_has_prefix (lines[i], "LC_NUMERIC")) {
                                region = lines[i] + strlen ("LC_NUMERIC=");
                        } else if (g_str_has_prefix (lines[i], "LC_COLLATE")) {
                                region = lines[i] + strlen ("LC_COLLATE=");
                        } else if (g_str_has_prefix (lines[i], "LC_MONETARY")) {
                                region = lines[i] + strlen ("LC_MONETARY=");
                        } else if (g_str_has_prefix (lines[i], "LC_PAPER")) {
                                region = lines[i] + strlen ("LC_PAPER=");
                        } else if (g_str_has_prefix (lines[i], "LC_NAME")) {
                                region = lines[i] + strlen ("LC_NAME=");
                        } else if (g_str_has_prefix (lines[i], "LC_ADDRESS")) {
                                region = lines[i] + strlen ("LC_ADDRESS=");
                        } else if (g_str_has_prefix (lines[i], "LC_TELEPHONE")) {
                                region = lines[i] + strlen ("LC_TELEPHONE=");
                        } else if (g_str_has_prefix (lines[i], "LC_MEASUREMENT")) {
                                region = lines[i] + strlen ("LC_MEASUREMENT=");
                        } else if (g_str_has_prefix (lines[i], "LC_IDENTIFICATION")) {
                                region = lines[i] + strlen ("LC_IDENTIFICATION=");
                        }*/
                }
        }

        if (!lang) {
                lang = setlocale (LC_MESSAGES, NULL);
        }
        if (!messages) {
                messages = lang;
        }
        if (!region) {
                region = lang;
        }

        g_free (self->system_language);
        self->system_language = g_strdup (messages);
        g_free (self->system_region);
        self->system_region = g_strdup (region);

        update_language_label (self);
        update_region_label (self);

        if (contents) g_free (contents);
        if (lines) g_strfreev (lines);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact localed: %s\n", error->message);
                return;
        }

        self->localed = proxy;

        gtk_widget_set_sensitive (GTK_WIDGET (self->login_button), TRUE);

        update_system_locale (self);
}

static void
locale_file_changed_cb (GFileMonitor      *monitor,
                        GFile             *file,
                        GFile             *other_file,
                        GFileMonitorEvent  event_type,
                        gpointer           user_data)
{
        CcRegionPanel *self = CC_REGION_PANEL (user_data);

        switch (event_type)
        {
            case G_FILE_MONITOR_EVENT_CHANGED:
            case G_FILE_MONITOR_EVENT_DELETED:
            case G_FILE_MONITOR_EVENT_CREATED:
            {
                    update_system_locale (self);
                    break;
            }

            default:
                break;
        }
}

static void
login_changed (CcRegionPanel *self)
{
        gboolean can_acquire;

        self->login = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->login_button));
        gtk_widget_set_visible (GTK_WIDGET (self->login_label), self->login);

        if (self->login) {
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->restart_revealer), FALSE);
        } else {
                g_autoptr(GFile) needs_restart_file = NULL;
                needs_restart_file = get_needs_restart_file ();
                if (g_file_query_exists (needs_restart_file, NULL))
                        set_restart_notification_visible (self, NULL, TRUE);
        }

        can_acquire = self->permission &&
                (g_permission_get_allowed (self->permission) ||
                 g_permission_get_can_acquire (self->permission));
        /* FIXME: insensitive doesn't look quite right for this */
        gtk_widget_set_sensitive (GTK_WIDGET (self->language_section_frame), !self->login || can_acquire);

        update_language_label (self);
        update_region_label (self);
}

static void
setup_login_button (CcRegionPanel *self)
{
        g_autoptr(GDBusConnection) bus = NULL;
        g_autoptr(GError) error = NULL;
        GFile *file;
        GFileMonitor *monitor;

        self->permission = polkit_permission_new_sync ("org.gnome.controlcenter.update-locale", NULL, NULL, &error);
        if (self->permission == NULL) {
                g_warning ("Could not get 'org.gnome.controlcenter.update-locale' permission: %s",
                           error->message);
                return;
        }

        file = g_file_new_for_path ("/etc/default/locale");

        monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
        if (error) {
            g_error_free (error);
        } else {
            g_signal_connect (monitor, "changed", G_CALLBACK (locale_file_changed_cb), self);
        }
        g_object_unref (file);

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                          G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                          NULL,
                          "org.freedesktop.locale1",
                          "/org/freedesktop/locale1",
                          "org.freedesktop.locale1",
                          cc_panel_get_cancellable (CC_PANEL (self)),
                          (GAsyncReadyCallback) localed_proxy_ready,
                          self);

        self->login_button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new_with_mnemonic (_("Login _Screen")));
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->login_button)),
                                     "text-button");
        gtk_widget_set_valign (GTK_WIDGET (self->login_button), GTK_ALIGN_CENTER);
        gtk_widget_set_visible (GTK_WIDGET (self->login_button), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (self->login_button), FALSE);
        g_signal_connect_object (self->login_button, "notify::active",
                                 G_CALLBACK (login_changed), self, G_CONNECT_SWAPPED);
}

static void
session_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPanel *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact gnome-session: %s\n", error->message);
                return;
        }

        self->session = proxy;

        setup_language_section (self);
}

static void
cc_region_panel_class_init (CcRegionPanelClass * klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

        panel_class->get_help_uri = cc_region_panel_get_help_uri;

        object_class->constructed = cc_region_panel_constructed;
        object_class->finalize = cc_region_panel_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-region-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, login_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_label);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_list);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, language_section_frame);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_button);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPanel, restart_revealer);

        gtk_widget_class_bind_template_callback (widget_class, restart_now);
}

static void
cc_region_panel_init (CcRegionPanel *self)
{
        g_autoptr(GFile) needs_restart_file = NULL;

        g_resources_register (cc_region_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->user_manager = act_user_manager_get_default ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  cc_panel_get_cancellable (CC_PANEL (self)),
                                  session_proxy_ready,
                                  self);

        setup_login_button (self);
}
