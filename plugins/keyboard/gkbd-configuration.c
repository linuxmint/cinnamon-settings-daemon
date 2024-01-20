/*
 * Copyright (C) 2010 Canonical Ltd.
 * 
 * Authors: Jan Arne Petersen <jpetersen@openismus.com>
 * 
 * Based on gkbd-status.c by Sergey V. Udaltsov <svu@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <memory.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-indicator-config.h>

#include "gkbd-configuration.h"

struct _GkbdConfigurationPrivate {
	XklEngine *engine;
	XklConfigRegistry *registry;

	GkbdDesktopConfig cfg;
	GkbdIndicatorConfig ind_cfg;
	GkbdKeyboardConfig kbd_cfg;

	gchar **full_group_names;
	gchar **short_group_names;

	gulong state_changed_handler;
	gulong config_changed_handler;
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_GROUP_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

#define GKBD_CONFIGURATION_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), GKBD_TYPE_CONFIGURATION, GkbdConfigurationPrivate))

G_DEFINE_TYPE (GkbdConfiguration, gkbd_configuration, G_TYPE_OBJECT)

/* Should be called once for all widgets */
static void
gkbd_configuration_cfg_changed (GSettings *settings,
				 const char *key,
				 GkbdConfiguration * configuration)
{
	GkbdConfigurationPrivate *priv = configuration->priv;

	xkl_debug (100,
		   "General configuration changed in GSettings - reiniting...\n");
	gkbd_desktop_config_load (&priv->cfg);
	gkbd_desktop_config_activate (&priv->cfg);

	g_signal_emit (configuration,
		       signals[SIGNAL_CHANGED], 0);
}

/* Should be called once for all widgets */
static void
gkbd_configuration_ind_cfg_changed (GSettings *settings,
				     const char *key,
				     GkbdConfiguration * configuration)
{
	GkbdConfigurationPrivate *priv = configuration->priv;
	xkl_debug (100,
		   "Applet configuration changed in GSettings - reiniting...\n");
	gkbd_indicator_config_load (&priv->ind_cfg);

	gkbd_indicator_config_free_image_filenames (&priv->ind_cfg);
	gkbd_indicator_config_load_image_filenames (&priv->ind_cfg,
						    &priv->kbd_cfg);

	gkbd_indicator_config_activate (&priv->ind_cfg);

	g_signal_emit (configuration,
		       signals[SIGNAL_CHANGED], 0);
}

static void
gkbd_configuration_load_group_names (GkbdConfiguration * configuration,
				     XklConfigRec * xklrec)
{
	GkbdConfigurationPrivate *priv = configuration->priv;

	if (!gkbd_desktop_config_load_group_descriptions (&priv->cfg,
							  priv->registry,
							  (const char **) xklrec->layouts,
							  (const char **) xklrec->variants,
	     						  &priv->short_group_names,
							  &priv->full_group_names)) {
		/* We just populate no short names (remain NULL) - 
		 * full names are going to be used anyway */
		gint i, total_groups =
		    xkl_engine_get_num_groups (priv->engine);
		xkl_debug (150, "group descriptions loaded: %d!\n",
			   total_groups);
		priv->full_group_names =
		    g_new0 (char *, total_groups + 1);

		if (xkl_engine_get_features (priv->engine) &
		    XKLF_MULTIPLE_LAYOUTS_SUPPORTED) {
			for (i = 0; priv->kbd_cfg.layouts_variants[i]; i++) {
				priv->full_group_names[i] =
				    g_strdup ((char *) priv->kbd_cfg.layouts_variants[i]);
			}
		} else {
			for (i = total_groups; --i >= 0;) {
				priv->full_group_names[i] =
				    g_strdup_printf ("Group %d", i);
			}
		}
	}
}

/* Should be called once for all widgets */
static void
gkbd_configuration_kbd_cfg_callback (XklEngine *engine,
				     GkbdConfiguration *configuration)
{
	GkbdConfigurationPrivate *priv = configuration->priv;
	XklConfigRec *xklrec = xkl_config_rec_new ();
	xkl_debug (100,
		   "XKB configuration changed on X Server - reiniting...\n");

	gkbd_keyboard_config_load_from_x_current (&priv->kbd_cfg,
						  xklrec);

	gkbd_indicator_config_free_image_filenames (&priv->ind_cfg);
	gkbd_indicator_config_load_image_filenames (&priv->ind_cfg,
						    &priv->kbd_cfg);

	g_strfreev (priv->full_group_names);
	priv->full_group_names = NULL;

	g_strfreev (priv->short_group_names);
	priv->short_group_names = NULL;

	gkbd_configuration_load_group_names (configuration,
				 	     xklrec);

	g_signal_emit (configuration,
		       signals[SIGNAL_CHANGED],
		       0);

	g_object_unref (G_OBJECT (xklrec));
}

/* Should be called once for all applets */
static void
gkbd_configuration_state_callback (XklEngine * engine,
				   XklEngineStateChange changeType,
			    	   gint group, gboolean restore,
				   GkbdConfiguration * configuration)
{
	xkl_debug (150, "group is now %d, restore: %d\n", group, restore);

	if (changeType == GROUP_CHANGED) {
		g_signal_emit (configuration,
			       signals[SIGNAL_GROUP_CHANGED], 0,
			       group);
	}
}

static void
gkbd_configuration_init (GkbdConfiguration *configuration)
{
	GkbdConfigurationPrivate *priv;
	XklConfigRec *xklrec = xkl_config_rec_new ();

	priv = GKBD_CONFIGURATION_GET_PRIVATE (configuration);
	configuration->priv = priv;

	priv->engine = xkl_engine_get_instance (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
	if (priv->engine == NULL) {
		xkl_debug (0, "Libxklavier initialization error");
		return;
	}

	priv->state_changed_handler =
	    g_signal_connect (priv->engine, "X-state-changed",
			      G_CALLBACK (gkbd_configuration_state_callback),
			      configuration);
	priv->config_changed_handler =
	    g_signal_connect (priv->engine, "X-config-changed",
			      G_CALLBACK (gkbd_configuration_kbd_cfg_callback),
			      configuration);

	gkbd_desktop_config_init (&priv->cfg, priv->engine);
	gkbd_keyboard_config_init (&priv->kbd_cfg, priv->engine);
	gkbd_indicator_config_init (&priv->ind_cfg, priv->engine);

	gkbd_desktop_config_load (&priv->cfg);
	gkbd_desktop_config_activate (&priv->cfg);

	priv->registry = xkl_config_registry_get_instance (priv->engine);
	xkl_config_registry_load (priv->registry,
				  priv->cfg.load_extra_items);

	gkbd_keyboard_config_load_from_x_current (&priv->kbd_cfg,
						  xklrec);

	gkbd_indicator_config_load (&priv->ind_cfg);

	gkbd_indicator_config_load_image_filenames (&priv->ind_cfg,
						    &priv->kbd_cfg);

	gkbd_indicator_config_activate (&priv->ind_cfg);

	gkbd_configuration_load_group_names (configuration,
					     xklrec);
	g_object_unref (G_OBJECT (xklrec));

	gkbd_desktop_config_start_listen (&priv->cfg,
					  G_CALLBACK (gkbd_configuration_cfg_changed),
					  configuration);
	gkbd_indicator_config_start_listen (&priv->ind_cfg,
					    G_CALLBACK (gkbd_configuration_ind_cfg_changed),
					    configuration);
	xkl_engine_start_listen (priv->engine,
				 XKLL_TRACK_KEYBOARD_STATE);

	xkl_debug (100, "Initiating the widget startup process for %p\n",
		   configuration);
}

static void
gkbd_configuration_finalize (GObject * obj)
{
	GkbdConfiguration *configuration = GKBD_CONFIGURATION (obj);
	GkbdConfigurationPrivate *priv = configuration->priv;

	xkl_debug (100,
		   "Starting the gnome-kbd-configuration widget shutdown process for %p\n",
		   configuration);

	xkl_engine_stop_listen (priv->engine,
				XKLL_TRACK_KEYBOARD_STATE);

	gkbd_desktop_config_stop_listen (&priv->cfg);
	gkbd_indicator_config_stop_listen (&priv->ind_cfg);

	gkbd_indicator_config_term (&priv->ind_cfg);
	gkbd_keyboard_config_term (&priv->kbd_cfg);
	gkbd_desktop_config_term (&priv->cfg);

	if (g_signal_handler_is_connected (priv->engine,
					   priv->state_changed_handler)) {
		g_signal_handler_disconnect (priv->engine,
					     priv->state_changed_handler);
		priv->state_changed_handler = 0;
	}
	if (g_signal_handler_is_connected (priv->engine,
					   priv->config_changed_handler)) {
		g_signal_handler_disconnect (priv->engine,
					     priv->config_changed_handler);
		priv->config_changed_handler = 0;
	}

	g_object_unref (priv->registry);
	priv->registry = NULL;
	g_object_unref (priv->engine);
	priv->engine = NULL;

	G_OBJECT_CLASS (gkbd_configuration_parent_class)->finalize (obj);
}

static void
gkbd_configuration_class_init (GkbdConfigurationClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* Initing vtable */
	object_class->finalize = gkbd_configuration_finalize;

	/* Signals */
	signals[SIGNAL_CHANGED] = g_signal_new ("changed",
						GKBD_TYPE_CONFIGURATION,
						G_SIGNAL_RUN_LAST,
						0,
						NULL, NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);
	signals[SIGNAL_GROUP_CHANGED] = g_signal_new ("group-changed",
						      GKBD_TYPE_CONFIGURATION,
						      G_SIGNAL_RUN_LAST,
						      0,
						      NULL, NULL,
						      g_cclosure_marshal_VOID__INT,
						      G_TYPE_NONE,
						      1,
						      G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GkbdConfigurationPrivate));
}

GkbdConfiguration *
gkbd_configuration_get (void)
{
	static gpointer instance = NULL;

	if (!instance) {
		instance = g_object_new (GKBD_TYPE_CONFIGURATION, NULL);
		g_object_add_weak_pointer (instance, &instance);
	} else {
		g_object_ref (instance);
	}

	return instance;
}

XklEngine *
gkbd_configuration_get_xkl_engine (GkbdConfiguration *configuration)
{
	return configuration->priv->engine;
}

const char * const *
gkbd_configuration_get_group_names (GkbdConfiguration *configuration)
{
	return (const char * const *)configuration->priv->full_group_names;
}

const char * const *
gkbd_configuration_get_short_group_names (GkbdConfiguration *configuration)
{
	return (const char * const *)configuration->priv->short_group_names;
}
