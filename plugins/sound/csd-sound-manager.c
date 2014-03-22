/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Lennart Poettering <lennart@poettering.net>
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
#include <signal.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

#include "csd-sound-manager.h"
#include "cinnamon-settings-profile.h"

#define CSD_SOUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_SOUND_MANAGER, CsdSoundManagerPrivate))

struct CsdSoundManagerPrivate
{
        GSettings *settings;
        GList     *monitors;
        guint      timeout;
};

static void csd_sound_manager_class_init (CsdSoundManagerClass *klass);
static void csd_sound_manager_init (CsdSoundManager *sound_manager);
static void csd_sound_manager_finalize (GObject *object);

G_DEFINE_TYPE (CsdSoundManager, csd_sound_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
sample_info_cb (pa_context *c, const pa_sample_info *i, int eol, void *userdata)
{
        pa_operation *o;

        if (!i)
                return;

        g_debug ("Found sample %s", i->name);

        /* We only flush those samples which have an XDG sound name
         * attached, because only those originate from themeing  */
        if (!(pa_proplist_gets (i->proplist, PA_PROP_EVENT_ID)))
                return;

        g_debug ("Dropping sample %s from cache", i->name);

        if (!(o = pa_context_remove_sample (c, i->name, NULL, NULL))) {
                g_debug ("pa_context_remove_sample (): %s", pa_strerror (pa_context_errno (c)));
                return;
        }

        pa_operation_unref (o);

        /* We won't wait until the operation is actually executed to
         * speed things up a bit.*/
}

static void
flush_cache (void)
{
        pa_mainloop *ml = NULL;
        pa_context *c = NULL;
        pa_proplist *pl = NULL;
        pa_operation *o = NULL;

        g_debug ("Flushing sample cache");

        if (!(ml = pa_mainloop_new ())) {
                g_debug ("Failed to allocate pa_mainloop");
                goto fail;
        }

        if (!(pl = pa_proplist_new ())) {
                g_debug ("Failed to allocate pa_proplist");
                goto fail;
        }

        pa_proplist_sets (pl, PA_PROP_APPLICATION_NAME, PACKAGE_NAME);
        pa_proplist_sets (pl, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);
        pa_proplist_sets (pl, PA_PROP_APPLICATION_ID, "org.cinnamon.SettingsDaemon");

        if (!(c = pa_context_new_with_proplist (pa_mainloop_get_api (ml), PACKAGE_NAME, pl))) {
                g_debug ("Failed to allocate pa_context");
                goto fail;
        }

        pa_proplist_free (pl);
        pl = NULL;

        if (pa_context_connect (c, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
                g_debug ("pa_context_connect(): %s", pa_strerror (pa_context_errno (c)));
                goto fail;
        }

        /* Wait until the connection is established */
        while (pa_context_get_state (c) != PA_CONTEXT_READY) {

                if (!PA_CONTEXT_IS_GOOD (pa_context_get_state (c))) {
                        g_debug ("Connection failed: %s", pa_strerror (pa_context_errno (c)));
                        goto fail;
                }

                if (pa_mainloop_iterate (ml, TRUE, NULL) < 0) {
                        g_debug ("pa_mainloop_iterate() failed");
                        goto fail;
                }
        }

        /* Enumerate all cached samples */
        if (!(o = pa_context_get_sample_info_list (c, sample_info_cb, NULL))) {
                g_debug ("pa_context_get_sample_info_list(): %s", pa_strerror (pa_context_errno (c)));
                goto fail;
        }

        /* Wait until our operation is finished and there's nothing
         * more queued to send to the server */
        while (pa_operation_get_state (o) == PA_OPERATION_RUNNING || pa_context_is_pending (c)) {

                if (!PA_CONTEXT_IS_GOOD (pa_context_get_state (c))) {
                        g_debug ("Connection failed: %s", pa_strerror (pa_context_errno (c)));
                        goto fail;
                }

                if (pa_mainloop_iterate (ml, TRUE, NULL) < 0) {
                        g_debug ("pa_mainloop_iterate() failed");
                        goto fail;
                }
        }

        g_debug ("Sample cache flushed");

fail:
        if (o) {
                pa_operation_cancel (o);
                pa_operation_unref (o);
        }

        if (c) {
                pa_context_disconnect (c);
                pa_context_unref (c);
        }

        if (pl)
                pa_proplist_free (pl);

        if (ml)
                pa_mainloop_free (ml);
}

static gboolean
flush_cb (CsdSoundManager *manager)
{
        flush_cache ();
        manager->priv->timeout = 0;
        return FALSE;
}

static void
trigger_flush (CsdSoundManager *manager)
{

        if (manager->priv->timeout) {
            g_source_remove (manager->priv->timeout);
            manager->priv->timeout = 0;
        }

        /* We delay the flushing a bit so that we can coalesce
         * multiple changes into a single cache flush */
        manager->priv->timeout = g_timeout_add (500, (GSourceFunc) flush_cb, manager);
}

static void
settings_changed_cb (GSettings       *settings,
		     const char      *key,
		     CsdSoundManager *manager)
{
        trigger_flush (manager);
}

static void
register_config_callback (CsdSoundManager *manager)
{
	manager->priv->settings = g_settings_new ("org.cinnamon.desktop.sound");
	g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
			  G_CALLBACK (settings_changed_cb), manager);
}

static void
file_monitor_changed_cb (GFileMonitor *monitor,
                         GFile *file,
                         GFile *other_file,
                         GFileMonitorEvent event,
                         CsdSoundManager *manager)
{
        g_debug ("Theme dir changed");
        trigger_flush (manager);
}

static gboolean
register_directory_callback (CsdSoundManager *manager,
                             const char *path,
                             GError **error)
{
        GFile *f;
        GFileMonitor *m;
        gboolean succ = FALSE;

        g_debug ("Registering directory monitor for %s", path);

        f = g_file_new_for_path (path);

        m = g_file_monitor_directory (f, 0, NULL, error);

        if (m != NULL) {
                g_signal_connect (m, "changed", G_CALLBACK (file_monitor_changed_cb), manager);

                manager->priv->monitors = g_list_prepend (manager->priv->monitors, m);

                succ = TRUE;
        }

        g_object_unref (f);

        return succ;
}

gboolean
csd_sound_manager_start (CsdSoundManager *manager,
                         GError **error)
{
        char *p, **ps, **k;
        const char *env, *dd;

        g_debug ("Starting sound manager");
        cinnamon_settings_profile_start (NULL);

        /* We listen for change of the selected theme ... */
        register_config_callback (manager);

        /* ... and we listen to changes of the theme base directories
         * in $HOME ...*/

        if ((env = g_getenv ("XDG_DATA_HOME")) && *env == '/')
                p = g_build_filename (env, "sounds", NULL);
        else if (((env = g_getenv ("HOME")) && *env == '/') || (env = g_get_home_dir ()))
                p = g_build_filename (env, ".local", "share", "sounds", NULL);
        else
                p = NULL;

        if (p) {
                register_directory_callback (manager, p, NULL);
                g_free (p);
        }

        /* ... and globally. */
        if (!(dd = g_getenv ("XDG_DATA_DIRS")) || *dd == 0)
                dd = "/usr/local/share:/usr/share";

        ps = g_strsplit (dd, ":", 0);

        for (k = ps; *k; ++k)
                register_directory_callback (manager, *k, NULL);

        g_strfreev (ps);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_sound_manager_stop (CsdSoundManager *manager)
{
        g_debug ("Stopping sound manager");

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->timeout) {
                g_source_remove (manager->priv->timeout);
                manager->priv->timeout = 0;
        }

        while (manager->priv->monitors) {
                g_file_monitor_cancel (G_FILE_MONITOR (manager->priv->monitors->data));
                g_object_unref (manager->priv->monitors->data);
                manager->priv->monitors = g_list_delete_link (manager->priv->monitors, manager->priv->monitors);
        }
}

static GObject *
csd_sound_manager_constructor (
                GType type,
                guint n_construct_properties,
                GObjectConstructParam *construct_properties)
{
        CsdSoundManager *m;

        m = CSD_SOUND_MANAGER (G_OBJECT_CLASS (csd_sound_manager_parent_class)->constructor (
                                                           type,
                                                           n_construct_properties,
                                                           construct_properties));

        return G_OBJECT (m);
}

static void
csd_sound_manager_dispose (GObject *object)
{
        CsdSoundManager *manager;

        manager = CSD_SOUND_MANAGER (object);

        csd_sound_manager_stop (manager);

        G_OBJECT_CLASS (csd_sound_manager_parent_class)->dispose (object);
}

static void
csd_sound_manager_class_init (CsdSoundManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_sound_manager_constructor;
        object_class->dispose = csd_sound_manager_dispose;
        object_class->finalize = csd_sound_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdSoundManagerPrivate));
}

static void
csd_sound_manager_init (CsdSoundManager *manager)
{
        manager->priv = CSD_SOUND_MANAGER_GET_PRIVATE (manager);
}

static void
csd_sound_manager_finalize (GObject *object)
{
        CsdSoundManager *sound_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_SOUND_MANAGER (object));

        sound_manager = CSD_SOUND_MANAGER (object);

        g_return_if_fail (sound_manager->priv);

        G_OBJECT_CLASS (csd_sound_manager_parent_class)->finalize (object);
}

CsdSoundManager *
csd_sound_manager_new (void)
{
        if (manager_object) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_SOUND_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object, (gpointer *) &manager_object);
        }

        return CSD_SOUND_MANAGER (manager_object);
}
