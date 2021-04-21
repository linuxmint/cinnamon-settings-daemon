/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 * csd-automount.c: helpers for automounting hotplugged volumes
 *
 * Copyright (C) 2008, 2010 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */
 
#include <config.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#include <gdk/gdkkeysyms.h>

#include "csd-autorun.h"

static gboolean should_autorun_mount (GMount *mount);

#define CUSTOM_ITEM_ASK "csd-item-ask"
#define CUSTOM_ITEM_DO_NOTHING "csd-item-do-nothing"
#define CUSTOM_ITEM_OPEN_FOLDER "csd-item-open-folder"

typedef struct
{
	GtkWidget *dialog;

	GMount *mount;
	gboolean should_eject;

	gboolean selected_ignore;
	gboolean selected_open_folder;
	GAppInfo *selected_app;

	gboolean remember;

	char *x_content_type;

	CsdAutorunOpenWindow open_window_func;
	gpointer user_data;
} AutorunDialogData;

static int
csd_autorun_g_strv_find (char **strv, const char *find_me)
{
	guint index;

	g_return_val_if_fail (find_me != NULL, -1);

	for (index = 0; strv[index] != NULL; ++index) {
		if (strcmp (strv[index], find_me) == 0) {
			return index;
		}
	}

	return -1;
}


#define ICON_SIZE_STANDARD 48

static gint
get_icon_size_for_stock_size (GtkIconSize size)
{
	gint w, h;

	if (gtk_icon_size_lookup (size, &w, &h)) {
		return MAX (w, h);
	}
	return ICON_SIZE_STANDARD;
}

static GdkPixbuf *
render_icon (GIcon *icon, gint icon_size)
{
	GdkPixbuf *pixbuf;
	GtkIconInfo *info;

	pixbuf = NULL;

	if (G_IS_THEMED_ICON (icon)) {
		gchar const * const *names;

		info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
				                       icon,
				                       icon_size,
				                       0);

		if (info) {
			pixbuf = gtk_icon_info_load_icon (info, NULL);
		        g_object_unref (info);
		}

		if (pixbuf == NULL) {
			names = g_themed_icon_get_names (G_THEMED_ICON (icon));
			pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
							   *names,
							   icon_size,
							   0, NULL);
		}
	}
	else
	if (G_IS_FILE_ICON (icon)) {
		GFile *icon_file;
		gchar *path;

		icon_file = g_file_icon_get_file (G_FILE_ICON (icon));
		path = g_file_get_path (icon_file);
		pixbuf = gdk_pixbuf_new_from_file_at_size (path,
							   icon_size, icon_size, 
							   NULL);
		g_free (path);
		g_object_unref (G_OBJECT (icon_file));
	}

	return pixbuf;
}

static void
csd_autorun_get_preferences (const char *x_content_type,
                             gboolean *pref_start_app,
                             gboolean *pref_ignore,
                             gboolean *pref_open_folder)
{
        GSettings *settings;
	char **x_content_start_app;
	char **x_content_ignore;
	char **x_content_open_folder;

	g_return_if_fail (pref_start_app != NULL);
	g_return_if_fail (pref_ignore != NULL);
	g_return_if_fail (pref_open_folder != NULL);

        settings = g_settings_new ("org.cinnamon.desktop.media-handling");

	*pref_start_app = FALSE;
	*pref_ignore = FALSE;
	*pref_open_folder = FALSE;
	x_content_start_app = g_settings_get_strv (settings, "autorun-x-content-start-app");
	x_content_ignore = g_settings_get_strv (settings, "autorun-x-content-ignore");
	x_content_open_folder = g_settings_get_strv (settings, "autorun-x-content-open-folder");
	if (x_content_start_app != NULL) {
		*pref_start_app = csd_autorun_g_strv_find (x_content_start_app, x_content_type) != -1;
	}
	if (x_content_ignore != NULL) {
		*pref_ignore = csd_autorun_g_strv_find (x_content_ignore, x_content_type) != -1;
	}
	if (x_content_open_folder != NULL) {
		*pref_open_folder = csd_autorun_g_strv_find (x_content_open_folder, x_content_type) != -1;
	}
	g_strfreev (x_content_ignore);
	g_strfreev (x_content_start_app);
	g_strfreev (x_content_open_folder);
        g_object_unref (settings);
}

static char **
remove_elem_from_str_array (char **v,
                            const char *s)
{
        GPtrArray *array;
        guint idx;

        array = g_ptr_array_new ();

        for (idx = 0; v[idx] != NULL; idx++) {
                if (g_strcmp0 (v[idx], s) == 0) {
                        continue;
                }

                g_ptr_array_add (array, v[idx]);
        }

        g_ptr_array_add (array, NULL);

        g_free (v);

        return (char **) g_ptr_array_free (array, FALSE);
}

static char **
add_elem_to_str_array (char **v,
                       const char *s)
{
        GPtrArray *array;
        guint idx;

        array = g_ptr_array_new ();

        for (idx = 0; v[idx] != NULL; idx++) {
                g_ptr_array_add (array, v[idx]);
        }

        g_ptr_array_add (array, g_strdup (s));
        g_ptr_array_add (array, NULL);

        g_free (v);

        return (char **) g_ptr_array_free (array, FALSE);
}

static void
csd_autorun_set_preferences (const char *x_content_type,
                             gboolean pref_start_app,
                             gboolean pref_ignore,
                             gboolean pref_open_folder)
{
        GSettings *settings;
	char **x_content_start_app;
	char **x_content_ignore;
	char **x_content_open_folder;

	g_assert (x_content_type != NULL);

	settings = g_settings_new ("org.cinnamon.desktop.media-handling");

	x_content_start_app = g_settings_get_strv (settings, "autorun-x-content-start-app");
	x_content_ignore = g_settings_get_strv (settings, "autorun-x-content-ignore");
	x_content_open_folder = g_settings_get_strv (settings, "autorun-x-content-open-folder");

	x_content_start_app = remove_elem_from_str_array (x_content_start_app, x_content_type);
	if (pref_start_app) {
		x_content_start_app = add_elem_to_str_array (x_content_start_app, x_content_type);
	}
	g_settings_set_strv (settings, "autorun-x-content-start-app", (const gchar * const*) x_content_start_app);

	x_content_ignore = remove_elem_from_str_array (x_content_ignore, x_content_type);
	if (pref_ignore) {
		x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
	}
	g_settings_set_strv (settings, "autorun-x-content-ignore", (const gchar * const*) x_content_ignore);

	x_content_open_folder = remove_elem_from_str_array (x_content_open_folder, x_content_type);
	if (pref_open_folder) {
		x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
	}
	g_settings_set_strv (settings, "autorun-x-content-open-folder", (const gchar * const*) x_content_open_folder);

	g_strfreev (x_content_open_folder);
	g_strfreev (x_content_ignore);
	g_strfreev (x_content_start_app);
        g_object_unref (settings);
}

static void
custom_item_activated_cb (GtkAppChooserButton *button,
                          const gchar *item,
                          gpointer user_data)
{
        gchar *content_type;
        AutorunDialogData *data = user_data;

        content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (button));

        if (g_strcmp0 (item, CUSTOM_ITEM_ASK) == 0) {
                csd_autorun_set_preferences (content_type,
                                             FALSE, FALSE, FALSE);
                data->selected_open_folder = FALSE;
                data->selected_ignore = FALSE;
        } else if (g_strcmp0 (item, CUSTOM_ITEM_OPEN_FOLDER) == 0) {
                csd_autorun_set_preferences (content_type,
                                             FALSE, FALSE, TRUE);
                data->selected_open_folder = TRUE;
                data->selected_ignore = FALSE;
        } else if (g_strcmp0 (item, CUSTOM_ITEM_DO_NOTHING) == 0) {
                csd_autorun_set_preferences (content_type,
                                             FALSE, TRUE, FALSE);
                data->selected_open_folder = FALSE;
                data->selected_ignore = TRUE;
        }

        g_free (content_type);
}

static void 
combo_box_changed_cb (GtkComboBox *combo_box,
                      gpointer user_data)
{
        GAppInfo *info;
        AutorunDialogData *data = user_data;

        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

        if (info == NULL)
                return;

        if (data->selected_app != NULL) {
                g_object_unref (data->selected_app);
                data->selected_app = NULL;
        }

        data->selected_app = info;
}

static void
prepare_combo_box (GtkWidget *combo_box,
		   AutorunDialogData *data)
{
        GtkAppChooserButton *app_chooser = GTK_APP_CHOOSER_BUTTON (combo_box);
        GIcon *icon;
        gboolean pref_ask;
        gboolean pref_start_app;
        gboolean pref_ignore;
        gboolean pref_open_folder;
        GAppInfo *info;
        gchar *content_type;

        content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (app_chooser));

        gtk_app_chooser_button_set_show_default_item (GTK_APP_CHOOSER_BUTTON (combo_box), TRUE);

        /* fetch preferences for this content type */
        csd_autorun_get_preferences (content_type,
                                     &pref_start_app, &pref_ignore, &pref_open_folder);
        pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

        /* append the separator only if we have >= 1 apps in the chooser */
        if (info != NULL) {
                gtk_app_chooser_button_append_separator (app_chooser);
                g_object_unref (info);
        }

        icon = g_themed_icon_new ("dialog-question");
        gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_ASK,
                                                   _("Ask what to do"),
                                                   icon);
        g_object_unref (icon);

        icon = g_themed_icon_new ("window-close");
        gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING,
                                                   _("Do Nothing"),
                                                   icon);
        g_object_unref (icon);

        icon = g_themed_icon_new ("folder-open");
        gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER,
                                                   _("Open Folder"),
                                                   icon);
        g_object_unref (icon);

        gtk_app_chooser_button_set_show_dialog_item (app_chooser, TRUE);

        if (pref_ask) {
                gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_ASK);
        } else if (pref_ignore) {
                gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING);
        } else if (pref_open_folder) {
                gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER);
        }

        g_signal_connect (app_chooser, "changed",
                          G_CALLBACK (combo_box_changed_cb), data);
        g_signal_connect (app_chooser, "custom-item-activated",
                          G_CALLBACK (custom_item_activated_cb), data);

        g_free (content_type);
}

static gboolean
is_shift_pressed (void)
{
	gboolean ret;
	XkbStateRec state;
	Bool status;

	ret = FALSE;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
	status = XkbGetState (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
			      XkbUseCoreKbd, &state);
        gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

	if (status == Success) {
		ret = state.mods & ShiftMask;
	}

	return ret;
}

enum {
	AUTORUN_DIALOG_RESPONSE_EJECT = 0
};

static void
csd_autorun_launch_for_mount (GMount *mount, GAppInfo *app_info)
{
	GFile *root;
	GdkAppLaunchContext *launch_context;
	GError *error;
	gboolean result;
	GList *list;
	gchar *uri_scheme;
	gchar *uri;

	root = g_mount_get_root (mount);
	list = g_list_append (NULL, root);

	launch_context = gdk_app_launch_context_new ();

	error = NULL;
	result = g_app_info_launch (app_info,
				    list,
				    G_APP_LAUNCH_CONTEXT (launch_context),
				    &error);

	g_object_unref (launch_context);

	if (!result) {
		if (error->domain == G_IO_ERROR &&
		    error->code == G_IO_ERROR_NOT_SUPPORTED) {
			uri = g_file_get_uri (root);
			uri_scheme = g_uri_parse_scheme (uri);
			
			/* FIXME: Present user a dialog to choose another app when the last one failed to handle a file */
			g_warning ("Cannot open location: %s\n", error->message);

			g_free (uri_scheme);
			g_free (uri);
		} else {
			g_warning ("Cannot open app: %s\n", error->message);
		}
		g_error_free (error);
	}

	g_list_free (list);
	g_object_unref (root);
}

static void autorun_dialog_mount_unmounted (GMount *mount, AutorunDialogData *data);

static void
autorun_dialog_destroy (AutorunDialogData *data)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (data->mount),
					      G_CALLBACK (autorun_dialog_mount_unmounted),
					      data);

	gtk_widget_destroy (GTK_WIDGET (data->dialog));
	if (data->selected_app != NULL) {
		g_object_unref (data->selected_app);
	}
	g_object_unref (data->mount);
	g_free (data->x_content_type);
	g_free (data);
}

static void
autorun_dialog_mount_unmounted (GMount *mount, AutorunDialogData *data)
{
	/* remove the dialog if the media is unmounted */
	autorun_dialog_destroy (data);
}

static void
unmount_mount_callback (GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	GError *error;
	char *primary;
	gboolean unmounted;
	gboolean should_eject;
	GtkWidget *dialog;


	should_eject = user_data != NULL;

	error = NULL;
	if (should_eject) {
		unmounted = g_mount_eject_with_operation_finish (G_MOUNT (source_object),
								 res, &error);
	} else {
		unmounted = g_mount_unmount_with_operation_finish (G_MOUNT (source_object),
								   res, &error);
	}
	
	if (! unmounted) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			if (should_eject) {
				primary = g_strdup_printf (_("Unable to eject %p"), source_object);
			} else {
				primary = g_strdup_printf (_("Unable to unmount %p"), source_object);
			}

			dialog = gtk_message_dialog_new (NULL,
							 0,
							 GTK_MESSAGE_INFO,
							 GTK_BUTTONS_OK,
							 "%s",
							 primary);
			gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
								    "%s",
								    error->message);
			
			gtk_widget_show (GTK_WIDGET (dialog));
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			g_free (primary);
		}
	}

	if (error != NULL) {
		g_error_free (error);
	}
}

static void
do_unmount (GMount *mount, gboolean should_eject, GtkWindow *window)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (window);
	if (should_eject) {
		g_mount_eject_with_operation (mount,
					      0,
					      mount_op,
					      NULL,
					      unmount_mount_callback,
					      (gpointer) 1);
	} else {
		g_mount_unmount_with_operation (mount,
						0,
						mount_op,
						NULL,
						unmount_mount_callback,
						(gpointer) 0);
	}
	g_object_unref (mount_op);
}

static void
autorun_dialog_response (GtkDialog *dialog, gint response, AutorunDialogData *data)
{
	switch (response) {
	case AUTORUN_DIALOG_RESPONSE_EJECT:
		do_unmount (data->mount, data->should_eject, GTK_WINDOW (dialog));
		break;

	case GTK_RESPONSE_NONE:
		/* window was closed */
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	case GTK_RESPONSE_OK:
		/* do the selected action */

		if (data->remember) {
			/* make sure we don't ask again */
			csd_autorun_set_preferences (data->x_content_type, TRUE, data->selected_ignore, data->selected_open_folder);
			if (!data->selected_ignore && !data->selected_open_folder && data->selected_app != NULL) {
				g_app_info_set_as_default_for_type (data->selected_app,
								    data->x_content_type,
								    NULL);
			}
		} else {
			/* make sure we do ask again */
			csd_autorun_set_preferences (data->x_content_type, FALSE, FALSE, FALSE);
		}

		if (!data->selected_ignore && !data->selected_open_folder && data->selected_app != NULL) {
			csd_autorun_launch_for_mount (data->mount, data->selected_app);
		} else if (!data->selected_ignore && data->selected_open_folder) {
			if (data->open_window_func != NULL)
				data->open_window_func (data->mount, data->user_data);
		}
		break;
	}

	autorun_dialog_destroy (data);
}

static void
autorun_always_toggled (GtkToggleButton *togglebutton, AutorunDialogData *data)
{
	data->remember = gtk_toggle_button_get_active (togglebutton);
}

static gboolean
combo_box_enter_ok (GtkWidget *togglebutton, GdkEventKey *event, GtkDialog *dialog)
{
	if (event->keyval == GDK_KEY_KP_Enter || event->keyval == GDK_KEY_Return) {
		gtk_dialog_response (dialog, GTK_RESPONSE_OK);
		return TRUE;
	}
	return FALSE;
}

/* returns TRUE if a folder window should be opened */
static gboolean
do_autorun_for_content_type (GMount *mount,
                             const char *x_content_type,
                             CsdAutorunOpenWindow open_window_func,
                             gpointer user_data)
{
	AutorunDialogData *data;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *combo_box;
	GtkWidget *always_check_button;
	GtkWidget *eject_button;
	GtkWidget *image;
	char *markup;
	char *content_description;
	char *mount_name;
	GIcon *icon;
	GdkPixbuf *pixbuf;
	int icon_size;
	gboolean user_forced_dialog;
	gboolean pref_ask;
	gboolean pref_start_app;
	gboolean pref_ignore;
	gboolean pref_open_folder;
	char *media_greeting;
	gboolean ret;

	ret = FALSE;
	mount_name = NULL;

	if (g_content_type_is_a (x_content_type, "x-content/win32-software")) {
		/* don't pop up the dialog anyway if the content type says
 		 * windows software.
 		 */
		goto out;
	}

	user_forced_dialog = is_shift_pressed ();

	csd_autorun_get_preferences (x_content_type, &pref_start_app, &pref_ignore, &pref_open_folder);
	pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

	if (user_forced_dialog) {
		goto show_dialog;
	}

	if (!pref_ask && !pref_ignore && !pref_open_folder) {
		GAppInfo *app_info;
		app_info = g_app_info_get_default_for_type (x_content_type, FALSE);
		if (app_info != NULL) {
			csd_autorun_launch_for_mount (mount, app_info);
		}
		goto out;
	}

	if (pref_open_folder) {
		ret = TRUE;
		goto out;
	}

	if (pref_ignore) {
		goto out;
	}

show_dialog:

	mount_name = g_mount_get_name (mount);

	dialog = gtk_dialog_new ();
        gtk_window_set_default_size (GTK_WINDOW (dialog), 450, -1);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);

	icon = g_mount_get_icon (mount);
	icon_size = get_icon_size_for_stock_size (GTK_ICON_SIZE_DIALOG);
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
	pixbuf = render_icon (icon, icon_size);
        gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (image, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
	/* also use the icon on the dialog */
	gtk_window_set_title (GTK_WINDOW (dialog), mount_name);
	gtk_window_set_icon (GTK_WINDOW (dialog), pixbuf);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	g_object_unref (icon);
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);


	/* Customize greeting for well-known x-content types */
	if (strcmp (x_content_type, "x-content/audio-cdda") == 0) {
		media_greeting = _("You have just inserted an Audio CD.");
	} else if (strcmp (x_content_type, "x-content/audio-dvd") == 0) {
		media_greeting = _("You have just inserted an Audio DVD.");
	} else if (strcmp (x_content_type, "x-content/video-dvd") == 0) {
		media_greeting = _("You have just inserted a Video DVD.");
	} else if (strcmp (x_content_type, "x-content/video-vcd") == 0) {
		media_greeting = _("You have just inserted a Video CD.");
	} else if (strcmp (x_content_type, "x-content/video-svcd") == 0) {
		media_greeting = _("You have just inserted a Super Video CD.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank CD.");
	} else if (strcmp (x_content_type, "x-content/blank-dvd") == 0) {
		media_greeting = _("You have just inserted a blank DVD.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank Blu-Ray disc.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank HD DVD.");
	} else if (strcmp (x_content_type, "x-content/image-photocd") == 0) {
		media_greeting = _("You have just inserted a Photo CD.");
	} else if (strcmp (x_content_type, "x-content/image-picturecd") == 0) {
		media_greeting = _("You have just inserted a Picture CD.");
	} else if (strcmp (x_content_type, "x-content/image-dcf") == 0) {
		media_greeting = _("You have just inserted a medium with digital photos.");
	} else if (strcmp (x_content_type, "x-content/audio-player") == 0) {
		media_greeting = _("You have just inserted a digital audio player.");
	} else if (g_content_type_is_a (x_content_type, "x-content/software")) {
		media_greeting = _("You have just inserted a medium with software intended to be automatically started.");
	} else {
		/* fallback to generic greeting */
		media_greeting = _("You have just inserted a medium.");
	}
	markup = g_strdup_printf ("<big><b>%s %s</b></big>", media_greeting, _("Choose what application to launch."));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	content_description = g_content_type_get_description (x_content_type);
	markup = g_strdup_printf (_("Select how to open \"%s\" and whether to perform this action in the future for other media of type \"%s\"."), mount_name, content_description);
	g_free (content_description);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (label), 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	
	data = g_new0 (AutorunDialogData, 1);
	data->dialog = dialog;
	data->mount = g_object_ref (mount);
	data->remember = !pref_ask;
	data->selected_ignore = pref_ignore;
	data->x_content_type = g_strdup (x_content_type);
	data->selected_app = g_app_info_get_default_for_type (x_content_type, FALSE);
	data->open_window_func = open_window_func;
	data->user_data = user_data;

	combo_box = gtk_app_chooser_button_new (x_content_type);
	prepare_combo_box (combo_box, data);
	g_signal_connect (G_OBJECT (combo_box),
			  "key-press-event",
			  G_CALLBACK (combo_box_enter_ok),
			  dialog);

	gtk_box_pack_start (GTK_BOX (vbox), combo_box, TRUE, TRUE, 0);

	always_check_button = gtk_check_button_new_with_mnemonic (_("_Always perform this action"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (always_check_button), data->remember);
	g_signal_connect (G_OBJECT (always_check_button),
			  "toggled",
			  G_CALLBACK (autorun_always_toggled),
			  data);
	gtk_box_pack_start (GTK_BOX (vbox), always_check_button, TRUE, TRUE, 0);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), 
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Ok"), GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	if (g_mount_can_eject (mount)) {
		eject_button = gtk_button_new_with_mnemonic (_("_Eject"));
		data->should_eject = TRUE;
	} else {
		eject_button = gtk_button_new_with_mnemonic (_("_Unmount"));
		data->should_eject = FALSE;
	}
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), eject_button, AUTORUN_DIALOG_RESPONSE_EJECT);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (gtk_dialog_get_action_area (GTK_DIALOG (dialog))), eject_button, TRUE);

	/* show the dialog */
	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (autorun_dialog_response),
			  data);

	g_signal_connect (G_OBJECT (data->mount),
			  "unmounted",
			  G_CALLBACK (autorun_dialog_mount_unmounted),
			  data);

out:
	g_free (mount_name);
	return ret;
}

typedef struct {
	GMount *mount;
	CsdAutorunOpenWindow open_window_func;
	gpointer user_data;
	GSettings *settings;
} AutorunData;

static void
autorun_guessed_content_type_callback (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	GError *error;
	char **guessed_content_type;
	AutorunData *data = user_data;
	gboolean open_folder;

	open_folder = FALSE;

	error = NULL;
	guessed_content_type = g_mount_guess_content_type_finish (G_MOUNT (source_object), res, &error);
	g_object_set_data_full (source_object,
				"csd-content-type-cache",
				g_strdupv (guessed_content_type),
				(GDestroyNotify)g_strfreev);
	if (error != NULL) {
		g_warning ("Unable to guess content type for mount: %s", error->message);
		g_error_free (error);
	} else {
		if (guessed_content_type != NULL && g_strv_length (guessed_content_type) > 0) {
			int n;
			for (n = 0; guessed_content_type[n] != NULL; n++) {
				if (do_autorun_for_content_type (data->mount, guessed_content_type[n], 
								 data->open_window_func, data->user_data)) {
					open_folder = TRUE;
				}
			}
			g_strfreev (guessed_content_type);
		} else {
			if (g_settings_get_boolean (data->settings, "automount-open")) {
				open_folder = TRUE;
			}
		}
	}

	/* only open the folder once.. */
	if (open_folder && data->open_window_func != NULL) {
		data->open_window_func (data->mount, data->user_data);
	}

	g_object_unref (data->mount);
	g_object_unref (data->settings);
	g_free (data);
}

void
csd_autorun (GMount *mount,
             GSettings *settings,
             CsdAutorunOpenWindow open_window_func,
             gpointer user_data)
{
	AutorunData *data;

	if (!should_autorun_mount (mount) ||
	    g_settings_get_boolean (settings, "autorun-never")) {
		return;
	}

	data = g_new0 (AutorunData, 1);
	data->mount = g_object_ref (mount);
	data->open_window_func = open_window_func;
	data->user_data = user_data;
	data->settings = g_object_ref (settings);

	g_mount_guess_content_type (mount,
				    FALSE,
				    NULL,
				    autorun_guessed_content_type_callback,
				    data);
}

static gboolean
remove_allow_volume (gpointer data)
{
	GVolume *volume = data;

	g_object_set_data (G_OBJECT (volume), "csd-allow-autorun", NULL);
	return FALSE;
}

void
csd_allow_autorun_for_volume (GVolume *volume)
{
	g_object_set_data (G_OBJECT (volume), "csd-allow-autorun", GINT_TO_POINTER (1));
}

#define INHIBIT_AUTORUN_SECONDS 10

void
csd_allow_autorun_for_volume_finish (GVolume *volume)
{
	if (g_object_get_data (G_OBJECT (volume), "csd-allow-autorun") != NULL) {
		g_timeout_add_seconds_full (0,
					    INHIBIT_AUTORUN_SECONDS,
					    remove_allow_volume,
					    g_object_ref (volume),
					    g_object_unref);
	}
}

static gboolean
should_skip_native_mount_root (GFile *root)
{
	char *path;
	gboolean should_skip;

	/* skip any mounts in hidden directory hierarchies */
	path = g_file_get_path (root);
	should_skip = strstr (path, "/.") != NULL;
	g_free (path);

	return should_skip;
}

static gboolean
should_autorun_mount (GMount *mount)
{
	GFile *root;
	GVolume *enclosing_volume;
	gboolean ignore_autorun;

	ignore_autorun = TRUE;
	enclosing_volume = g_mount_get_volume (mount);
	if (enclosing_volume != NULL) {
		if (g_object_get_data (G_OBJECT (enclosing_volume), "csd-allow-autorun") != NULL) {
			ignore_autorun = FALSE;
			g_object_set_data (G_OBJECT (enclosing_volume), "csd-allow-autorun", NULL);
		}
	}

	if (ignore_autorun) {
		if (enclosing_volume != NULL) {
			g_object_unref (enclosing_volume);
		}
		return FALSE;
	}
	
	root = g_mount_get_root (mount);

	/* only do autorun on local files or files where g_volume_should_automount() returns TRUE */
	ignore_autorun = TRUE;
	if ((g_file_is_native (root) && !should_skip_native_mount_root (root)) || 
	    (enclosing_volume != NULL && g_volume_should_automount (enclosing_volume))) {
		ignore_autorun = FALSE;
	}
	if (enclosing_volume != NULL) {
		g_object_unref (enclosing_volume);
	}
	g_object_unref (root);

	return !ignore_autorun;
}

void
csd_autorun_for_content_type (GMount               *mount,
                              const gchar          *content_type,
                              CsdAutorunOpenWindow  callback,
                              gpointer              user_data)
{
    do_autorun_for_content_type (mount, content_type, callback, user_data);
}
