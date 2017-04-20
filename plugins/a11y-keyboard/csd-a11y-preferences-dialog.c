/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "csd-a11y-preferences-dialog.h"

#define SM_DBUS_NAME      "org.gnome.SessionManager"
#define SM_DBUS_PATH      "/org/gnome/SessionManager"
#define SM_DBUS_INTERFACE "org.gnome.SessionManager"


#define CSD_A11Y_PREFERENCES_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_A11Y_PREFERENCES_DIALOG, CsdA11yPreferencesDialogPrivate))

#define GTKBUILDER_UI_FILE "csd-a11y-preferences-dialog.ui"

#define INTERFACE_SCHEMA          "org.cinnamon.desktop.interface"
#define KEY_TEXT_SCALING_FACTOR   "text-scaling-factor"

#define KEYBOARD_A11Y_SCHEMA      "org.cinnamon.desktop.a11y.keyboard"
#define KEY_STICKY_KEYS_ENABLED   "stickykeys-enable"
#define KEY_BOUNCE_KEYS_ENABLED   "bouncekeys-enable"
#define KEY_SLOW_KEYS_ENABLED     "slowkeys-enable"

#define KEY_AT_SCHEMA                "org.cinnamon.desktop.a11y.applications"
#define KEY_AT_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_AT_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_AT_SCREEN_READER_ENABLED    "screen-reader-enabled"

#define DPI_FACTOR_LARGE   1.25
#define DPI_FACTOR_LARGER  1.5
#define DPI_FACTOR_LARGEST 2.0

#define KEY_GTK_THEME          "gtk-theme"
#define KEY_ICON_THEME         "icon-theme"
#define KEY_METACITY_THEME     "theme"

#define HIGH_CONTRAST_THEME    "HighContrast"

struct CsdA11yPreferencesDialogPrivate
{
        GtkWidget *large_print_checkbutton;
        GtkWidget *high_contrast_checkbutton;

        GSettings *a11y_settings;
        GSettings *interface_settings;
        GSettings *apps_settings;
};

enum {
        PROP_0,
};

static void     csd_a11y_preferences_dialog_finalize    (GObject                       *object);

G_DEFINE_TYPE (CsdA11yPreferencesDialog, csd_a11y_preferences_dialog, GTK_TYPE_DIALOG)

static GObject *
csd_a11y_preferences_dialog_constructor (GType                  type,
                                         guint                  n_construct_properties,
                                         GObjectConstructParam *construct_properties)
{
        CsdA11yPreferencesDialog      *a11y_preferences_dialog;

        a11y_preferences_dialog = CSD_A11Y_PREFERENCES_DIALOG (G_OBJECT_CLASS (csd_a11y_preferences_dialog_parent_class)->constructor (type,
                                                                                                                                       n_construct_properties,
                                                                                                                                       construct_properties));

        return G_OBJECT (a11y_preferences_dialog);
}

static void
csd_a11y_preferences_dialog_class_init (CsdA11yPreferencesDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_a11y_preferences_dialog_constructor;
        object_class->finalize = csd_a11y_preferences_dialog_finalize;

        g_type_class_add_private (klass, sizeof (CsdA11yPreferencesDialogPrivate));
}

static void
on_response (CsdA11yPreferencesDialog *dialog,
             gint                      response_id)
{
        switch (response_id) {
        default:
                break;
        }
}

static gboolean
config_get_large_print (CsdA11yPreferencesDialog *dialog,
			gboolean                 *is_writable)
{
        gboolean     ret;
        gdouble      factor;

        factor = g_settings_get_double (dialog->priv->interface_settings, KEY_TEXT_SCALING_FACTOR);

        ret = (factor > 1.0);

        *is_writable = g_settings_is_writable (dialog->priv->interface_settings, KEY_TEXT_SCALING_FACTOR);

        return ret;
}

static void
config_set_large_print (CsdA11yPreferencesDialog *dialog,
			gboolean                  enabled)
{
        if (enabled)
                g_settings_set_double (dialog->priv->interface_settings, KEY_TEXT_SCALING_FACTOR, DPI_FACTOR_LARGER);
        else
                g_settings_reset (dialog->priv->interface_settings, KEY_TEXT_SCALING_FACTOR);
}

static gboolean
config_get_high_contrast (CsdA11yPreferencesDialog *dialog)
{
        gboolean ret;
        char    *gtk_theme;

        ret = FALSE;

        gtk_theme = g_settings_get_string (dialog->priv->interface_settings, KEY_GTK_THEME);
        if (gtk_theme != NULL && g_str_equal (gtk_theme, HIGH_CONTRAST_THEME)) {
                ret = TRUE;
        }

        g_free (gtk_theme);

        return ret;
}

static void
config_set_high_contrast (gboolean enabled)
{
        GSettings *settings;
        GSettings *wm_settings;

        settings = g_settings_new ("org.cinnamon.desktop.interface");
        wm_settings = g_settings_new ("org.cinnamon.desktop.wm.preferences");

        if (enabled) {
                g_settings_set_string (settings, KEY_GTK_THEME, HIGH_CONTRAST_THEME);
                g_settings_set_string (settings, KEY_ICON_THEME, HIGH_CONTRAST_THEME);
                /* there isn't a high contrast metacity theme afaik */
        } else {
                g_settings_reset (settings, KEY_GTK_THEME);
                g_settings_reset (settings, KEY_ICON_THEME);
                g_settings_reset (wm_settings, KEY_METACITY_THEME);
        }

        g_object_unref (settings);
        g_object_unref (wm_settings);
}

static gboolean
config_have_at_gsettings_condition (const char *condition)
{
        GDBusProxy      *sm_proxy;
        GDBusConnection *connection;
        GError          *error;
        GVariant        *res;
        gboolean         is_handled;

        error = NULL;
        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
        if (connection == NULL) {
                g_warning ("Unable to connect to session bus: %s", error->message);
                g_error_free (error);
                return FALSE;
        }
        sm_proxy = g_dbus_proxy_new_sync (connection,
                                          0, NULL,
                                          SM_DBUS_NAME,
                                          SM_DBUS_PATH,
                                          SM_DBUS_INTERFACE,
                                          NULL,
                                          &error);
        if (sm_proxy == NULL) {
                g_warning ("Unable to get proxy for %s: %s", SM_DBUS_NAME, error->message);
                g_error_free (error);
                return FALSE;
        }

        is_handled = FALSE;
        res = g_dbus_proxy_call_sync (sm_proxy,
                                      "IsAutostartConditionHandled",
                                      g_variant_new ("(s)", condition),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1, NULL, &error);
        if (! res) {
                g_warning ("Unable to call IsAutostartConditionHandled (%s): %s",
                           condition,
                           error->message);
        }

        if (g_variant_is_of_type (res, G_VARIANT_TYPE_BOOLEAN)) {
                is_handled = g_variant_get_boolean (res);
        }

        g_object_unref (sm_proxy);
        g_variant_unref (res);

        return is_handled;
}

static void
on_high_contrast_checkbutton_toggled (GtkToggleButton          *button,
                                      CsdA11yPreferencesDialog *dialog)
{
        config_set_high_contrast (gtk_toggle_button_get_active (button));
}

static void
on_large_print_checkbutton_toggled (GtkToggleButton          *button,
                                    CsdA11yPreferencesDialog *dialog)
{
        config_set_large_print (dialog, gtk_toggle_button_get_active (button));
}

static void
ui_set_high_contrast (CsdA11yPreferencesDialog *dialog,
                      gboolean                  enabled)
{
        gboolean active;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->high_contrast_checkbutton));
        if (active != enabled) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->high_contrast_checkbutton), enabled);
        }
}

static void
ui_set_large_print (CsdA11yPreferencesDialog *dialog,
                    gboolean                  enabled)
{
        gboolean active;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->large_print_checkbutton));
        if (active != enabled) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->large_print_checkbutton), enabled);
        }
}

static void
setup_dialog (CsdA11yPreferencesDialog *dialog,
              GtkBuilder               *builder)
{
        GtkWidget   *widget;
        gboolean     enabled;
        gboolean     is_writable;
        GSettings   *settings;

        dialog->priv->a11y_settings = g_settings_new (KEYBOARD_A11Y_SCHEMA);
        settings = dialog->priv->a11y_settings;

        dialog->priv->interface_settings = g_settings_new (INTERFACE_SCHEMA);
        dialog->priv->apps_settings = g_settings_new (KEY_AT_SCHEMA);

        /* Sticky keys */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "sticky_keys_checkbutton"));
        g_settings_bind (settings, KEY_STICKY_KEYS_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (settings, KEY_STICKY_KEYS_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);

        /* Bounce keys */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "bounce_keys_checkbutton"));
        g_settings_bind (settings, KEY_BOUNCE_KEYS_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (settings, KEY_BOUNCE_KEYS_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);

        /* Slow keys */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "slow_keys_checkbutton"));
        g_settings_bind (settings, KEY_SLOW_KEYS_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (settings, KEY_SLOW_KEYS_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);

        /* High contrast */
        widget = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     "high_contrast_checkbutton"));
        g_settings_bind_writable (dialog->priv->interface_settings, KEY_GTK_THEME,
                                  G_OBJECT (widget), "sensitive", TRUE);
        dialog->priv->high_contrast_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_high_contrast_checkbutton_toggled),
                          dialog);
        enabled = config_get_high_contrast (dialog);
        ui_set_high_contrast (dialog, enabled);

        /* On-screen keyboard */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "at_screen_keyboard_checkbutton"));
        g_settings_bind (dialog->priv->apps_settings, KEY_AT_SCREEN_KEYBOARD_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (dialog->priv->apps_settings, KEY_AT_SCREEN_KEYBOARD_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);
        gtk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("GSettings " KEYBOARD_A11Y_SCHEMA " " KEY_AT_SCREEN_KEYBOARD_ENABLED)) {
                gtk_widget_show_all (widget);
        } else {
                gtk_widget_hide (widget);
        }

        /* Screen reader */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "at_screen_reader_checkbutton"));
        g_settings_bind (dialog->priv->apps_settings, KEY_AT_SCREEN_READER_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (dialog->priv->apps_settings, KEY_AT_SCREEN_READER_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);
        gtk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("GSettings " KEYBOARD_A11Y_SCHEMA " " KEY_AT_SCREEN_READER_ENABLED)) {
                gtk_widget_show_all (widget);
        } else {
                gtk_widget_hide (widget);
        }

        /* Screen magnifier */
        widget = GTK_WIDGET (gtk_builder_get_object (builder, "at_screen_magnifier_checkbutton"));
        g_settings_bind (dialog->priv->apps_settings, KEY_AT_SCREEN_MAGNIFIER_ENABLED,
                         G_OBJECT (widget), "active", G_SETTINGS_BIND_DEFAULT);
        g_settings_bind_writable (dialog->priv->apps_settings, KEY_AT_SCREEN_MAGNIFIER_ENABLED,
                                  G_OBJECT (widget), "sensitive", TRUE);
        gtk_widget_set_no_show_all (widget, TRUE);
        if (config_have_at_gsettings_condition ("GSettings " KEYBOARD_A11Y_SCHEMA " " KEY_AT_SCREEN_MAGNIFIER_ENABLED)) {
                gtk_widget_show_all (widget);
        } else {
                gtk_widget_hide (widget);
        }

        /* Large print */
        widget = GTK_WIDGET (gtk_builder_get_object (builder,
                                                     "large_print_checkbutton"));
        dialog->priv->large_print_checkbutton = widget;
        g_signal_connect (widget,
                          "toggled",
                          G_CALLBACK (on_large_print_checkbutton_toggled),
                          dialog);
        enabled = config_get_large_print (dialog, &is_writable);
        ui_set_large_print (dialog, enabled);
        if (! is_writable) {
                gtk_widget_set_sensitive (widget, FALSE);
        }
}

static void
csd_a11y_preferences_dialog_init (CsdA11yPreferencesDialog *dialog)
{
        static const gchar *ui_file_path = GTKBUILDERDIR "/" GTKBUILDER_UI_FILE;
        gchar *objects[] = {"main_box", NULL};
        GError *error = NULL;
        GtkBuilder  *builder;

        dialog->priv = CSD_A11Y_PREFERENCES_DIALOG_GET_PRIVATE (dialog);

        builder = gtk_builder_new ();
        gtk_builder_set_translation_domain (builder, PACKAGE);
        if (gtk_builder_add_objects_from_file (builder, ui_file_path, objects,
                                               &error) == 0) {
                g_warning ("Could not load A11Y-UI: %s", error->message);
                g_error_free (error);
        } else {
                GtkWidget *widget;

                widget = GTK_WIDGET (gtk_builder_get_object (builder,
                                                             "main_box"));
                gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                                   widget);
                gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
                setup_dialog (dialog, builder);
       }

        g_object_unref (builder);

        gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
        gtk_window_set_title (GTK_WINDOW (dialog), _("Universal Access Preferences"));
        gtk_window_set_icon_name (GTK_WINDOW (dialog), "preferences-desktop-accessibility");
        g_object_set (dialog,
                      "resizable", FALSE,
                      NULL);

        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                NULL);
        g_signal_connect (dialog,
                          "response",
                          G_CALLBACK (on_response),
                          dialog);


        gtk_widget_show_all (GTK_WIDGET (dialog));
}

static void
csd_a11y_preferences_dialog_finalize (GObject *object)
{
        CsdA11yPreferencesDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_A11Y_PREFERENCES_DIALOG (object));

        dialog = CSD_A11Y_PREFERENCES_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        g_object_unref (dialog->priv->a11y_settings);
        g_object_unref (dialog->priv->interface_settings);
        g_object_unref (dialog->priv->apps_settings);

        G_OBJECT_CLASS (csd_a11y_preferences_dialog_parent_class)->finalize (object);
}

GtkWidget *
csd_a11y_preferences_dialog_new (void)
{
        GObject *object;

        object = g_object_new (CSD_TYPE_A11Y_PREFERENCES_DIALOG,
                               NULL);

        return GTK_WIDGET (object);
}
