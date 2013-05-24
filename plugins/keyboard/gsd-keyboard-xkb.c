/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#ifndef HAVE_APPINDICATOR
#include <libgnomekbd/gkbd-status.h>
#endif
#include <libgnomekbd/gkbd-keyboard-drawing.h>
#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-indicator-config.h>
#include <libgnomekbd/gkbd-keyboard-config.h>
#include <libgnomekbd/gkbd-util.h>

#ifdef HAVE_APPINDICATOR
#include <libappindicator/app-indicator.h>
#include "gkbd-configuration.h"
#endif

#include "gsd-keyboard-xkb.h"
#include "delayed-dialog.h"
#include "gnome-settings-profile.h"

#define SETTINGS_KEYBOARD_DIR "org.gnome.settings-daemon.plugins.keyboard"

static GsdKeyboardManager *manager = NULL;

static XklEngine *xkl_engine;
static XklConfigRegistry *xkl_registry = NULL;

static GkbdDesktopConfig current_config;
static GkbdKeyboardConfig current_kbd_config;

/* never terminated */
static GkbdKeyboardConfig initial_sys_kbd_config;

static gboolean inited_ok = FALSE;

static GSettings *settings_desktop = NULL;
static GSettings *settings_keyboard = NULL;

static PostActivationCallback pa_callback = NULL;
static void *pa_callback_user_data = NULL;

#ifdef HAVE_APPINDICATOR
static AppIndicator *app_indicator = NULL;
static GkbdConfiguration *gkbd_configuration = NULL;
static GkbdIndicatorConfig current_ind_config;
static GSList *groups_items_group = NULL;
static size_t lang_menu_items = 0;

static void state_callback (XklEngine * engine,
                            XklEngineStateChange changeType,
                            gint group, gboolean restore);
static void gsd_keyboard_configuration_changed (GkbdConfiguration *configuration);

#else
static GtkStatusIcon *icon = NULL;
#endif

static GHashTable *preview_dialogs = NULL;

static void
activation_error (void)
{
	char const *vendor;
	GtkWidget *dialog;

	vendor =
	    ServerVendor (GDK_DISPLAY_XDISPLAY
			  (gdk_display_get_default ()));

	/* VNC viewers will not work, do not barrage them with warnings */
	if (NULL != vendor && NULL != strstr (vendor, "VNC"))
		return;

	dialog = gtk_message_dialog_new_with_markup (NULL,
						     0,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     _
						     ("Error activating XKB configuration.\n"
						      "There can be various reasons for that.\n\n"
						      "If you report this situation as a bug, include the results of\n"
						      " • <b>%s</b>\n"
						      " • <b>%s</b>\n"
						      " • <b>%s</b>\n"
						      " • <b>%s</b>"),
						     "xprop -root | grep XKB",
						     "gsettings get org.gnome.libgnomekbd.keyboard model",
						     "gsettings get org.gnome.libgnomekbd.keyboard layouts",
						     "gsettings get org.gnome.libgnomekbd.keyboard options");
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gsd_delayed_show_dialog (dialog);
}

static gboolean
ensure_xkl_registry (void)
{
	if (!xkl_registry) {
		xkl_registry =
		    xkl_config_registry_get_instance (xkl_engine);
		/* load all materials, unconditionally! */
		if (!xkl_config_registry_load (xkl_registry, TRUE)) {
			g_object_unref (xkl_registry);
			xkl_registry = NULL;
			return FALSE;
		}
	}

	return TRUE;
}

static void
apply_desktop_settings (void)
{
	if (!inited_ok)
		return;

	gsd_keyboard_manager_apply_settings (manager);
	gkbd_desktop_config_load (&current_config);
	/* again, probably it would be nice to compare things
	   before activating them */
	gkbd_desktop_config_activate (&current_config);
}

static void
popup_menu_launch_capplet ()
{
	GAppInfo *info;
	GdkAppLaunchContext *ctx;
	GError *error = NULL;

	info =
	    g_app_info_create_from_commandline
	    ("gnome-control-center region", NULL, 0, &error);

	if (info != NULL) {
		ctx =
		    gdk_display_get_app_launch_context
		    (gdk_display_get_default ());

		if (g_app_info_launch (info, NULL,
				   G_APP_LAUNCH_CONTEXT (ctx), &error) == FALSE) {
			g_warning
				("Could not execute keyboard properties capplet: [%s]\n",
				 error->message);
			g_error_free (error);
		}

		g_object_unref (info);
		g_object_unref (ctx);
	}

}

static void
show_layout_destroy (GtkWidget * dialog, gint group)
{
	g_hash_table_remove (preview_dialogs, GINT_TO_POINTER (group));
}

static void
popup_menu_show_layout ()
{
	GtkWidget *dialog;
	XklEngine *engine =
	    xkl_engine_get_instance (GDK_DISPLAY_XDISPLAY
				     (gdk_display_get_default ()));
	XklState *xkl_state = xkl_engine_get_current_state (engine);
#ifdef HAVE_APPINDICATOR
	gchar **group_names = gkbd_configuration_get_group_names (gkbd_configuration);
#else
	gchar **group_names = gkbd_status_get_group_names ();
#endif
	gpointer p = g_hash_table_lookup (preview_dialogs,
					  GINT_TO_POINTER
					  (xkl_state->group));

	if (xkl_state->group < 0
	    || xkl_state->group >= g_strv_length (group_names)) {
		return;
	}

	if (p != NULL) {
		/* existing window */
		gtk_window_present (GTK_WINDOW (p));
		return;
	}

	if (!ensure_xkl_registry ())
		return;

	dialog = gkbd_keyboard_drawing_dialog_new ();
	gkbd_keyboard_drawing_dialog_set_group (dialog, xkl_registry, xkl_state->group);

	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (show_layout_destroy),
			  GINT_TO_POINTER (xkl_state->group));
	g_hash_table_insert (preview_dialogs,
			     GINT_TO_POINTER (xkl_state->group), dialog);
	gtk_widget_show_all (dialog);
}

static void
popup_menu_set_group (gint group_number, gboolean only_menu)
{
#ifdef HAVE_APPINDICATOR
	XklEngine *engine = gkbd_configuration_get_xkl_engine (gkbd_configuration);
#else
	XklEngine *engine = gkbd_status_get_xkl_engine ();
#endif
	XklState *st = xkl_engine_get_current_state(engine);
	Window cur;
	st->group = group_number;
	xkl_engine_allow_one_switch_to_secondary_group (engine);
	cur = xkl_engine_get_current_window (engine);
	if (cur != (Window) NULL) {
		xkl_debug (150, "Enforcing the state %d for window %lx\n",
			   st->group, cur);
#ifdef HAVE_APPINDICATOR
                // Setting the state may trigger state_callback to be called, which will then
                // cause popup_menu_set_group to be called again.
                g_signal_handlers_block_by_func (engine, G_CALLBACK (state_callback), NULL);
#endif
		xkl_engine_save_state (engine,
				       xkl_engine_get_current_window
				       (engine), st);
#ifdef HAVE_APPINDICATOR
                g_signal_handlers_unblock_by_func (engine, G_CALLBACK (state_callback), NULL);
#endif
/*    XSetInputFocus( GDK_DISPLAY(), cur, RevertToNone, CurrentTime );*/
	} else {
		xkl_debug (150,
			   "??? Enforcing the state %d for unknown window\n",
			   st->group);
		/* strange situation - bad things can happen */
	}
        if (!only_menu)
        	xkl_engine_lock_group (engine, st->group);
#ifdef HAVE_APPINDICATOR
	XklConfigRec * xklrec = xkl_config_rec_new();
	xkl_config_rec_get_from_server (xklrec, engine);
	XklConfigRegistry *registry = xkl_config_registry_get_instance(engine);

	gkbd_keyboard_config_load_from_x_current (&current_kbd_config, xklrec);
	xkl_config_registry_load (registry, current_config.load_extra_items);

	int g;

        if (current_ind_config.show_flags) {
	        gchar *image_file = gkbd_indicator_config_get_images_file (&current_ind_config,
						              &current_kbd_config,
						              st->group);

        
                app_indicator_set_icon_full(app_indicator, image_file, _("Keyboard"));
	        app_indicator_set_label(app_indicator, NULL, NULL);
                g_free(image_file);
        } else {
                gchar * guide = "XXX";
	        gchar ** shortnames;
	        gchar ** longnames;
	        gchar * layout_name = NULL;
	        gchar * lname = NULL;
	        GHashTable *ln2cnt_map = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, NULL);

	        gkbd_desktop_config_load_group_descriptions(&current_config, registry, 
		        (const gchar **) xklrec->layouts, 
		        (const gchar **) xklrec->variants,
		        &shortnames,
		        &longnames); 

	        for (g = 0; g < g_strv_length (shortnames);g++) {
		        gpointer pcounter = NULL;
		        gchar *prev_layout_name = NULL;
		        int counter = 0;

		        if (g < g_strv_length (shortnames)) {
			        if (xkl_engine_get_features (engine) &
			            XKLF_MULTIPLE_LAYOUTS_SUPPORTED) {
				        gchar *longname = (gchar *) current_kbd_config.layouts_variants[g];
				        gchar *variant_name;
				        if (!gkbd_keyboard_config_split_items (longname, &lname, &variant_name))
					        /* just in case */
					        lname = longname;

				        if (shortnames != NULL) {
					        gchar *shortname = shortnames[g];
					        if (shortname != NULL && *shortname != '\0') {
						        lname = shortname;
					        }
				        }
			        } else {
				        lname = longnames[g];
			        }
		        }
		        if (lname == NULL)
			        lname = "";

		        /* Process layouts with repeating description */
		        if (g_hash_table_lookup_extended (ln2cnt_map, lname, (gpointer *) & prev_layout_name, &pcounter)) {
			        /* "next" same description */
			        counter = GPOINTER_TO_INT (pcounter);
                                guide = "XXX1";
		        }
		        g_hash_table_insert (ln2cnt_map, g_strdup (lname), GINT_TO_POINTER (counter+1));

		        if (st->group == g) {
			        if (counter > 0) {
				        gchar appendix[10] = "";
				        gint utf8length;
				        gunichar cidx;
				        /* Unicode subscript 2, 3, 4 */
				        cidx = 0x2081 + counter;
				        utf8length = g_unichar_to_utf8 (cidx, appendix);
				        appendix[utf8length] = '\0';
				        layout_name = g_strconcat (lname, appendix, NULL);
			        } else {
				        layout_name = g_strdup(lname);
			        }
		        }
	        }

	        // Guide of 3 wide-ish and one thin
	        app_indicator_set_label(app_indicator, layout_name, guide);
	        g_hash_table_destroy(ln2cnt_map);
	        g_free(layout_name);
	        g_strfreev(longnames);
	        g_strfreev(shortnames);
        }

        // Refresh popup menu
        gsd_keyboard_configuration_changed (gkbd_configuration);

	g_object_unref (G_OBJECT (xklrec));
	g_object_unref (G_OBJECT (registry));
#endif
}

static void
popup_menu_set_group_cb (GtkMenuItem * item, gpointer param)
{
	gint group_number = GPOINTER_TO_INT (param);

#ifdef HAVE_APPINDICATOR
	if ((item) != NULL && (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item))))
		return;
#endif

	popup_menu_set_group(group_number, FALSE);
}

#ifdef HAVE_APPINDICATOR
static void
state_callback (XklEngine * engine,
		XklEngineStateChange changeType,
		gint group, gboolean restore)
{
	if ((changeType == GROUP_CHANGED) || (changeType == INDICATORS_CHANGED))
		popup_menu_set_group (GINT_TO_POINTER(group), TRUE);
}

static int
get_current_group(void)
{
	XklEngine *engine = gkbd_configuration_get_xkl_engine (gkbd_configuration);
	return xkl_engine_get_current_window_group (engine);
}
#endif

static GtkMenu *
create_status_menu (void)
{
	GtkMenu *popup_menu = GTK_MENU (gtk_menu_new ());
	int i = 0;
#ifdef HAVE_APPINDICATOR
	const char * const *current_name = gkbd_configuration_get_group_names (gkbd_configuration);
	groups_items_group = NULL;
        GtkWidget *item;
	int group = get_current_group();
	lang_menu_items = 0;
#else
	GtkMenu *groups_menu = GTK_MENU (gtk_menu_new ());
	gchar **current_name = gkbd_status_get_group_names ();

	GtkWidget *item = gtk_menu_item_new_with_mnemonic (_("_Layouts"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
				   GTK_WIDGET (groups_menu));

	item = gtk_menu_item_new_with_mnemonic (_("Show _Keyboard Layout..."));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_show_layout, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);

	/* translators note:
	 * This is the name of the gnome-control-center "region" panel */
	item = gtk_menu_item_new_with_mnemonic (_("Region and Language Settings"));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_launch_capplet, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
#endif

	for (i = 0; current_name && *current_name; i++, current_name++) {
#ifdef HAVE_APPINDICATOR
		item = gtk_radio_menu_item_new_with_label (groups_items_group, *current_name);
		groups_items_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
						i == group);

		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (popup_menu_set_group_cb),
				  GINT_TO_POINTER (i));
		lang_menu_items++;
#else
		gchar *image_file = gkbd_status_get_image_filename (i);

		if (image_file == NULL) {
			item =
			    gtk_menu_item_new_with_label (*current_name);
		} else {
			GdkPixbuf *pixbuf =
			    gdk_pixbuf_new_from_file_at_size (image_file,
							      24, 24,
							      NULL);
			GtkWidget *img =
			    gtk_image_new_from_pixbuf (pixbuf);
			item =
			    gtk_image_menu_item_new_with_label
			    (*current_name);
			gtk_widget_show (img);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
						       (item), img);
			gtk_image_menu_item_set_always_show_image
			    (GTK_IMAGE_MENU_ITEM (item), TRUE);
			g_free (image_file);
		}
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (groups_menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (popup_menu_set_group_cb),
				  GINT_TO_POINTER (i));
#endif
	}

#ifdef HAVE_APPINDICATOR
        item = gtk_separator_menu_item_new();
        gtk_widget_show(item);
        gtk_menu_shell_append(GTK_MENU_SHELL (popup_menu), item);

	item =
	    gtk_menu_item_new_with_mnemonic (_("Show _Layout Chart"));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_show_layout,
			  NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Keyboard Layout _Settings..."));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_launch_capplet, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
#endif

	return popup_menu;
}

#ifndef HAVE_APPINDICATOR
static void
status_icon_popup_menu_cb (GtkStatusIcon * icon, guint button, guint time)
{
	GtkMenu *popup_menu = create_status_menu ();

	gtk_menu_popup (popup_menu, NULL, NULL,
			gtk_status_icon_position_menu,
			(gpointer) icon, button, time);
}
#endif

#ifdef HAVE_APPINDICATOR
static void
scroll_event (AppIndicator *indicator, gint delta, guint direction)
{
	g_return_if_fail(IS_APP_INDICATOR(indicator));
	int group = get_current_group();

	if ((direction == 0 && group == 0) ||
	    (direction == 1 && group == lang_menu_items-1))
		return;

        popup_menu_set_group((direction == 0 ? group-1 : group+1), FALSE);
}
#endif

static void
show_hide_icon ()
{
	if (g_strv_length (current_kbd_config.layouts_variants) > 1) {
#ifdef HAVE_APPINDICATOR
		if (app_indicator == NULL) {
			GtkMenu *popup_menu = create_status_menu ();

			app_indicator = app_indicator_new ("keyboard",
							   "keyboard",
							   APP_INDICATOR_CATEGORY_HARDWARE);
	                int group = get_current_group();
	        	popup_menu_set_group(GINT_TO_POINTER(group), TRUE);
			app_indicator_set_status (app_indicator,
						  APP_INDICATOR_STATUS_ACTIVE);
			app_indicator_set_menu (app_indicator,
						popup_menu);
			app_indicator_set_title (app_indicator, _("Keyboard"));
			g_signal_connect (app_indicator, "scroll-event", G_CALLBACK (scroll_event), NULL);
		} else {
                        XklEngine *engine = gkbd_configuration_get_xkl_engine (gkbd_configuration);
			XklState *st = xkl_engine_get_current_state(engine);
			popup_menu_set_group(GINT_TO_POINTER(st->group), TRUE);
                }
#else
		if (icon == NULL) {
			xkl_debug (150, "Creating keyboard status icon\n");
			icon = gkbd_status_new ();
			g_signal_connect (icon, "popup-menu",
					  G_CALLBACK
					  (status_icon_popup_menu_cb),
					  NULL);

		}
#endif
	} else {
#ifdef HAVE_APPINDICATOR
		g_clear_object (&app_indicator);
#else
		if (icon != NULL) {
			xkl_debug (150, "Destroying icon\n");
			g_object_unref (icon);
			icon = NULL;
		}
#endif
	}
}

static gboolean
try_activating_xkb_config_if_new (GkbdKeyboardConfig *
				  current_sys_kbd_config)
{
	/* Activate - only if different! */
	if (!gkbd_keyboard_config_equals
	    (&current_kbd_config, current_sys_kbd_config)) {
		if (gkbd_keyboard_config_activate (&current_kbd_config)) {
			if (pa_callback != NULL) {
				(*pa_callback) (pa_callback_user_data);
				return TRUE;
			}
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
filter_xkb_config (void)
{
	XklConfigItem *item;
	gchar *lname;
	gchar *vname;
	gchar **lv;
	gboolean any_change = FALSE;

	xkl_debug (100, "Filtering configuration against the registry\n");
	if (!ensure_xkl_registry ())
		return FALSE;

	lv = current_kbd_config.layouts_variants;
	item = xkl_config_item_new ();
	while (*lv) {
		xkl_debug (100, "Checking [%s]\n", *lv);
		if (gkbd_keyboard_config_split_items (*lv, &lname, &vname)) {
			gboolean should_be_dropped = FALSE;
			g_snprintf (item->name, sizeof (item->name), "%s",
				    lname);
			if (!xkl_config_registry_find_layout
			    (xkl_registry, item)) {
				xkl_debug (100, "Bad layout [%s]\n",
					   lname);
				should_be_dropped = TRUE;
			} else if (vname) {
				g_snprintf (item->name,
					    sizeof (item->name), "%s",
					    vname);
				if (!xkl_config_registry_find_variant
				    (xkl_registry, lname, item)) {
					xkl_debug (100,
						   "Bad variant [%s(%s)]\n",
						   lname, vname);
					should_be_dropped = TRUE;
				}
			}
			if (should_be_dropped) {
				gkbd_strv_behead (lv);
				any_change = TRUE;
				continue;
			}
		}
		lv++;
	}
	g_object_unref (item);
	return any_change;
}

static void
apply_xkb_settings (void)
{
	GkbdKeyboardConfig current_sys_kbd_config;

	if (!inited_ok)
		return;

	gkbd_keyboard_config_init (&current_sys_kbd_config, xkl_engine);

	gkbd_keyboard_config_load (&current_kbd_config,
				   &initial_sys_kbd_config);

	gkbd_keyboard_config_load_from_x_current (&current_sys_kbd_config,
						  NULL);

#ifdef HAVE_APPINDICATOR
	gkbd_indicator_config_init (&current_ind_config, xkl_engine);
	gkbd_indicator_config_load (&current_ind_config);

	gkbd_indicator_config_load_image_filenames (&current_ind_config,
						    &current_kbd_config);
	gkbd_indicator_config_activate (&current_ind_config);
#endif /* HAVE_APPINDICATOR */

	if (!try_activating_xkb_config_if_new (&current_sys_kbd_config)) {
		if (filter_xkb_config ()) {
			if (!try_activating_xkb_config_if_new
			    (&current_sys_kbd_config)) {
				g_warning
				    ("Could not activate the filtered XKB configuration");
				activation_error ();
			}
		} else {
			g_warning
			    ("Could not activate the XKB configuration");
			activation_error ();
		}
	} else
		xkl_debug (100,
			   "Actual KBD configuration was not changed: redundant notification\n");

	gkbd_keyboard_config_term (&current_sys_kbd_config);
	show_hide_icon ();
}

static void
gsd_keyboard_xkb_analyze_sysconfig (void)
{
	if (!inited_ok)
		return;

	gkbd_keyboard_config_init (&initial_sys_kbd_config, xkl_engine);
	gkbd_keyboard_config_load_from_x_initial (&initial_sys_kbd_config,
						  NULL);
}

#ifdef HAVE_APPINDICATOR
/* When the configuration changed update the indicator */
static void
gsd_keyboard_configuration_changed (GkbdConfiguration *configuration)
{
	GtkMenu *popup_menu;

	if (!app_indicator)
		return;

	popup_menu = create_status_menu ();
	app_indicator_set_menu (app_indicator,
			popup_menu);
}
#endif

void
gsd_keyboard_xkb_set_post_activation_callback (PostActivationCallback fun,
					       void *user_data)
{
	pa_callback = fun;
	pa_callback_user_data = user_data;
}

static GdkFilterReturn
gsd_keyboard_xkb_evt_filter (GdkXEvent * xev, GdkEvent * event)
{
	XEvent *xevent = (XEvent *) xev;
	xkl_engine_filter_events (xkl_engine, xevent);
	return GDK_FILTER_CONTINUE;
}

/* When new Keyboard is plugged in - reload the settings */
static void
gsd_keyboard_new_device (XklEngine * engine)
{
	apply_desktop_settings ();
	apply_xkb_settings ();
}

void
gsd_keyboard_xkb_init (GsdKeyboardManager * kbd_manager)
{
	Display *display =
	    GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	gnome_settings_profile_start (NULL);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   DATADIR G_DIR_SEPARATOR_S
					   "icons");

#ifdef HAVE_APPINDICATOR
	gkbd_configuration = gkbd_configuration_get ();
	g_signal_connect (gkbd_configuration, "changed",
			  G_CALLBACK (gsd_keyboard_configuration_changed), NULL);
	g_signal_connect (gkbd_configuration, "group-changed",
			  G_CALLBACK (gsd_keyboard_configuration_changed), NULL);
#endif
	manager = kbd_manager;
	gnome_settings_profile_start ("xkl_engine_get_instance");
	xkl_engine = xkl_engine_get_instance (display);
	gnome_settings_profile_end ("xkl_engine_get_instance");
	if (xkl_engine) {
		inited_ok = TRUE;

		gkbd_desktop_config_init (&current_config, xkl_engine);
		gkbd_keyboard_config_init (&current_kbd_config,
					   xkl_engine);
		xkl_engine_backup_names_prop (xkl_engine);
		gsd_keyboard_xkb_analyze_sysconfig ();

		settings_desktop = g_settings_new (GKBD_DESKTOP_SCHEMA);
		settings_keyboard = g_settings_new (GKBD_KEYBOARD_SCHEMA);
		g_signal_connect (settings_desktop, "changed",
				  (GCallback) apply_desktop_settings,
				  NULL);
		g_signal_connect (settings_keyboard, "changed",
				  (GCallback) apply_xkb_settings, NULL);

#ifdef HAVE_APPINDICATOR
		g_signal_connect (xkl_engine, "X-state-changed", G_CALLBACK (state_callback), NULL);
#endif
		gdk_window_add_filter (NULL, (GdkFilterFunc)
				       gsd_keyboard_xkb_evt_filter, NULL);

		if (xkl_engine_get_features (xkl_engine) &
		    XKLF_DEVICE_DISCOVERY)
			g_signal_connect (xkl_engine, "X-new-device",
					  G_CALLBACK
					  (gsd_keyboard_new_device), NULL);

		gnome_settings_profile_start ("xkl_engine_start_listen");
		xkl_engine_start_listen (xkl_engine,
					 XKLL_MANAGE_LAYOUTS |
					 XKLL_MANAGE_WINDOW_STATES);
		gnome_settings_profile_end ("xkl_engine_start_listen");

		gnome_settings_profile_start ("apply_desktop_settings");
		apply_desktop_settings ();
		gnome_settings_profile_end ("apply_desktop_settings");
		gnome_settings_profile_start ("apply_xkb_settings");
		apply_xkb_settings ();
		gnome_settings_profile_end ("apply_xkb_settings");
	}
	preview_dialogs = g_hash_table_new (g_direct_hash, g_direct_equal);

	gnome_settings_profile_end (NULL);
}

void
gsd_keyboard_xkb_shutdown (void)
{
	if (!inited_ok)
		return;

	pa_callback = NULL;
	pa_callback_user_data = NULL;
	manager = NULL;

	if (preview_dialogs != NULL)
		g_hash_table_destroy (preview_dialogs);

	if (!inited_ok)
		return;

	xkl_engine_stop_listen (xkl_engine,
				XKLL_MANAGE_LAYOUTS |
				XKLL_MANAGE_WINDOW_STATES);

	gdk_window_remove_filter (NULL, (GdkFilterFunc)
				  gsd_keyboard_xkb_evt_filter, NULL);

	g_object_unref (settings_desktop);
	settings_desktop = NULL;
	g_object_unref (settings_keyboard);
	settings_keyboard = NULL;

	if (xkl_registry) {
		g_object_unref (xkl_registry);
	}

	g_object_unref (xkl_engine);

	xkl_engine = NULL;

#ifdef HAVE_APPINDICATOR
	g_clear_object (&gkbd_configuration);
#endif

	inited_ok = FALSE;
}
