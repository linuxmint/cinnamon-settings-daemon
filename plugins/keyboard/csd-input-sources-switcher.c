/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

#include <X11/XKBlib.h>

#include "csd-enums.h"
#include "csd-keygrab.h"

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.cinnamon.desktop.input-sources"
#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"

#define CSD_KEYBOARD_DIR "org.cinnamon.settings-daemon.peripherals.keyboard"
#define KEY_SWITCHER "input-sources-switcher"

static GSettings *input_sources_settings;

static Key *the_keys = NULL;
static guint n_keys = 0;

static guint master_keyboard_id = 0;

static gboolean includes_caps = FALSE;

static void
do_switch (void)
{
  GVariant *sources;
  gint i, n;

  /* FIXME: this is racy with the g-s-d media-keys plugin. Instead we
     should have a DBus API on g-s-d and poke it from here.*/
  sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);

  n = g_variant_n_children (sources);
  if (n < 2)
    goto out;

  i = g_settings_get_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE) + 1;
  if (i >= n)
    i = 0;

  g_settings_set_uint (input_sources_settings, KEY_CURRENT_INPUT_SOURCE, i);

 out:
  g_variant_unref (sources);
}

static void
init_keys (void)
{
  GSettings *settings;

  settings = g_settings_new (CSD_KEYBOARD_DIR);

  switch (g_settings_get_enum (settings, KEY_SWITCHER))
    {
    case CSD_INPUT_SOURCES_SWITCHER_SHIFT_L:
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_L:
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Alt_L;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_L:
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Control_L;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_SHIFT_R:
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_R;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_R:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Alt_R;
      the_keys[0].state = 0;
      the_keys[1].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[1].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_R:
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Control_R;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_SHIFT_L:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Alt_L;
      the_keys[1].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_SHIFT_R:
      n_keys = 4;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_R;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Alt_R;
      the_keys[1].state = GDK_SHIFT_MASK;
      the_keys[2].keysym = GDK_KEY_Shift_R;
      the_keys[2].state = GDK_MOD5_MASK;
      the_keys[3].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[3].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_SHIFT_L:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = GDK_CONTROL_MASK;
      the_keys[1].keysym = GDK_KEY_Control_L;
      the_keys[1].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_SHIFT_R:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_R;
      the_keys[0].state = GDK_CONTROL_MASK;
      the_keys[1].keysym = GDK_KEY_Control_R;
      the_keys[1].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_SHIFT_L_SHIFT_R:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = GDK_SHIFT_MASK;
      the_keys[1].keysym = GDK_KEY_Shift_R;
      the_keys[1].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_L_ALT_R:
      n_keys = 4;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Alt_L;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Alt_R;
      the_keys[1].state = GDK_MOD1_MASK;
      the_keys[2].keysym = GDK_KEY_Alt_L;
      the_keys[2].state = GDK_MOD5_MASK;
      the_keys[3].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[3].state = GDK_MOD1_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_L_CTRL_R:
      n_keys = 2;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Control_L;
      the_keys[0].state = GDK_CONTROL_MASK;
      the_keys[1].keysym = GDK_KEY_Control_R;
      the_keys[1].state = GDK_CONTROL_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_SHIFT:
      n_keys = 7;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Shift_L;
      the_keys[1].state = GDK_MOD5_MASK;
      the_keys[2].keysym = GDK_KEY_Shift_R;
      the_keys[2].state = GDK_MOD1_MASK;
      the_keys[3].keysym = GDK_KEY_Shift_R;
      the_keys[3].state = GDK_MOD5_MASK;
      the_keys[4].keysym = GDK_KEY_Alt_L;
      the_keys[4].state = GDK_SHIFT_MASK;
      the_keys[5].keysym = GDK_KEY_Alt_R;
      the_keys[5].state = GDK_SHIFT_MASK;
      the_keys[6].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[6].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_SHIFT:
      n_keys = 4;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Shift_L;
      the_keys[0].state = GDK_CONTROL_MASK;
      the_keys[1].keysym = GDK_KEY_Shift_R;
      the_keys[1].state = GDK_CONTROL_MASK;
      the_keys[2].keysym = GDK_KEY_Control_L;
      the_keys[2].state = GDK_SHIFT_MASK;
      the_keys[3].keysym = GDK_KEY_Control_R;
      the_keys[3].state = GDK_SHIFT_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_CTRL:
      n_keys = 7;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Control_L;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Control_L;
      the_keys[1].state = GDK_MOD5_MASK;
      the_keys[2].keysym = GDK_KEY_Control_R;
      the_keys[2].state = GDK_MOD1_MASK;
      the_keys[3].keysym = GDK_KEY_Control_R;
      the_keys[3].state = GDK_MOD5_MASK;
      the_keys[4].keysym = GDK_KEY_Alt_L;
      the_keys[4].state = GDK_CONTROL_MASK;
      the_keys[5].keysym = GDK_KEY_Alt_R;
      the_keys[5].state = GDK_CONTROL_MASK;
      the_keys[6].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[6].state = GDK_CONTROL_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CAPS:
      includes_caps = TRUE;
      n_keys = 1;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Caps_Lock;
      the_keys[0].state = 0;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_SHIFT_CAPS:
      includes_caps = TRUE;
      n_keys = 3;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Caps_Lock;
      the_keys[0].state = GDK_SHIFT_MASK;
      the_keys[1].keysym = GDK_KEY_Shift_L;
      the_keys[1].state = GDK_LOCK_MASK;
      the_keys[2].keysym = GDK_KEY_Shift_R;
      the_keys[2].state = GDK_LOCK_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_ALT_CAPS:
      includes_caps = TRUE;
      n_keys = 5;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Caps_Lock;
      the_keys[0].state = GDK_MOD1_MASK;
      the_keys[1].keysym = GDK_KEY_Caps_Lock;
      the_keys[1].state = GDK_MOD5_MASK;
      the_keys[2].keysym = GDK_KEY_Alt_L;
      the_keys[2].state = GDK_LOCK_MASK;
      the_keys[3].keysym = GDK_KEY_Alt_R;
      the_keys[3].state = GDK_LOCK_MASK;
      the_keys[4].keysym = GDK_KEY_ISO_Level3_Shift;
      the_keys[4].state = GDK_LOCK_MASK;
      break;

    case CSD_INPUT_SOURCES_SWITCHER_CTRL_CAPS:
      includes_caps = TRUE;
      n_keys = 3;
      the_keys = g_new0 (Key, n_keys);

      the_keys[0].keysym = GDK_KEY_Caps_Lock;
      the_keys[0].state = GDK_CONTROL_MASK;
      the_keys[1].keysym = GDK_KEY_Control_L;
      the_keys[1].state = GDK_LOCK_MASK;
      the_keys[2].keysym = GDK_KEY_Control_R;
      the_keys[2].state = GDK_LOCK_MASK;
      break;
    }

  g_object_unref (settings);
}

static void
free_keys (void)
{
  gint i;

  for (i = 0; i < n_keys; ++i)
    g_free (the_keys[i].keycodes);

  g_free (the_keys);
}

static gboolean
match_caps_locked (const Key     *key,
                   XIDeviceEvent *xev)
{
  if (key->state & xev->mods.effective &&
      key->keysym == XkbKeycodeToKeysym (xev->display, xev->detail, 0, 0))
    return TRUE;

  return FALSE;
}

static gboolean
match_modifier (const Key *key,
                XIEvent   *xiev)
{
  Key meta;

  /* When the grab is established with Caps Lock as the modifier
     (i.e. key->state == GDK_LOCK_MASK) we can't use match_xi2_key()
     as this modifier is black listed there, so we do the match
     ourselves. */
  if (key->state == GDK_LOCK_MASK)
    return match_caps_locked (key, (XIDeviceEvent *) xiev);

  meta = *key;

  switch (key->keysym)
    {
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
      if (xiev->evtype == XI_KeyRelease)
        meta.state |= GDK_SHIFT_MASK;
      break;

    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
      if (xiev->evtype == XI_KeyRelease)
        meta.state |= GDK_CONTROL_MASK;
      break;

    case GDK_KEY_ISO_Level3_Shift:
      if (xiev->evtype == XI_KeyRelease)
        meta.state |= GDK_MOD5_MASK;
      break;

    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
      if (key->state == GDK_SHIFT_MASK)
        meta.keysym = key->keysym == GDK_KEY_Alt_L ? GDK_KEY_Meta_L : GDK_KEY_Meta_R;

      if (xiev->evtype == XI_KeyRelease)
        meta.state |= GDK_MOD1_MASK;
      break;
    }

  return match_xi2_key (&meta, (XIDeviceEvent *) xiev);
}

static gboolean
matches_key (XIEvent *xiev)
{
  gint i;

  for (i = 0; i < n_keys; ++i)
    if (match_modifier (&the_keys[i], xiev))
      return TRUE;

  return FALSE;
}

/* Owen magic, ported to XI2 */
static GdkFilterReturn
filter (XEvent   *xevent,
        GdkEvent *event,
        gpointer  data)
{
  XIEvent *xiev;
  XIDeviceEvent *xev;
  XIGrabModifiers mods;
  XIEventMask evmask;
  unsigned char mask[(XI_LASTEVENT + 7)/8] = { 0 };

  if (xevent->type != GenericEvent)
    return GDK_FILTER_CONTINUE;

  xiev = (XIEvent *) xevent->xcookie.data;

  if (xiev->evtype != XI_ButtonPress &&
      xiev->evtype != XI_KeyPress &&
      xiev->evtype != XI_KeyRelease)
    return GDK_FILTER_CONTINUE;

  xev = (XIDeviceEvent *) xiev;

  mods.modifiers = XIAnyModifier;
  XISetMask (mask, XI_ButtonPress);
  evmask.deviceid = XIAllMasterDevices;
  evmask.mask_len = sizeof (mask);
  evmask.mask = mask;

  if (xiev->evtype != XI_ButtonPress &&
      matches_key (xiev))
    {
      if (xiev->evtype == XI_KeyPress)
        {
          if (includes_caps)
            {
              do_switch ();
              XIUngrabDevice (xev->display,
                              master_keyboard_id,
                              xev->time);
              XkbLockModifiers (xev->display, XkbUseCoreKbd, LockMask, 0);
            }
          else
            {
              XIAllowEvents (xev->display,
                             xev->deviceid,
                             XISyncDevice,
                             xev->time);
              XIGrabButton (xev->display,
                            XIAllMasterDevices,
                            XIAnyButton,
                            xev->root,
                            None,
                            GrabModeSync,
                            GrabModeSync,
                            False,
                            &evmask,
                            1,
                            &mods);
            }
        }
      else
        {
          do_switch ();
          XIUngrabDevice (xev->display,
                          master_keyboard_id,
                          xev->time);
          XIUngrabButton (xev->display,
                          XIAllMasterDevices,
                          XIAnyButton,
                          xev->root,
                          1,
                          &mods);
        }
    }
  else
    {
      XIAllowEvents (xev->display,
                     xev->deviceid,
                     XIReplayDevice,
                     xev->time);
      XIUngrabDevice (xev->display,
                      master_keyboard_id,
                      xev->time);
      XIUngrabButton (xev->display,
                      XIAllMasterDevices,
                      XIAnyButton,
                      xev->root,
                      1,
                      &mods);
    }

  return GDK_FILTER_CONTINUE;
}

static void
grab_key (Key        *key,
          GdkDisplay *display,
          GSList     *screens)
{
  GdkKeymapKey *keys;
  gboolean has_entries;
  GArray *keycodes;
  gint n, i;

  has_entries = gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
                                                   key->keysym,
                                                   &keys,
                                                   &n);
  if (!has_entries)
    return;

  keycodes = g_array_sized_new (TRUE, TRUE, sizeof (guint), n);
  for (i = 0; i < n; ++i)
    g_array_append_val (keycodes, keys[i].keycode);

  key->keycodes = (guint *) g_array_free (keycodes, FALSE);

  gdk_x11_display_error_trap_push (display);

  grab_key_unsafe (key, CSD_KEYGRAB_ALLOW_UNMODIFIED | CSD_KEYGRAB_SYNCHRONOUS, screens);

  gdk_x11_display_error_trap_pop_ignored (display);

  g_free (keys);
}

static guint
get_master_keyboard_id (GdkDisplay *display)
{
  XIDeviceInfo *info;
  guint id;
  int i, n;

  id = 0;
  info = XIQueryDevice (GDK_DISPLAY_XDISPLAY (display), XIAllMasterDevices, &n);
  for (i = 0; i < n; ++i)
    if (info[i].use == XIMasterKeyboard && info[i].enabled)
      {
        id = info[i].deviceid;
        break;
      }
  XIFreeDeviceInfo (info);

  return id;
}

static void
set_input_sources_switcher (void)
{
  GdkDisplay *display;
  gint n_screens;
  GSList *screens, *l;
  gint i;

  display = gdk_display_get_default ();
  n_screens = gdk_display_get_n_screens (display);
  screens = NULL;
  for (i = 0; i < n_screens; ++i)
    screens = g_slist_prepend (screens, gdk_display_get_screen (display, i));

  for (i = 0; i < n_keys; ++i)
    grab_key (&the_keys[i], display, screens);

  for (l = screens; l; l = l->next)
    {
      GdkScreen *screen;

      screen = (GdkScreen *) l->data;
      gdk_window_add_filter (gdk_screen_get_root_window (screen),
                             (GdkFilterFunc) filter,
                             screen);
    }

  g_slist_free (screens);

  master_keyboard_id = get_master_keyboard_id (display);
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  init_keys ();
  if (n_keys == 0)
    {
      g_warning ("No shortcut defined, exiting");
      return -1;
    }
  input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);

  set_input_sources_switcher ();
  gtk_main ();

  g_object_unref (input_sources_settings);
  free_keys ();

  return 0;
}
