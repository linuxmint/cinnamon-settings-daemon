/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008 Jens Granseuer <jensgr@gmx.net>
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

#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XKB.h>
#include <gdk/gdkkeysyms.h>

#include "csd-keygrab.h"

/* these are the mods whose combinations are ignored by the keygrabbing code */
static GdkModifierType csd_ignored_mods = 0;

/* these are the ones we actually use for global keys, we always only check
 * for these set */
static GdkModifierType csd_used_mods = 0;

/* Taken from a comment in XF86keysym.h */
#define XF86KEYS_RANGE_MIN 0x10080001
#define XF86KEYS_RANGE_MAX 0x1008FFFF

#define FKEYS_RANGE_MIN GDK_KEY_F1
#define FKEYS_RANGE_MAX GDK_KEY_R15

#define IN_RANGE(x, min, max) (x >= min && x <= max)

static void
setup_modifiers (void)
{
        if (csd_used_mods == 0 || csd_ignored_mods == 0) {
                GdkModifierType dynmods;

                /* default modifiers */
                csd_ignored_mods = \
                        0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK | GDK_HYPER_MASK;
		csd_used_mods = \
                        GDK_SHIFT_MASK | GDK_CONTROL_MASK |\
                        GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK |\
                        GDK_MOD5_MASK | GDK_SUPER_MASK | GDK_META_MASK;

                /* NumLock can be assigned to varying keys so we need to
                 * resolve and ignore it specially */
                dynmods = XkbKeysymToModifiers (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), GDK_KEY_Num_Lock);

                csd_ignored_mods |= dynmods;
                csd_used_mods &= ~dynmods;
	}
}

static void
grab_key_real (guint      keycode,
               GdkWindow *root,
               gboolean   grab,
               gboolean   synchronous,
               XIGrabModifiers *mods,
               int        num_mods)
{
	XIEventMask evmask;
	unsigned char mask[(XI_LASTEVENT + 7)/8];

	memset (mask, 0, sizeof (mask));
	XISetMask (mask, XI_KeyPress);
	XISetMask (mask, XI_KeyRelease);

	evmask.deviceid = XIAllMasterDevices;
	evmask.mask_len = sizeof (mask);
	evmask.mask = mask;

        if (grab) {
                XIGrabKeycode (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               XIAllMasterDevices,
                               keycode,
                               GDK_WINDOW_XID (root),
                               GrabModeAsync,
                               synchronous ? GrabModeSync : GrabModeAsync,
                               False,
                               &evmask,
                               num_mods,
                               mods);
        } else {
                XIUngrabKeycode (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 XIAllMasterDevices,
                                 keycode,
                                 GDK_WINDOW_XID (root),
                                 num_mods,
                                 mods);
        }
}

/* Grab the key. In order to ignore CSD_IGNORED_MODS we need to grab
 * all combinations of the ignored modifiers and those actually used
 * for the binding (if any).
 *
 * inspired by all_combinations from gnome-panel/gnome-panel/global-keys.c
 *
 * This may generate X errors.  The correct way to use this is like:
 *
 *        gdk_x11_display_error_trap_push (gdk_display_get_default ());
 *
 *        grab_key_unsafe (key, grab, screens);
 *
 *        gdk_flush ();
 *        if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()))
 *                g_warning ("Grab failed, another application may already have access to key '%u'",
 *                           key->keycode);
 *
 * This is not done in the function itself, to allow doing multiple grab_key
 * operations with one flush only.
 */
#define N_BITS 32
static void
grab_key_internal (Key             *key,
                   gboolean         grab,
                   CsdKeygrabFlags  flags,
                   GSList          *screens)
{
        int     indexes[N_BITS]; /* indexes of bits we need to flip */
        int     i;
        int     bit;
        int     bits_set_cnt;
        int     uppervalue;
        guint   mask, modifiers;
        GArray *all_mods;
        GSList *l;

        setup_modifiers ();

        mask = csd_ignored_mods & ~key->state & GDK_MODIFIER_MASK;

        /* XGrabKey requires real modifiers, not virtual ones */
        modifiers = key->state;
        gdk_keymap_map_virtual_modifiers (gdk_keymap_get_default (), &modifiers);
        modifiers &= ~(GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK);

        /* If key doesn't have a usable modifier, we don't want
         * to grab it, since the user might lose a useful key.
         *
         * The exception is the XFree86 keys and the Function keys
         * (which are useful to grab without a modifier).
         */
        if (!(flags & CSD_KEYGRAB_ALLOW_UNMODIFIED) &&
            (modifiers & csd_used_mods) == 0 &&
            !IN_RANGE(key->keysym, XF86KEYS_RANGE_MIN, XF86KEYS_RANGE_MAX) &&
            !IN_RANGE(key->keysym, FKEYS_RANGE_MIN, FKEYS_RANGE_MAX) &&
             key->keysym != GDK_KEY_Pause &&
             key->keysym != GDK_KEY_Print &&
             key->keysym != GDK_KEY_Scroll_Lock &&
             key->keysym != GDK_KEY_Break &&
             key->keysym != GDK_KEY_Menu) {
                GString *keycodes;

                keycodes = g_string_new ("");
                if (key->keycodes != NULL) {
                        guint *c;

                        for (c = key->keycodes; *c; ++c) {
                                g_string_printf (keycodes, " %u", *c);
                        }
                }
                g_warning ("Key 0x%x (keycodes: %s)  with state 0x%x (resolved to 0x%x) "
                           " has no usable modifiers (usable modifiers are 0x%x)",
                           key->keysym, keycodes->str, key->state, modifiers, csd_used_mods);
                g_string_free (keycodes, TRUE);

                return;
        }

        bit = 0;
        /* store the indexes of all set bits in mask in the array */
        for (i = 0; mask; ++i, mask >>= 1) {
                if (mask & 0x1) {
                        indexes[bit++] = i;
                }
        }

        bits_set_cnt = bit;

	all_mods = g_array_new (FALSE, TRUE, sizeof(XIGrabModifiers));
        uppervalue = 1 << bits_set_cnt;
        /* store all possible modifier combinations for our mask into all_mods */
        for (i = 0; i < uppervalue; ++i) {
                int     j;
                int     result = 0;
                XIGrabModifiers *mod;

                /* map bits in the counter to those in the mask */
                for (j = 0; j < bits_set_cnt; ++j) {
                        if (i & (1 << j)) {
                                result |= (1 << indexes[j]);
                        }
                }

                /* Grow the array by one, to fit our new XIGrabModifiers item */
                g_array_set_size (all_mods, all_mods->len + 1);
                mod = &g_array_index (all_mods, XIGrabModifiers, all_mods->len - 1);
                mod->modifiers = result | modifiers;
        }

	/* Capture the actual keycodes with the modifier array */
        for (l = screens; l; l = l->next) {
                GdkScreen *screen = l->data;
                guint *code;

                for (code = key->keycodes; *code; ++code) {
                        grab_key_real (*code,
                                       gdk_screen_get_root_window (screen),
                                       grab,
                                       flags & CSD_KEYGRAB_SYNCHRONOUS,
                                       (XIGrabModifiers *) all_mods->data,
                                       all_mods->len);
                }
        }
        g_array_free (all_mods, TRUE);
}

void
grab_key_unsafe (Key             *key,
                 CsdKeygrabFlags  flags,
                 GSList          *screens)
{
        grab_key_internal (key, TRUE, flags, screens);
}

void
ungrab_key_unsafe (Key    *key,
                   GSList *screens)
{
        grab_key_internal (key, FALSE, 0, screens);
}

static gboolean
have_xkb (Display *dpy)
{
	static int have_xkb = -1;

	if (have_xkb == -1) {
		int opcode, error_base, major, minor, xkb_event_base;

		have_xkb = XkbQueryExtension (dpy,
					      &opcode,
					      &xkb_event_base,
					      &error_base,
					      &major,
					      &minor)
			&& XkbUseExtension (dpy, &major, &minor);
	}

	return have_xkb;
}

gboolean
key_uses_keycode (const Key *key, guint keycode)
{
	if (key->keycodes != NULL) {
		guint *c;

		for (c = key->keycodes; *c; ++c) {
			if (*c == keycode)
				return TRUE;
		}
	}
	return FALSE;
}

/* Adapted from _gdk_x11_device_xi2_translate_state()
 * in gtk+/gdk/x11/gdkdevice-xi2.c */
static guint
device_xi2_translate_state (XIModifierState *mods_state,
			    XIGroupState    *group_state)
{
	guint state;
	gint group;

	state = (guint) mods_state->base | mods_state->latched | mods_state->locked;

	group = group_state->base | group_state->latched | group_state->locked;
	/* FIXME: do we need the XKB complications for this ? */
	group = CLAMP(group, 0, 3);
	state |= group << 13;

	return state;
}

gboolean
match_xi2_key (Key *key, XIDeviceEvent *event)
{
	guint keyval;
	GdkModifierType consumed;
	gint group;
	guint keycode, state;

	if (key == NULL)
		return FALSE;

	setup_modifiers ();

	state = device_xi2_translate_state (&event->mods, &event->group);

	if (have_xkb (event->display))
		group = XkbGroupForCoreState (state);
	else
		group = (state & GDK_KEY_Mode_switch) ? 1 : 0;

	keycode = event->detail;

	/* Check if we find a keysym that matches our current state */
	if (gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (), keycode,
						 state, group,
						 &keyval, NULL, NULL, &consumed)) {
		guint lower, upper;
		guint mask;

		/* HACK: we don't want to use SysRq as a keybinding, so we avoid
		 * its translation from Alt+Print. */
		if (keyval == GDK_KEY_Sys_Req &&
		    (state & GDK_MOD1_MASK) != 0) {
			consumed = 0;
			keyval = GDK_KEY_Print;
		}

		/* The Key structure contains virtual modifiers, whereas
		 * the XEvent will be using the real modifier, so translate those */
		mask = key->state;
		gdk_keymap_map_virtual_modifiers (gdk_keymap_get_default (), &mask);
                mask &= ~(GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK);

		gdk_keyval_convert_case (keyval, &lower, &upper);

		/* If we are checking against the lower version of the
		 * keysym, we might need the Shift state for matching,
		 * so remove it from the consumed modifiers */
		if (lower == key->keysym)
			consumed &= ~GDK_SHIFT_MASK;

		return ((lower == key->keysym || upper == key->keysym)
			&& (state & ~consumed & csd_used_mods) == mask);
	}

	/* The key we passed doesn't have a keysym, so try with just the keycode */
        return (key != NULL
                && key->state == (state & csd_used_mods)
                && key_uses_keycode (key, keycode));
}

Key *
parse_key (const char *str)
{
	Key *key;

	if (str == NULL ||
	    *str == '\0' ||
	    g_str_equal (str, "disabled")) {
		return NULL;
	}

	key = g_new0 (Key, 1);
	gtk_accelerator_parse_with_keycode (str, &key->keysym, &key->keycodes, &key->state);
	if (key->keysym == 0 &&
	    key->keycodes == NULL &&
	    key->state == 0) {
		g_free (key);
                return NULL;
	}

	return key;
}

void
free_key (Key *key)
{
	if (key == NULL)
		return;
	g_free (key->keycodes);
	g_free (key);
}

static void
grab_button_real (int        deviceid,
		  gboolean   grab,
		  GdkWindow *root)
{
	XIGrabModifiers mods;

	mods.modifiers = XIAnyModifier;

	if (grab) {
		XIEventMask evmask;
		unsigned char mask[(XI_LASTEVENT + 7)/8];

		memset (mask, 0, sizeof (mask));
		XISetMask (mask, XI_ButtonRelease);
		XISetMask (mask, XI_ButtonPress);

		evmask.deviceid = deviceid;
		evmask.mask_len = sizeof (mask);
		evmask.mask = mask;

		XIGrabButton (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
			      deviceid,
			      XIAnyButton,
			      GDK_WINDOW_XID (root),
			      None,
			      GrabModeAsync,
			      GrabModeAsync,
			      False,
			      &evmask,
			      1,
			      &mods);
	} else {
		XIUngrabButton (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
				deviceid,
				XIAnyButton,
		                GDK_WINDOW_XID (root),
				1, &mods);
	}
}

void
grab_button (int      deviceid,
	     gboolean grab,
	     GSList  *screens)
{
        GSList *l;

        for (l = screens; l; l = l->next) {
                GdkScreen *screen = l->data;

		grab_button_real (deviceid,
				  grab,
				  gdk_screen_get_root_window (screen));
        }
}
