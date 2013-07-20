/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 - Paolo Maggi
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
 * Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "config.h"

#include "cinnamon-settings-module.h"

#include <gmodule.h>

typedef struct _CinnamonSettingsModuleClass CinnamonSettingsModuleClass;

struct _CinnamonSettingsModuleClass
{
        GTypeModuleClass parent_class;
};

struct _CinnamonSettingsModule
{
        GTypeModule parent_instance;

        GModule    *library;

        char       *path;
        GType       type;
};

typedef GType (*CinnamonSettingsModuleRegisterFunc) (GTypeModule *);

G_DEFINE_TYPE (CinnamonSettingsModule, cinnamon_settings_module, G_TYPE_TYPE_MODULE)

static gboolean
cinnamon_settings_module_load (GTypeModule *gmodule)
{
        CinnamonSettingsModule            *module;
        CinnamonSettingsModuleRegisterFunc register_func;
        gboolean                        res;

        module = CINNAMON_SETTINGS_MODULE (gmodule);

        g_debug ("Loading %s", module->path);

        module->library = g_module_open (module->path, 0);

        if (module->library == NULL) {
                g_warning ("%s", g_module_error ());

                return FALSE;
        }

        /* extract symbols from the lib */
        res = g_module_symbol (module->library, "register_cinnamon_settings_plugin", (void *) &register_func);
        if (! res) {
                g_warning ("%s", g_module_error ());
                g_module_close (module->library);

                return FALSE;
        }

        g_assert (register_func);

        module->type = register_func (gmodule);

        if (module->type == 0) {
                g_warning ("Invalid gnome settings plugin in module %s", module->path);
                return FALSE;
        }

        return TRUE;
}

static void
cinnamon_settings_module_unload (GTypeModule *gmodule)
{
        CinnamonSettingsModule *module = CINNAMON_SETTINGS_MODULE (gmodule);

        g_debug ("Unloading %s", module->path);

        g_module_close (module->library);

        module->library = NULL;
        module->type = 0;
}

const gchar *
cinnamon_settings_module_get_path (CinnamonSettingsModule *module)
{
        g_return_val_if_fail (CINNAMON_IS_SETTINGS_MODULE (module), NULL);

        return module->path;
}

GObject *
cinnamon_settings_module_new_object (CinnamonSettingsModule *module)
{
        g_debug ("Creating object of type %s", g_type_name (module->type));

        if (module->type == 0) {
                return NULL;
        }

        return g_object_new (module->type, NULL);
}

static void
cinnamon_settings_module_init (CinnamonSettingsModule *module)
{
        g_debug ("CinnamonSettingsModule %p initialising", module);
}

static void
cinnamon_settings_module_finalize (GObject *object)
{
        CinnamonSettingsModule *module = CINNAMON_SETTINGS_MODULE (object);

        g_debug ("CinnamonSettingsModule %p finalizing", module);

        g_free (module->path);

        G_OBJECT_CLASS (cinnamon_settings_module_parent_class)->finalize (object);
}

static void
cinnamon_settings_module_class_init (CinnamonSettingsModuleClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

        object_class->finalize = cinnamon_settings_module_finalize;

        module_class->load = cinnamon_settings_module_load;
        module_class->unload = cinnamon_settings_module_unload;
}

CinnamonSettingsModule *
cinnamon_settings_module_new (const char *path)
{
        CinnamonSettingsModule *result;

        if (path == NULL || path[0] == '\0') {
                return NULL;
        }

        result = g_object_new (CINNAMON_TYPE_SETTINGS_MODULE, NULL);

        g_type_module_set_name (G_TYPE_MODULE (result), path);
        result->path = g_strdup (path);

        return result;
}
