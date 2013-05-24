/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <X11/Xmd.h>		/* For CARD16 */

#include "xsettings-manager.h"

#define XSETTINGS_VARIANT_TYPE_COLOR  (G_VARIANT_TYPE ("(qqqq)"))

struct _XSettingsManager
{
  Display *display;
  int screen;

  Window window;
  Atom manager_atom;
  Atom selection_atom;
  Atom xsettings_atom;

  XSettingsTerminateFunc terminate;
  void *cb_data;

  GHashTable *settings;
  unsigned long serial;

  GVariant *overrides;
};

typedef struct 
{
  Window window;
  Atom timestamp_prop_atom;
} TimeStampInfo;

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  TimeStampInfo *info = (TimeStampInfo *)arg;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == info->window &&
      xevent->xproperty.atom == info->timestamp_prop_atom)
    return True;

  return False;
}

/**
 * get_server_time:
 * @display: display from which to get the time
 * @window: a #Window, used for communication with the server.
 *          The window must have PropertyChangeMask in its
 *          events mask or a hang will result.
 * 
 * Routine to get the current X server time stamp. 
 * 
 * Return value: the time stamp.
 **/
static Time
get_server_time (Display *display,
		 Window   window)
{
  unsigned char c = 'a';
  XEvent xevent;
  TimeStampInfo info;

  info.timestamp_prop_atom = XInternAtom  (display, "_TIMESTAMP_PROP", False);
  info.window = window;

  XChangeProperty (display, window,
		   info.timestamp_prop_atom, info.timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (display, &xevent,
	    timestamp_predicate, (XPointer)&info);

  return xevent.xproperty.time;
}

Bool
xsettings_manager_check_running (Display *display,
				 int      screen)
{
  char buffer[256];
  Atom selection_atom;
  
  sprintf(buffer, "_XSETTINGS_S%d", screen);
  selection_atom = XInternAtom (display, buffer, False);

  if (XGetSelectionOwner (display, selection_atom))
    return True;
  else
    return False;
}

XSettingsManager *
xsettings_manager_new (Display                *display,
		       int                     screen,
		       XSettingsTerminateFunc  terminate,
		       void                   *cb_data)
{
  XSettingsManager *manager;
  Time timestamp;
  XClientMessageEvent xev;

  char buffer[256];

  manager = g_slice_new (XSettingsManager);

  manager->display = display;
  manager->screen = screen;

  sprintf(buffer, "_XSETTINGS_S%d", screen);
  manager->selection_atom = XInternAtom (display, buffer, False);
  manager->xsettings_atom = XInternAtom (display, "_XSETTINGS_SETTINGS", False);
  manager->manager_atom = XInternAtom (display, "MANAGER", False);

  manager->terminate = terminate;
  manager->cb_data = cb_data;

  manager->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) xsettings_setting_free);
  manager->serial = 0;
  manager->overrides = NULL;

  manager->window = XCreateSimpleWindow (display,
					 RootWindow (display, screen),
					 0, 0, 10, 10, 0,
					 WhitePixel (display, screen),
					 WhitePixel (display, screen));

  XSelectInput (display, manager->window, PropertyChangeMask);
  timestamp = get_server_time (display, manager->window);

  XSetSelectionOwner (display, manager->selection_atom,
		      manager->window, timestamp);

  /* Check to see if we managed to claim the selection. If not,
   * we treat it as if we got it then immediately lost it
   */

  if (XGetSelectionOwner (display, manager->selection_atom) ==
      manager->window)
    {
      xev.type = ClientMessage;
      xev.window = RootWindow (display, screen);
      xev.message_type = manager->manager_atom;
      xev.format = 32;
      xev.data.l[0] = timestamp;
      xev.data.l[1] = manager->selection_atom;
      xev.data.l[2] = manager->window;
      xev.data.l[3] = 0;	/* manager specific data */
      xev.data.l[4] = 0;	/* manager specific data */
      
      XSendEvent (display, RootWindow (display, screen),
		  False, StructureNotifyMask, (XEvent *)&xev);
    }
  else
    {
      manager->terminate (manager->cb_data);
    }
  
  return manager;
}

void
xsettings_manager_destroy (XSettingsManager *manager)
{
  XDestroyWindow (manager->display, manager->window);

  g_hash_table_unref (manager->settings);

  g_slice_free (XSettingsManager, manager);
}

static void
xsettings_manager_set_setting (XSettingsManager *manager,
                               const gchar      *name,
                               gint              tier,
                               GVariant         *value)
{
  XSettingsSetting *setting;

  setting = g_hash_table_lookup (manager->settings, name);

  if (setting == NULL)
    {
      setting = xsettings_setting_new (name);
      setting->last_change_serial = manager->serial;
      g_hash_table_insert (manager->settings, setting->name, setting);
    }

  xsettings_setting_set (setting, tier, value, manager->serial);

  if (xsettings_setting_get (setting) == NULL)
    g_hash_table_remove (manager->settings, name);
}

void
xsettings_manager_set_int (XSettingsManager *manager,
			   const char       *name,
			   int               value)
{
  xsettings_manager_set_setting (manager, name, 0, g_variant_new_int32 (value));
}

void
xsettings_manager_set_string (XSettingsManager *manager,
			      const char       *name,
			      const char       *value)
{
  xsettings_manager_set_setting (manager, name, 0, g_variant_new_string (value));
}

void
xsettings_manager_set_color (XSettingsManager *manager,
			     const char       *name,
			     XSettingsColor   *value)
{
  GVariant *tmp;

  tmp = g_variant_new ("(qqqq)", value->red, value->green, value->blue, value->alpha);
  g_assert (g_variant_is_of_type (tmp, XSETTINGS_VARIANT_TYPE_COLOR)); /* paranoia... */
  xsettings_manager_set_setting (manager, name, 0, tmp);
}

void
xsettings_manager_delete_setting (XSettingsManager *manager,
                                  const char       *name)
{
  xsettings_manager_set_setting (manager, name, 0, NULL);
}

static gchar
xsettings_get_typecode (GVariant *value)
{
  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_INT32:
      return XSETTINGS_TYPE_INT;
    case G_VARIANT_CLASS_STRING:
      return XSETTINGS_TYPE_STRING;
    case G_VARIANT_CLASS_TUPLE:
      return XSETTINGS_TYPE_COLOR;
    default:
      g_assert_not_reached ();
    }
}

static void
align_string (GString *string,
              gint     alignment)
{
  /* Adds nul-bytes to the string until its length is an even multiple
   * of the specified alignment requirement.
   */
  while ((string->len % alignment) != 0)
    g_string_append_c (string, '\0');
}

static void
setting_store (XSettingsSetting *setting,
               GString          *buffer)
{
  XSettingsType type;
  GVariant *value;
  guint16 len16;

  value = xsettings_setting_get (setting);

  type = xsettings_get_typecode (value);

  g_string_append_c (buffer, type);
  g_string_append_c (buffer, 0);

  len16 = strlen (setting->name);
  g_string_append_len (buffer, (gchar *) &len16, 2);
  g_string_append (buffer, setting->name);
  align_string (buffer, 4);

  g_string_append_len (buffer, (gchar *) &setting->last_change_serial, 4);

  if (type == XSETTINGS_TYPE_STRING)
    {
      const gchar *string;
      gsize stringlen;
      guint32 len32;

      string = g_variant_get_string (value, &stringlen);
      len32 = stringlen;
      g_string_append_len (buffer, (gchar *) &len32, 4);
      g_string_append (buffer, string);
      align_string (buffer, 4);
    }
  else
    /* GVariant format is the same as XSETTINGS format for the non-string types */
    g_string_append_len (buffer, g_variant_get_data (value), g_variant_get_size (value));
}

void
xsettings_manager_notify (XSettingsManager *manager)
{
  GString *buffer;
  GHashTableIter iter;
  int n_settings;
  gpointer value;

  n_settings = g_hash_table_size (manager->settings);

  buffer = g_string_new (NULL);
  g_string_append_c (buffer, xsettings_byte_order ());
  g_string_append_c (buffer, '\0');
  g_string_append_c (buffer, '\0');
  g_string_append_c (buffer, '\0');

  g_string_append_len (buffer, (gchar *) &manager->serial, 4);
  g_string_append_len (buffer, (gchar *) &n_settings, 4);

  g_hash_table_iter_init (&iter, manager->settings);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    setting_store (value, buffer);

  XChangeProperty (manager->display, manager->window,
                   manager->xsettings_atom, manager->xsettings_atom,
                   8, PropModeReplace, (guchar *) buffer->str, buffer->len);

  g_string_free (buffer, TRUE);
  manager->serial++;
}

void
xsettings_manager_set_overrides (XSettingsManager *manager,
                                 GVariant         *overrides)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_return_if_fail (overrides != NULL && g_variant_is_of_type (overrides, G_VARIANT_TYPE_VARDICT));

  if (manager->overrides)
    {
      /* unset the existing overrides */

      g_variant_iter_init (&iter, manager->overrides);
      while (g_variant_iter_next (&iter, "{&sv}", &key, NULL))
        /* only unset it at this point if it's not in the new list */
        if (!g_variant_lookup (overrides, key, "*", NULL))
          xsettings_manager_set_setting (manager, key, 1, NULL);
      g_variant_unref (manager->overrides);
    }

  /* save this so we can do the unsets next time */
  manager->overrides = g_variant_ref_sink (overrides);

  /* set the new values */
  g_variant_iter_init (&iter, overrides);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      /* only accept recognised types... */
      if (!g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) &&
          !g_variant_is_of_type (value, G_VARIANT_TYPE_INT32) &&
          !g_variant_is_of_type (value, XSETTINGS_VARIANT_TYPE_COLOR))
        continue;

      xsettings_manager_set_setting (manager, key, 1, value);
    }
}
