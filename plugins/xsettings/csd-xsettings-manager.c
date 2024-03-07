/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Rodrigo Moya
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <X11/Xatom.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "cinnamon-settings-profile.h"
#include "csd-enums.h"
#include "csd-xsettings-manager.h"
#include "csd-xsettings-gtk.h"
#include "xsettings-manager.h"
#include "fontconfig-monitor.h"
#include "migrate-settings.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/cdesktop-enums.h>

#define CINNAMON_XSETTINGS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CINNAMON_TYPE_XSETTINGS_MANAGER, CinnamonSettingsXSettingsManagerPrivate))

#define MOUSE_SETTINGS_SCHEMA     "org.cinnamon.desktop.peripherals.mouse"
#define INTERFACE_SETTINGS_SCHEMA "org.cinnamon.desktop.interface"
#define INTERFACE_WM_SETTINGS_SCHEMA "org.cinnamon.desktop.wm.preferences"
#define SOUND_SETTINGS_SCHEMA     "org.cinnamon.desktop.sound"
#define PRIVACY_SETTINGS_SCHEMA   "org.cinnamon.desktop.privacy"
#define KEYBOARD_A11Y_SCHEMA      "org.cinnamon.desktop.a11y.keyboard"

#define XSETTINGS_PLUGIN_SCHEMA "org.cinnamon.settings-daemon.plugins.xsettings"
#define XSETTINGS_OVERRIDE_KEY  "overrides"

#define GTK_MODULES_DISABLED_KEY "disabled-gtk-modules"
#define GTK_MODULES_ENABLED_KEY  "enabled-gtk-modules"

#define TEXT_SCALING_FACTOR_KEY "text-scaling-factor"
#define SCALING_FACTOR_KEY "scaling-factor"
#define CURSOR_SIZE_KEY "cursor-size"
#define ANIMATIONS_KEY "enable-animations"

#define FONT_ANTIALIASING_KEY "antialiasing"
#define FONT_HINTING_KEY      "hinting"
#define FONT_RGBA_ORDER_KEY   "rgba-order"

/* As we cannot rely on the X server giving us good DPI information, and
 * that we don't want multi-monitor screens to have different DPIs (thus
 * different text sizes), we'll hard-code the value of the DPI
 *
 * See also:
 * https://bugzilla.novell.com/show_bug.cgi?id=217790•
 * https://bugzilla.gnome.org/show_bug.cgi?id=643704
 *
 * http://lists.fedoraproject.org/pipermail/devel/2011-October/157671.html
 * Why EDID is not trustworthy for DPI
 * Adam Jackson ajax at redhat.com
 * Tue Oct 4 17:54:57 UTC 2011
 * 
 *     Previous message: GNOME 3 - font point sizes now scaled?
 *     Next message: Why EDID is not trustworthy for DPI
 *     Messages sorted by: [ date ] [ thread ] [ subject ] [ author ]
 * 
 * On Tue, 2011-10-04 at 11:46 -0400, Kaleb S. KEITHLEY wrote:
 * 
 * > Grovelling around in the F15 xorg-server sources and reviewing the Xorg 
 * > log file on my F15 box, I see, with _modern hardware_ at least, that we 
 * > do have the monitor geometry available from DDC or EDIC, and obviously 
 * > it is trivial to compute the actual, correct DPI for each screen.
 * 
 * I am clearly going to have to explain this one more time, forever.
 * Let's see if I can't write it authoritatively once and simply answer
 * with a URL from here out.  (As always, use of the second person "you"
 * herein is plural, not singular.)
 * 
 * EDID does not reliably give you the size of the display.
 * 
 * Base EDID has at least two different places where you can give a
 * physical size (before considering extensions that aren't widely deployed
 * so whatever).  The first is a global property, measured in centimeters,
 * of the physical size of the glass.  The second is attached to your (zero
 * or more) detailed timing specifications, and reflects the size of the
 * mode, in millimeters.
 * 
 * So, how does this screw you?
 * 
 * a) Glass size is too coarse.  On a large display that cm roundoff isn't
 * a big deal, but on subnotebooks it's a different game.  The 11" MBA is
 * 25.68x14.44 cm, so that gives you a range of 52.54-54.64 dpcm horizontal
 * and 51.20-54.86 dpcm vertical (133.4-138.8 dpi h and 130.0-139.3 dpi v).
 * Which is optimistic, because that's doing the math forward from knowing
 * the actual size, and you as the EDID parser can't know which way the
 * manufacturer rounded.
 * 
 * b) Glass size need not be non-zero.  This is in fact the usual case for
 * projectors, which don't have a fixed display size since it's a function
 * of how far away the wall is from the lens.
 * 
 * c) Glass size could be partially non-zero.  Yes, really.  EDID 1.4
 * defines a method of using these two bytes to encode aspect ratio, where
 * if vertical size is 0 then the aspect ratio is computed as (horizontal
 * value + 99) / 100 in portrait mode (and the obvious reverse thing if
 * horizontal is zero).  Admittedly, unlike every other item in this list,
 * I've never seen this in the wild.  But it's legal.
 * 
 * d) Glass size could be a direct encoding of the aspect ratio.  Base EDID
 * doesn't condone this behaviour, but the CEA spec (to which all HDMI
 * monitors must conform) does allow-but-not-require it, which means your
 * 1920x1080 TV could claim to be 16 "cm" by 9 "cm".  So of course that's
 * what TV manufacturers do because that way they don't have to modify the
 * EDID info when physical construction changes, and that's cheaper.
 * 
 * e) You could use mode size to get size in millimeters, but you might not
 * have any detailed timings.
 * 
 * f) You could use mode size, but mode size is explicitly _not_ glass
 * size.  It's the size that the display chooses to present that mode.
 * Sometimes those are the same, and sometimes they're not.  You could be
 * scaled or {letter,pillar}boxed, and that's not necessarily something you
 * can control from the host side.
 * 
 * g) You could use mode size, but it could be an encoded aspect ratio, as
 * in case d above, because CEA says that's okay.
 * 
 * h) You could use mode size, but it could be the aspect ratio from case d
 * multiplied by 10 in each direction (because, of course, you gave size in
 * centimeters and so your authoring tool just multiplied it up).
 * 
 * i) Any or all of the above could be complete and utter garbage, because
 * - and I really, really need you to understand this - there is no
 * requirements program for any commercial OS or industry standard that
 * requires honesty here, as far as I'm aware.  There is every incentive
 * for there to _never_ be one, because it would make the manufacturing
 * process more expensive.
 * 
 * So from this point the suggestion is usually "well come up with some
 * heuristic to make a good guess assuming there's some correlation between
 * the various numbers you're given".  I have in fact written heuristics
 * for this, and they're in your kernel and your X server, and they still
 * encounter a huge number of cases where we simply _cannot_ know from EDID
 * anything like a physical size, because - to pick only one example - the
 * consumer electronics industry are cheap bastards, because you the
 * consumer demanded that they be cheap.
 * 
 * And then your only recourse is to an external database, and now you're
 * up the creek again because the identifying information here is a
 * vendor/model/serial tuple, and the vendor can and does change physical
 * construction without changing model number.  Now you get to play the
 * guessing game of how big the serial number range is for each subvariant,
 * assuming they bothered to encode a serial number - and they didn't.  Or,
 * if they bothered to encode week/year of manufacturer correctly - and
 * they didn't - which weeks meant which models.  And then you still have
 * to go out and buy one of every TV at Fry's, and that covers you for one
 * market, for three months.
 * 
 * If someone wants to write something better, please, by all means.  If
 * it's kernel code, send it to dri-devel at lists.freedesktop.org and cc me
 * and I will happily review it.  Likewise xorg-devel@ for X server
 * changes.
 * 
 * I gently suggest that doing so is a waste of time.
 * 
 * But if there's one thing free software has taught me, it's that you can
 * not tell people something is a bad idea and have any expectation they
 * will believe you.
 * 
 * > Obviously in a multi-screen set-up using Xinerama this has the potential 
 * > to be a Hard Problem if the monitors differ greatly in their DPI.
 * > 
 * > If the major resistance is over what to do with older hardware that 
 * > doesn't have this data available, then yes, punt; use a hard-coded 
 * > default. Likewise, if the two monitors really differ greatly, then punt.
 * 
 * I'm going to limit myself to observing that "greatly" is a matter of
 * opinion, and that in order to be really useful you'd need some way of
 * communicating "I punted" to the desktop.
 * 
 * Beyond that, sure, pick a heuristic, accept that it's going to be
 * insufficient for someone, and then sit back and wait to get
 * second-guessed on it over and over.
 * 
 * > And it wouldn't be so hard to to add something like -dpi:0, -dpi:1, 
 * > -dpi:2 command line options to specify per-screen dpi. I kinda thought I 
 * > did that a long, long time ago, but maybe I only thought about doing it 
 * > and never actually got around to it.
 * 
 * The RANDR extension as of version 1.2 does allow you to override
 * physical size on a per-output basis at runtime.  We even try pretty hard
 * to set them as honestly as we can up front.  The 96dpi thing people
 * complain about is from the per-screen info, which is simply a default
 * because of all the tl;dr above; because you have N outputs per screen
 * which means a single number is in general useless; and because there is
 * no way to refresh the per-screen info at runtime, as it's only ever sent
 * in the initial connection handshake.
 * 
 * - ajax
 * 
 */
#define DPI_FALLBACK 96

typedef struct _TranslationEntry TranslationEntry;
typedef void (* TranslationFunc) (CinnamonSettingsXSettingsManager *manager,
                                  TranslationEntry      *trans,
                                  GVariant              *value);

struct _TranslationEntry {
        const char     *gsettings_schema;
        const char     *gsettings_key;
        const char     *xsetting_name;

        TranslationFunc translate;
};

struct CinnamonSettingsXSettingsManagerPrivate
{
        guint              start_idle_id;
        XSettingsManager **managers;
        GHashTable        *settings;

        GSettings         *plugin_settings;
        fontconfig_monitor_handle_t *fontconfig_handle;

        CsdXSettingsGtk   *gtk;
        GDBusConnection   *dbus_connection;

        guint              cinnamon_properties_changed_id;
        guint              cinnamon_name_watch_id;
        gboolean           enable_animations;

        guint              display_config_watch_id;
        guint              monitors_changed_id;

        guint              notify_idle_id;
};

#define CSD_XSETTINGS_ERROR csd_xsettings_error_quark ()

enum {
        CSD_XSETTINGS_ERROR_INIT
};

static void     cinnamon_xsettings_manager_finalize    (GObject                  *object);
static void     animations_enabled_changed             (CinnamonSettingsXSettingsManager *manager);

G_DEFINE_TYPE (CinnamonSettingsXSettingsManager, cinnamon_xsettings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static GQuark
csd_xsettings_error_quark (void)
{
        return g_quark_from_static_string ("csd-xsettings-error-quark");
}

static void
set_session_bus_id (CinnamonSettingsXSettingsManager *manager)
{
        const gchar *id;
        GDBusConnection *bus;
        GVariant *res;

        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        res = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "GetId",
                                           NULL,
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           NULL);

        if (res) {
                g_variant_get (res, "(&s)", &id);

                int i;

                for (i = 0; manager->priv->managers[i]; i++) {
                        xsettings_manager_set_string (manager->priv->managers[i], "Gtk/SessionBusId", id);
                }

                g_variant_unref (res);
        }

        g_object_unref (bus);
}

static void
translate_bool_int (CinnamonSettingsXSettingsManager *manager,
                    TranslationEntry      *trans,
                    GVariant              *value)
{
        int i;

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_int (manager->priv->managers [i], trans->xsetting_name,
                                           g_variant_get_boolean (value));
        }
}

static void
translate_int_int (CinnamonSettingsXSettingsManager *manager,
                   TranslationEntry      *trans,
                   GVariant              *value)
{
        int i;

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_int (manager->priv->managers [i], trans->xsetting_name,
                                           g_variant_get_int32 (value));
        }
}

static void
translate_string_string (CinnamonSettingsXSettingsManager *manager,
                         TranslationEntry      *trans,
                         GVariant              *value)
{
        int i;

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_string (manager->priv->managers [i],
                                              trans->xsetting_name,
                                              g_variant_get_string (value, NULL));
        }
}

static void
translate_string_string_toolbar (CinnamonSettingsXSettingsManager *manager,
                                 TranslationEntry      *trans,
                                 GVariant              *value)
{
        int         i;
        const char *tmp;

        /* This is kind of a workaround since GNOME expects the key value to be
         * "both_horiz" and gtk+ wants the XSetting to be "both-horiz".
         */
        tmp = g_variant_get_string (value, NULL);
        if (tmp && strcmp (tmp, "both_horiz") == 0) {
                tmp = "both-horiz";
        }

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_string (manager->priv->managers [i],
                                              trans->xsetting_name,
                                              tmp);
        }
}

static void
translate_string_string_window_buttons (CinnamonSettingsXSettingsManager *manager,
                                 TranslationEntry      *trans,
                                 GVariant              *value)
{
        int         i;
        const char *tmp;
        gchar *ptr, *final_str;

        /* This is kind of a workaround. "menu" is useless in metacity titlebars
         * it duplicates the same features as the right-click menu.
         * In CSD windows on the hand it is required to show unique features.
         */

        tmp = g_variant_get_string (value, NULL);

        /* Check if menu is in the setting string already */
        ptr = g_strstr_len (tmp, -1, "menu");

        if (!ptr) {
            /* If it wasn't there already, we add it... */

            /* Simple cases, :* - all items on right, just prepend menu on left side*/
            if (g_str_has_prefix (tmp, ":")) {
                final_str = g_strdup_printf ("menu%s", tmp);
            }
            else
            /* All items on left... * (no :), append menu - we want actual window
               controls on the outside */
            if (!g_strstr_len (tmp, -1, ":")) {
                final_str = g_strdup_printf ("%s,menu", tmp);
            }
            else {
                /* Items on both sides, split it, append menu to the lefthand, and re-
                 * construct the string with the : separator */

                gchar **split = g_strsplit (tmp, ":", 2);

                final_str = g_strdup_printf ("%s,menu:%s", split[0], split[1]);

                g_strfreev (split);
            }
        } else {
            /* If menu was already included, just copy the original string */
            final_str = g_strdup (tmp);
        }

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_string (manager->priv->managers [i],
                                              trans->xsetting_name,
                                              final_str);
        }

        g_free (final_str);
}

static void
translate_enable_animations (CinnamonSettingsXSettingsManager *manager,
                             TranslationEntry                 *trans,
                             GVariant                         *value)
{
    animations_enabled_changed (manager);
}

static TranslationEntry translations [] = {

        { "org.cinnamon.settings-daemon.plugins.xsettings", "menus-have-icons", "Gtk/MenuImages",          translate_bool_int },
        { "org.cinnamon.settings-daemon.plugins.xsettings", "buttons-have-icons",     "Gtk/ButtonImages",        translate_bool_int },
        { "org.cinnamon.settings-daemon.plugins.xsettings", "show-input-method-menu", "Gtk/ShowInputMethodMenu", translate_bool_int },
        { "org.cinnamon.settings-daemon.plugins.xsettings", "show-unicode-menu",      "Gtk/ShowUnicodeMenu",     translate_bool_int },
        { "org.cinnamon.settings-daemon.plugins.xsettings", "automatic-mnemonics",    "Gtk/AutoMnemonics",       translate_bool_int },
        { "org.cinnamon.settings-daemon.plugins.xsettings", "dialogs-use-header",     "Gtk/DialogsUseHeader",    translate_bool_int },
        { "org.cinnamon.desktop.interface", "gtk-color-palette",      "Gtk/ColorPalette",        translate_string_string },
        { "org.cinnamon.desktop.interface", "font-name",              "Gtk/FontName",            translate_string_string },
        { "org.cinnamon.desktop.interface", "gtk-key-theme",          "Gtk/KeyThemeName",        translate_string_string },
        { "org.cinnamon.desktop.interface", "toolbar-style",          "Gtk/ToolbarStyle",        translate_string_string_toolbar },
        { "org.cinnamon.desktop.interface", "toolbar-icons-size",     "Gtk/ToolbarIconSize",     translate_string_string },
        { "org.cinnamon.desktop.interface", "can-change-accels",      "Gtk/CanChangeAccels",     translate_bool_int },
        { "org.cinnamon.desktop.interface", "cursor-blink-timeout",   "Gtk/CursorBlinkTimeout",  translate_int_int },
        { "org.cinnamon.desktop.interface", "gtk-timeout-initial",    "Gtk/TimeoutInitial",      translate_int_int },
        { "org.cinnamon.desktop.interface", "gtk-timeout-repeat",     "Gtk/TimeoutRepeat",       translate_int_int },
        { "org.cinnamon.desktop.interface", "gtk-color-scheme",       "Gtk/ColorScheme",         translate_string_string },
        { "org.cinnamon.desktop.interface", "gtk-im-preedit-style",   "Gtk/IMPreeditStyle",      translate_string_string },
        { "org.cinnamon.desktop.interface", "gtk-im-status-style",    "Gtk/IMStatusStyle",       translate_string_string },
        { "org.cinnamon.desktop.interface", "gtk-im-module",          "Gtk/IMModule",            translate_string_string },
        { "org.cinnamon.desktop.interface", "menubar-accel",          "Gtk/MenuBarAccel",        translate_string_string },
        { "org.cinnamon.desktop.interface", "enable-animations",      "Gtk/EnableAnimations",    translate_enable_animations },
        { "org.cinnamon.desktop.interface", "cursor-theme",           "Gtk/CursorThemeName",     translate_string_string },
        { "org.cinnamon.desktop.interface", "gtk-enable-primary-paste", "Gtk/EnablePrimaryPaste", translate_bool_int },
        { "org.cinnamon.desktop.interface", "gtk-overlay-scrollbars", "Gtk/OverlayScrolling",     translate_bool_int },
        { "org.cinnamon.desktop.wm.preferences", "button-layout",  "Gtk/DecorationLayout",    translate_string_string_window_buttons },
        { "org.cinnamon.desktop.wm.preferences", "action-double-click-titlebar",  "Gtk/TitlebarDoubleClick",    translate_string_string },
        { "org.cinnamon.desktop.wm.preferences", "action-middle-click-titlebar",  "Gtk/TitlebarMiddleClick",    translate_string_string },
        { "org.cinnamon.desktop.wm.preferences", "action-right-click-titlebar",  "Gtk/TitlebarRightClick",    translate_string_string },
        { "org.cinnamon.desktop.privacy", "recent-files-max-age", "Gtk/RecentFilesMaxAge", translate_int_int },
        { "org.cinnamon.desktop.privacy", "remember-recent-files", "Gtk/RecentFilesEnabled", translate_bool_int },
        { "org.cinnamon.desktop.a11y.keyboard", "always-show-text-caret",      "Gtk/KeynavUseCaret",      translate_bool_int },

        { "org.cinnamon.desktop.peripherals.mouse", "double-click",   "Net/DoubleClickTime",  translate_int_int },
        { "org.cinnamon.desktop.peripherals.mouse", "drag-threshold", "Net/DndDragThreshold", translate_int_int },
        { "org.cinnamon.desktop.interface", "cursor-blink",           "Net/CursorBlink",         translate_bool_int },
        { "org.cinnamon.desktop.interface", "cursor-blink-time",      "Net/CursorBlinkTime",     translate_int_int },
        { "org.cinnamon.desktop.interface", "gtk-theme",              "Net/ThemeName",           translate_string_string },
        { "org.cinnamon.desktop.interface", "icon-theme",             "Net/IconThemeName",       translate_string_string },
        { "org.cinnamon.desktop.sound", "theme-name",                 "Net/SoundThemeName",            translate_string_string },
        { "org.cinnamon.desktop.sound", "event-sounds",               "Net/EnableEventSounds" ,        translate_bool_int },
        { "org.cinnamon.desktop.sound", "input-feedback-sounds",      "Net/EnableInputFeedbackSounds", translate_bool_int }
};

static gboolean
notify_idle (gpointer data)
{
        CinnamonSettingsXSettingsManager *manager = data;
        gint i;
        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_notify (manager->priv->managers[i]);
        }
        manager->priv->notify_idle_id = 0;
        return G_SOURCE_REMOVE;
}

static void
queue_notify (CinnamonSettingsXSettingsManager *manager)
{
        if (manager->priv->notify_idle_id != 0)
                return;

        manager->priv->notify_idle_id = g_idle_add (notify_idle, manager);
}

static double
get_dpi_from_gsettings (CinnamonSettingsXSettingsManager *manager)
{
	GSettings  *interface_settings;
        double      dpi;
        double      factor;

	interface_settings = g_hash_table_lookup (manager->priv->settings, INTERFACE_SETTINGS_SCHEMA);
        factor = g_settings_get_double (interface_settings, TEXT_SCALING_FACTOR_KEY);

	dpi = DPI_FALLBACK;

        return dpi * factor;
}


static gboolean
get_legacy_ui_scale (GVariantIter *properties,
                     int          *scale)
{
        const char *key;
        GVariant *value;

        *scale = 0;

        while (g_variant_iter_loop (properties, "{&sv}", &key, &value)) {
                if (!g_str_equal (key, "legacy-ui-scaling-factor"))
                        continue;

                *scale = g_variant_get_int32 (value);
                break;
        }

        if (*scale < 1) {
                g_warning ("Failed to get current UI legacy scaling factor");
                *scale = 1;
                return FALSE;
        }

        return TRUE;
}

#define MODE_FORMAT "(siiddada{sv})"
#define MODES_FORMAT "a" MODE_FORMAT

#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_FORMAT "(iiduba" MONITOR_SPEC_FORMAT "a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

#define CURRENT_STATE_FORMAT "(u" MONITORS_FORMAT LOGICAL_MONITORS_FORMAT "a{sv})"

static int
get_window_scale (CinnamonSettingsXSettingsManager *manager)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) current_state = NULL;
        g_autoptr(GVariantIter) properties = NULL;
        int scale = 1;

        current_state =
                g_dbus_connection_call_sync (manager->priv->dbus_connection,
                                             "org.cinnamon.Muffin.DisplayConfig",
                                             "/org/cinnamon/Muffin/DisplayConfig",
                                             "org.cinnamon.Muffin.DisplayConfig",
                                             "GetCurrentState",
                                             NULL,
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                             -1,
                                             NULL,
                                             &error);
        if (!current_state) {
                g_debug ("Failed to get current display configuration state: %s",
                           error->message);
                return 1;
        }

        g_variant_get (current_state,
                       CURRENT_STATE_FORMAT,
                       NULL,
                       NULL,
                       NULL,
                       &properties);

        if (!get_legacy_ui_scale (properties, &scale))
                g_warning ("Failed to get current UI legacy scaling factor");

        return scale;
}

typedef struct {
        gboolean    antialias;
        gboolean    hinting;
        int         scaled_dpi;
        int         dpi;
        int         window_scale;
        int         cursor_size;
        const char *rgba;
        const char *hintstyle;
} CinnamonSettingsXftSettings;

/* Read GSettings and determine the appropriate Xft settings based on them. */
static void
xft_settings_get (CinnamonSettingsXSettingsManager *manager,
                  CinnamonSettingsXftSettings      *settings)
{
        GSettings  *interface_settings;
        CsdFontAntialiasingMode antialiasing;
        CsdFontHinting hinting;
        CsdFontRgbaOrder order;
        gboolean use_rgba = FALSE;
        double dpi;
        int cursor_size;

        interface_settings = g_hash_table_lookup (manager->priv->settings, INTERFACE_SETTINGS_SCHEMA);

        antialiasing = g_settings_get_enum (manager->priv->plugin_settings, FONT_ANTIALIASING_KEY);
        hinting = g_settings_get_enum (manager->priv->plugin_settings, FONT_HINTING_KEY);
        order = g_settings_get_enum (manager->priv->plugin_settings, FONT_RGBA_ORDER_KEY);

        settings->antialias = (antialiasing != CSD_FONT_ANTIALIASING_MODE_NONE);
        settings->hinting = (hinting != CSD_FONT_HINTING_NONE);

        settings->window_scale = get_window_scale (manager);
        dpi = get_dpi_from_gsettings (manager);
        settings->dpi = dpi * 1024; /* Xft wants 1/1024ths of an inch */
        settings->scaled_dpi = dpi * settings->window_scale * 1024;
        cursor_size = g_settings_get_int (interface_settings, CURSOR_SIZE_KEY);
        settings->cursor_size = cursor_size * settings->window_scale;

        settings->rgba = "rgb";
        settings->hintstyle = "hintfull";

        switch (hinting) {
        case CSD_FONT_HINTING_NONE:
                settings->hintstyle = "hintnone";
                break;
        case CSD_FONT_HINTING_SLIGHT:
                settings->hintstyle = "hintslight";
                break;
        case CSD_FONT_HINTING_MEDIUM:
                settings->hintstyle = "hintmedium";
                break;
        case CSD_FONT_HINTING_FULL:
                settings->hintstyle = "hintfull";
                break;
        }

        switch (order) {
        case CSD_FONT_RGBA_ORDER_RGBA:
                settings->rgba = "rgba";
                break;
        case CSD_FONT_RGBA_ORDER_RGB:
                settings->rgba = "rgb";
                break;
        case CSD_FONT_RGBA_ORDER_BGR:
                settings->rgba = "bgr";
                break;
        case CSD_FONT_RGBA_ORDER_VRGB:
                settings->rgba = "vrgb";
                break;
        case CSD_FONT_RGBA_ORDER_VBGR:
                settings->rgba = "vbgr";
                break;
        }

        switch (antialiasing) {
        case CSD_FONT_ANTIALIASING_MODE_NONE:
                settings->antialias = 0;
                break;
        case CSD_FONT_ANTIALIASING_MODE_GRAYSCALE:
                settings->antialias = 1;
                break;
        case CSD_FONT_ANTIALIASING_MODE_RGBA:
                settings->antialias = 1;
                use_rgba = TRUE;
        }

        if (!use_rgba) {
                settings->rgba = "none";
        }
}

static void
xft_settings_set_xsettings (CinnamonSettingsXSettingsManager *manager,
                            CinnamonSettingsXftSettings      *settings)
{
        int i;

        cinnamon_settings_profile_start (NULL);

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_int (manager->priv->managers [i], "Xft/Antialias", settings->antialias);
                xsettings_manager_set_int (manager->priv->managers [i], "Xft/Hinting", settings->hinting);
                xsettings_manager_set_string (manager->priv->managers [i], "Xft/HintStyle", settings->hintstyle);
                xsettings_manager_set_int (manager->priv->managers [i], "Gdk/WindowScalingFactor", settings->window_scale);
                xsettings_manager_set_int (manager->priv->managers [i], "Gdk/UnscaledDPI", settings->dpi);
                xsettings_manager_set_int (manager->priv->managers [i], "Xft/DPI", settings->scaled_dpi);
                xsettings_manager_set_string (manager->priv->managers [i], "Xft/RGBA", settings->rgba);
                xsettings_manager_set_int (manager->priv->managers [i], "Gtk/CursorThemeSize", settings->cursor_size);
                xsettings_manager_set_int (manager->priv->managers [i], "Gtk/EnableAnimations", manager->priv->enable_animations);
        }
        cinnamon_settings_profile_end (NULL);
}

static void
update_property (GString *props, const gchar* key, const gchar* value)
{
        gchar* needle;
        size_t needle_len;
        gchar* found = NULL;

        /* update an existing property */
        needle = g_strconcat (key, ":", NULL);
        needle_len = strlen (needle);
        if (g_str_has_prefix (props->str, needle))
                found = props->str;
        else 
            found = strstr (props->str, needle);

        if (found) {
                size_t value_index;
                gchar* end;

                end = strchr (found, '\n');
                value_index = (found - props->str) + needle_len + 1;
                g_string_erase (props, value_index, end ? (end - found - needle_len) : -1);
                g_string_insert (props, value_index, "\n");
                g_string_insert (props, value_index, value);
        } else {
                g_string_append_printf (props, "%s:\t%s\n", key, value);
        }

	g_free (needle);
}

static void
xft_settings_set_xresources (CinnamonSettingsXftSettings *settings)
{
        GString    *add_string;
        char        dpibuf[G_ASCII_DTOSTR_BUF_SIZE];
        Display    *dpy;

        cinnamon_settings_profile_start (NULL);

        /* get existing properties */
        dpy = XOpenDisplay (NULL);
        g_return_if_fail (dpy != NULL);
        add_string = g_string_new (XResourceManagerString (dpy));

        g_debug("xft_settings_set_xresources: orig res '%s'", add_string->str);

        g_snprintf (dpibuf, sizeof (dpibuf), "%d", (int) (settings->scaled_dpi / 1024.0 + 0.5));
        update_property (add_string, "Xft.dpi", dpibuf);
        update_property (add_string, "Xft.antialias",
                                settings->antialias ? "1" : "0");
        update_property (add_string, "Xft.hinting",
                                settings->hinting ? "1" : "0");
        update_property (add_string, "Xft.hintstyle",
                                settings->hintstyle);
        update_property (add_string, "Xft.rgba",
                                settings->rgba);

        g_debug("xft_settings_set_xresources: new res '%s'", add_string->str);

        /* Set the new X property */
        XChangeProperty(dpy, RootWindow (dpy, 0),
                        XA_RESOURCE_MANAGER, XA_STRING, 8, PropModeReplace, (const unsigned char *) add_string->str, add_string->len);
        XCloseDisplay (dpy);

        g_string_free (add_string, TRUE);

        cinnamon_settings_profile_end (NULL);
}

/* We mirror the Xft properties both through XSETTINGS and through
 * X resources
 */
static void
update_xft_settings (CinnamonSettingsXSettingsManager *manager)
{
        CinnamonSettingsXftSettings settings;

        cinnamon_settings_profile_start (NULL);

        xft_settings_get (manager, &settings);
        xft_settings_set_xsettings (manager, &settings);
        xft_settings_set_xresources (&settings);

        cinnamon_settings_profile_end (NULL);
}

static void
xft_callback (GSettings             *settings,
              const gchar           *key,
              CinnamonSettingsXSettingsManager *manager)
{
        update_xft_settings (manager);
        queue_notify (manager);
}

static void
size_changed_callback (GdkScreen *screen, CinnamonSettingsXSettingsManager *manager)
{
    update_xft_settings (manager);
    queue_notify (manager);
}

static void
override_callback (GSettings             *settings,
                   const gchar           *key,
                   CinnamonSettingsXSettingsManager *manager)
{
        GVariant *value;
        int i;

        value = g_settings_get_value (settings, XSETTINGS_OVERRIDE_KEY);

        for (i = 0; manager->priv->managers[i]; i++) {
                xsettings_manager_set_overrides (manager->priv->managers[i], value);
        }
        queue_notify (manager);

        g_variant_unref (value);
}

static void
plugin_callback (GSettings             *settings,
                 const char            *key,
                 CinnamonSettingsXSettingsManager *manager)
{
        if (g_str_equal (key, GTK_MODULES_DISABLED_KEY) ||
            g_str_equal (key, GTK_MODULES_ENABLED_KEY)) {
                /* Do nothing, as CsdXsettingsGtk will handle it */
        } else if (g_str_equal (key, XSETTINGS_OVERRIDE_KEY)) {
                override_callback (settings, key, manager);
        } else {
                xft_callback (settings, key, manager);
        }
}

static void
gtk_modules_callback (CsdXSettingsGtk       *gtk,
                      GParamSpec            *spec,
                      CinnamonSettingsXSettingsManager *manager)
{
        const char *modules = csd_xsettings_gtk_get_modules (manager->priv->gtk);
        int i;

        if (modules == NULL) {
                for (i = 0; manager->priv->managers [i]; ++i) {
                        xsettings_manager_delete_setting (manager->priv->managers [i], "Gtk/Modules");
                }
        } else {
                g_debug ("Setting GTK modules '%s'", modules);
                for (i = 0; manager->priv->managers [i]; ++i) {
                        xsettings_manager_set_string (manager->priv->managers [i],
                                                      "Gtk/Modules",
                                                      modules);
                }
        }

        queue_notify (manager);
}

static void
fontconfig_callback (fontconfig_monitor_handle_t *handle,
                     CinnamonSettingsXSettingsManager       *manager)
{
        int i;
        int timestamp = time (NULL);

        cinnamon_settings_profile_start (NULL);

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_int (manager->priv->managers [i], "Fontconfig/Timestamp", timestamp);
        }
        queue_notify (manager);
        cinnamon_settings_profile_end (NULL);
}

static gboolean
start_fontconfig_monitor_idle_cb (CinnamonSettingsXSettingsManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        manager->priv->fontconfig_handle = fontconfig_monitor_start ((GFunc) fontconfig_callback, manager);

        cinnamon_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

static void
start_fontconfig_monitor (CinnamonSettingsXSettingsManager  *manager)
{
        cinnamon_settings_profile_start (NULL);

        fontconfig_cache_init ();

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) start_fontconfig_monitor_idle_cb, manager);

        cinnamon_settings_profile_end (NULL);
}

static void
stop_fontconfig_monitor (CinnamonSettingsXSettingsManager  *manager)
{
        if (manager->priv->fontconfig_handle) {
                fontconfig_monitor_stop (manager->priv->fontconfig_handle);
                manager->priv->fontconfig_handle = NULL;
        }
}

static void
process_value (CinnamonSettingsXSettingsManager *manager,
               TranslationEntry      *trans,
               GVariant              *value)
{
        (* trans->translate) (manager, trans, value);
}

static TranslationEntry *
find_translation_entry (GSettings *settings, const char *key)
{
        guint i;
        char *schema;

        g_object_get (settings, "schema-id", &schema, NULL);

        for (i = 0; i < G_N_ELEMENTS (translations); i++) {
                if (g_str_equal (schema, translations[i].gsettings_schema) &&
                    g_str_equal (key, translations[i].gsettings_key)) {
                            g_free (schema);
                        return &translations[i];
                }
        }

        g_free (schema);

        return NULL;
}

static void
xsettings_callback (GSettings             *settings,
                    const char            *key,
                    CinnamonSettingsXSettingsManager *manager)
{
        TranslationEntry *trans;
        guint             i;
        GVariant         *value;

        if (g_str_equal (key, TEXT_SCALING_FACTOR_KEY) ||
            g_str_equal (key, SCALING_FACTOR_KEY) ||
            g_str_equal (key, CURSOR_SIZE_KEY)) {
            xft_callback (NULL, key, manager);
            return;
	}

        trans = find_translation_entry (settings, key);
        if (trans == NULL) {
                return;
        }

        value = g_settings_get_value (settings, key);

        process_value (manager, trans, value);

        g_variant_unref (value);

        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_string (manager->priv->managers [i],
                                              "Net/FallbackIconTheme",
                                              "gnome");
        }
        queue_notify (manager);
}

static void
terminate_cb (void *data)
{
        gboolean *terminated = data;

        if (*terminated) {
                return;
        }

        *terminated = TRUE;
        g_warning ("X Settings Manager is terminating");
        gtk_main_quit ();
}

static gboolean
setup_xsettings_managers (CinnamonSettingsXSettingsManager *manager)
{
        GdkDisplay *display;
        int         i;
        int         n_screens;
        gboolean    res;
        gboolean    terminated;

        display = gdk_display_get_default ();
        n_screens = gdk_display_get_n_screens (display);

        res = xsettings_manager_check_running (gdk_x11_display_get_xdisplay (display),
                                               gdk_screen_get_number (gdk_screen_get_default ()));

        if (res) {
                g_warning ("You can only run one xsettings manager at a time; exiting");
                return FALSE;
        }

        manager->priv->managers = g_new0 (XSettingsManager *, n_screens + 1);

        terminated = FALSE;
        for (i = 0; i < n_screens; i++) {
                GdkScreen *screen;

                screen = gdk_display_get_screen (display, i);

                manager->priv->managers [i] = xsettings_manager_new (gdk_x11_display_get_xdisplay (display),
                                                                     gdk_screen_get_number (screen),
                                                                     terminate_cb,
                                                                     &terminated);
                if (! manager->priv->managers [i]) {
                        g_warning ("Could not create xsettings manager for screen %d!", i);
                        return FALSE;
                }
                g_signal_connect (screen, "size-changed", G_CALLBACK (size_changed_callback), manager);
        }

        return TRUE;
}

static void
monitors_changed (CinnamonSettingsXSettingsManager *manager)
{
        update_xft_settings (manager);
        queue_notify (manager);
}

static void
on_monitors_changed (GDBusConnection *connection,
                     const gchar     *sender_name,
                     const gchar     *object_path,
                     const gchar     *interface_name,
                     const gchar     *signal_name,
                     GVariant        *parameters,
                     gpointer         data)
{
        CinnamonSettingsXSettingsManager *manager = data;
        monitors_changed (manager);
}

static void
on_display_config_name_appeared_handler (GDBusConnection *connection,
                                         const gchar     *name,
                                         const gchar     *name_owner,
                                         gpointer         data)
{
        CinnamonSettingsXSettingsManager *manager = data;
        monitors_changed (manager);
}

static void
animations_enabled_changed (CinnamonSettingsXSettingsManager *manager)
{
        GSettings *settings;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) res = NULL;
        g_autoptr(GVariant) animations_enabled_variant = NULL;
        gboolean animations_enabled;
        gint i;

        // If the interface settings key is turned off, don't bother trying to check with cinnamon -
        // the user must have disabled it - this affects only Gtk programs. Otherwise, let Cinnamon
        // decide (based on current conditions and cinnamon-specific settings).

        settings = g_hash_table_lookup (manager->priv->settings, INTERFACE_SETTINGS_SCHEMA);
        animations_enabled = g_settings_get_boolean (settings, "enable-animations");

        if (animations_enabled) {
            res = g_dbus_connection_call_sync (manager->priv->dbus_connection,
                                               "org.Cinnamon",
                                               "/org/Cinnamon",
                                               "org.freedesktop.DBus.Properties",
                                               "Get",
                                               g_variant_new ("(ss)",
                                                              "org.Cinnamon",
                                                              "AnimationsEnabled"),
                                               NULL,
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
            if (!res) {
                    g_debug ("Failed to get AnimationsEnabled state from Cinnamon: %s",
                               error->message);
            } else {
                g_variant_get (res, "(v)", &animations_enabled_variant);
                g_variant_get (animations_enabled_variant, "b", &animations_enabled);
            }
        }

        if (manager->priv->enable_animations == animations_enabled)
                return;

        manager->priv->enable_animations = animations_enabled;

        for (i = 0; manager->priv->managers [i]; ++i) {
            xsettings_manager_set_int (manager->priv->managers [i], "Gtk/EnableAnimations", manager->priv->enable_animations);
        }

        queue_notify (manager);
}

static void
on_cinnamon_properties_changed (GDBusConnection *connection,
                                  const gchar     *sender_name,
                                  const gchar     *object_path,
                                  const gchar     *interface_name,
                                  const gchar     *signal_name,
                                  GVariant        *parameters,
                                  gpointer         data)
{
        CinnamonSettingsXSettingsManager *manager = data;
        animations_enabled_changed (manager);
}

static void
on_cinnamon_name_appeared_handler (GDBusConnection *connection,
                                   const gchar     *name,
                                   const gchar     *name_owner,
                                   gpointer         data)
{
        CinnamonSettingsXSettingsManager *manager = data;
        animations_enabled_changed (manager);
}

gboolean
cinnamon_xsettings_manager_start (CinnamonSettingsXSettingsManager *manager,
                               GError               **error)
{
        GVariant    *overrides;
        guint        i;
        GList       *list, *l;

        g_debug ("Starting xsettings manager");
        cinnamon_settings_profile_start (NULL);

        if (!setup_xsettings_managers (manager)) {
                g_set_error (error, CSD_XSETTINGS_ERROR,
                             CSD_XSETTINGS_ERROR_INIT,
                             "Could not initialize xsettings manager.");
                return FALSE;
        }

        manager->priv->monitors_changed_id =
                g_dbus_connection_signal_subscribe (manager->priv->dbus_connection,
                                                    "org.cinnamon.Muffin.DisplayConfig",
                                                    "org.cinnamon.Muffin.DisplayConfig",
                                                    "MonitorsChanged",
                                                    "/org/cinnamon/Muffin/DisplayConfig",
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                    on_monitors_changed,
                                                    manager,
                                                    NULL);
        manager->priv->display_config_watch_id =
                g_bus_watch_name_on_connection (manager->priv->dbus_connection,
                                                "org.cinnamon.Muffin.DisplayConfig",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                on_display_config_name_appeared_handler,
                                                NULL,
                                                manager,
                                                NULL);

        manager->priv->cinnamon_properties_changed_id =
                g_dbus_connection_signal_subscribe (manager->priv->dbus_connection,
                                                    "org.Cinnamon",
                                                    "org.freedesktop.DBus.Properties",
                                                    "PropertiesChanged",
                                                    "/org/Cinnamon",
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                    on_cinnamon_properties_changed,
                                                    manager,
                                                    NULL);
        manager->priv->cinnamon_name_watch_id =
                g_bus_watch_name_on_connection (manager->priv->dbus_connection,
                                                "org.Cinnamon",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                on_cinnamon_name_appeared_handler,
                                                NULL,
                                                manager,
                                                NULL);



        manager->priv->settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                         NULL, (GDestroyNotify) g_object_unref);

        g_hash_table_insert (manager->priv->settings,
                             MOUSE_SETTINGS_SCHEMA, g_settings_new (MOUSE_SETTINGS_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             INTERFACE_SETTINGS_SCHEMA, g_settings_new (INTERFACE_SETTINGS_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             INTERFACE_WM_SETTINGS_SCHEMA, g_settings_new (INTERFACE_WM_SETTINGS_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             SOUND_SETTINGS_SCHEMA, g_settings_new (SOUND_SETTINGS_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             XSETTINGS_PLUGIN_SCHEMA, g_settings_new (XSETTINGS_PLUGIN_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             PRIVACY_SETTINGS_SCHEMA, g_settings_new (PRIVACY_SETTINGS_SCHEMA));
        g_hash_table_insert (manager->priv->settings,
                             KEYBOARD_A11Y_SCHEMA, g_settings_new (KEYBOARD_A11Y_SCHEMA));

        for (i = 0; i < G_N_ELEMENTS (translations); i++) {
                GVariant *val;
                GSettings *settings;

                settings = g_hash_table_lookup (manager->priv->settings,
                                                translations[i].gsettings_schema);
                if (settings == NULL) {
                        g_warning ("Schemas '%s' has not been setup", translations[i].gsettings_schema);
                        continue;
                }

                val = g_settings_get_value (settings, translations[i].gsettings_key);

                process_value (manager, &translations[i], val);
                g_variant_unref (val);
        }

        list = g_hash_table_get_values (manager->priv->settings);
        for (l = list; l != NULL; l = l->next) {
                g_signal_connect_object (G_OBJECT (l->data), "changed", G_CALLBACK (xsettings_callback), manager, 0);
        }
        g_list_free (list);

        /* Plugin settings (GTK modules and Xft) */
        manager->priv->plugin_settings = g_settings_new (XSETTINGS_PLUGIN_SCHEMA);
        g_signal_connect_object (manager->priv->plugin_settings, "changed", G_CALLBACK (plugin_callback), manager, 0);

        manager->priv->gtk = csd_xsettings_gtk_new ();
        g_signal_connect (G_OBJECT (manager->priv->gtk), "notify::gtk-modules",
                          G_CALLBACK (gtk_modules_callback), manager);
        gtk_modules_callback (manager->priv->gtk, NULL, manager);

        /* Xft settings */
        update_xft_settings (manager);
        set_session_bus_id (manager);

        start_fontconfig_monitor (manager);

        overrides = g_settings_get_value (manager->priv->plugin_settings, XSETTINGS_OVERRIDE_KEY);
        for (i = 0; manager->priv->managers [i]; i++) {
                xsettings_manager_set_string (manager->priv->managers [i],
                                              "Net/FallbackIconTheme",
                                              "gnome");

                xsettings_manager_set_overrides (manager->priv->managers [i], overrides);

                xsettings_manager_set_int (manager->priv->managers [i], "Gtk/ShellShowsAppMenu",
                                           FALSE);

                xsettings_manager_set_int (manager->priv->managers [i], "Gtk/ShellShowsMenubar",
                                           FALSE);
        }

        queue_notify (manager);
        g_variant_unref (overrides);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
cinnamon_xsettings_manager_stop (CinnamonSettingsXSettingsManager *manager)
{
        CinnamonSettingsXSettingsManagerPrivate *p = manager->priv;
        int i;

        g_debug ("Stopping xsettings manager");

        if (manager->priv->cinnamon_properties_changed_id) {
                g_dbus_connection_signal_unsubscribe (manager->priv->dbus_connection,
                                                      manager->priv->cinnamon_properties_changed_id);
                manager->priv->cinnamon_properties_changed_id = 0;
        }

        if (manager->priv->cinnamon_name_watch_id) {
                g_bus_unwatch_name (manager->priv->cinnamon_name_watch_id);
                manager->priv->cinnamon_name_watch_id = 0;
        }

        if (manager->priv->monitors_changed_id) {
                g_dbus_connection_signal_unsubscribe (manager->priv->dbus_connection,
                                                      manager->priv->monitors_changed_id);
                manager->priv->monitors_changed_id = 0;
        }

        if (manager->priv->display_config_watch_id) {
                g_bus_unwatch_name (manager->priv->display_config_watch_id);
                manager->priv->display_config_watch_id = 0;
        }

        if (p->managers != NULL) {
                for (i = 0; p->managers [i]; ++i)
                        xsettings_manager_destroy (p->managers [i]);

                g_free (p->managers);
                p->managers = NULL;
        }

        if (p->plugin_settings != NULL) {
                g_object_unref (p->plugin_settings);
                p->plugin_settings = NULL;
        }

        stop_fontconfig_monitor (manager);

        if (p->settings != NULL) {
                g_hash_table_destroy (p->settings);
                p->settings = NULL;
        }

        if (p->gtk != NULL) {
                g_object_unref (p->gtk);
                p->gtk = NULL;
        }
}

static GObject *
cinnamon_xsettings_manager_constructor (GType                  type,
                                     guint                  n_construct_properties,
                                     GObjectConstructParam *construct_properties)
{
        CinnamonSettingsXSettingsManager      *xsettings_manager;

        xsettings_manager = CINNAMON_XSETTINGS_MANAGER (G_OBJECT_CLASS (cinnamon_xsettings_manager_parent_class)->constructor (type,
                                                                                                                  n_construct_properties,
                                                                                                                  construct_properties));

        return G_OBJECT (xsettings_manager);
}

static void
cinnamon_xsettings_manager_class_init (CinnamonSettingsXSettingsManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = cinnamon_xsettings_manager_constructor;
        object_class->finalize = cinnamon_xsettings_manager_finalize;

        g_type_class_add_private (klass, sizeof (CinnamonSettingsXSettingsManagerPrivate));
}

static void
cinnamon_xsettings_manager_init (CinnamonSettingsXSettingsManager *manager)
{
        GError *error;
        manager->priv = CINNAMON_XSETTINGS_MANAGER_GET_PRIVATE (manager);

        error = NULL;
        manager->priv->dbus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                                         NULL, &error);
        if (!manager->priv->dbus_connection) {
                g_error ("Failed to get session bus: %s", error->message);
        }
}

static void
cinnamon_xsettings_manager_finalize (GObject *object)
{
        CinnamonSettingsXSettingsManager *xsettings_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CINNAMON_IS_XSETTINGS_MANAGER (object));

        xsettings_manager = CINNAMON_XSETTINGS_MANAGER (object);

        g_return_if_fail (xsettings_manager->priv != NULL);

        if (xsettings_manager->priv->start_idle_id != 0) {
                g_source_remove (xsettings_manager->priv->start_idle_id);
                xsettings_manager->priv->start_idle_id = 0;
        }

        g_clear_object (&xsettings_manager->priv->dbus_connection);

        G_OBJECT_CLASS (cinnamon_xsettings_manager_parent_class)->finalize (object);
}

static GVariant *
map_speed (GVariant *variant,
           GSettings *origin_settings, GSettings *dest_settings,
           GVariant  *old_default,      GVariant *new_default)

{
        gdouble value;

        value = g_variant_get_double (variant);

        /* Remap from [0..10] to [-1..1] */
        value = (value / 5) - 1;

        return g_variant_new_double (value);
}

static GVariant *
map_send_events (GVariant *variant,
                 GSettings *origin_settings, GSettings *dest_settings,
                 GVariant  *old_default,      GVariant *new_default)
{
        gboolean enabled;

        enabled = g_settings_get_boolean (origin_settings, "touchpad-enabled");

        if (enabled) {
                if (g_settings_get_boolean (origin_settings, "disable-with-external-mouse")) {
                        g_settings_set_enum (dest_settings, "send-events", C_DESKTOP_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE);
                } else {
                        g_settings_set_enum (dest_settings, "send-events", C_DESKTOP_DEVICE_SEND_EVENTS_ENABLED);
                }
        } else {
                g_settings_set_enum (dest_settings, "send-events", C_DESKTOP_DEVICE_SEND_EVENTS_DISABLED);
        }

        return NULL;
}

static GVariant *
map_scroll_method (GVariant *variant,
                   GSettings *origin_settings, GSettings *dest_settings,
                   GVariant  *old_default,      GVariant *new_default)
{
        gint old;

        old = g_settings_get_int (origin_settings, "scrolling-method");

        switch (old) {
            case 0: // disabled
                g_settings_set_boolean (dest_settings, "edge-scrolling-enabled", FALSE);
                g_settings_set_boolean (dest_settings, "two-finger-scrolling-enabled", FALSE);
                break;
            case 2: // edge
                g_settings_set_boolean (dest_settings, "edge-scrolling-enabled", TRUE);
                g_settings_set_boolean (dest_settings, "two-finger-scrolling-enabled", FALSE);
                break;
            case 1: // two-finger
            case 3: // auto, also the new default
            default:
                g_settings_set_boolean (dest_settings, "edge-scrolling-enabled", FALSE);
                g_settings_set_boolean (dest_settings, "two-finger-scrolling-enabled", TRUE);
                break;
        }

        return NULL;
}

static GVariant *
map_click_method (GVariant *variant,
                  GSettings *origin_settings, GSettings *dest_settings,
                  GVariant  *old_default,      GVariant *new_default)
{
        gint old;
        old = g_settings_get_int (origin_settings, "clickpad-click");

        switch (old) {
            case 0: // disabled
                g_settings_set_enum (dest_settings, "click-method", C_DESKTOP_TOUCHPAD_CLICK_METHOD_NONE);
                break;
            case 1: // corners
                g_settings_set_enum (dest_settings, "click-method", C_DESKTOP_TOUCHPAD_CLICK_METHOD_AREAS);
                break;
            case 2: // fingers
                g_settings_set_enum (dest_settings, "click-method", C_DESKTOP_TOUCHPAD_CLICK_METHOD_FINGERS);
                break;
            case 3: // automatic
                g_settings_set_enum (dest_settings, "click-method", C_DESKTOP_TOUCHPAD_CLICK_METHOD_DEFAULT);
                break;
        }

        return NULL;
}

static void
migrate_mouse_settings (void)
{
        CsdSettingsMigrateEntry trackball_entries[] = {
                { "scroll-wheel-emulation-button", "scroll-wheel-emulation-button", NULL }
        };
        CsdSettingsMigrateEntry mouse_entries[] = {
                { "locate-pointer",        "locate-pointer", NULL },
                { "left-handed",           "left-handed",    NULL },
                { "natural-scroll",        "natural-scroll", NULL },
                { "custom-acceleration",   NULL,             NULL },
                { "motion-acceleration",   "speed",          map_speed },
                { "custom-threshold",      NULL,             NULL },
                { "motion-threshold",      NULL,             NULL },
                { "double-click",          "double-click",   NULL },
                { "drag-threshold",        "drag-threshold", NULL },
                { "middle-button-enabled", "middle-click-emulation", NULL },
        };

        CsdSettingsMigrateEntry touchpad_entries[] = {
                { "disable-while-typing", "disable-while-typing", NULL },
                { "scrolling-method",     "edge-scrolling-enabled", map_scroll_method },
                { "tap-to-click",         "tap-to-click",    NULL },
                { "clickpad-click",       "click-method",    map_click_method },
                { "touchpad-enabled",     "send-events",     map_send_events },
                { "disable-with-external-mouse", "send-events", map_send_events },
                { "left-handed",          "left-handed",     NULL },
                { "custom-acceleration",  NULL,              NULL },
                { "motion-acceleration",  "speed",           map_speed },
                { "motion-threshold",     NULL,              NULL },
                { "natural-scroll",       "natural-scroll",  NULL }
        };

        csd_settings_migrate_check ("org.cinnamon.settings-daemon.peripherals.trackball.deprecated",
                                    "/org/cinnamon/settings-daemon/peripherals/trackball/",
                                    "org.cinnamon.desktop.peripherals.trackball",
                                    "/org/cinnamon/desktop/peripherals/trackball/",
                                    trackball_entries, G_N_ELEMENTS (trackball_entries));
        csd_settings_migrate_check ("org.cinnamon.settings-daemon.peripherals.mouse.deprecated",
                                    "/org/cinnamon/settings-daemon/peripherals/mouse/",
                                    "org.cinnamon.desktop.peripherals.mouse",
                                    "/org/cinnamon/desktop/peripherals/mouse/",
                                    mouse_entries, G_N_ELEMENTS (mouse_entries));
        csd_settings_migrate_check ("org.cinnamon.settings-daemon.peripherals.touchpad.deprecated",
                                    "/org/cinnamon/settings-daemon/peripherals/touchpad/",
                                    "org.cinnamon.desktop.peripherals.touchpad",
                                    "/org/cinnamon/desktop/peripherals/touchpad/",
                                    touchpad_entries, G_N_ELEMENTS (touchpad_entries));
}

CinnamonSettingsXSettingsManager *
cinnamon_xsettings_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                migrate_mouse_settings ();
                manager_object = g_object_new (CINNAMON_TYPE_XSETTINGS_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CINNAMON_XSETTINGS_MANAGER (manager_object);
}
