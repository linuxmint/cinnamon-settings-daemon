#include <gtk/gtk.h>

#define KEY "<Alt>XF86AudioMute"

int main (int argc, char **argv)
{
	guint gdk_accel_key;
	guint *gdk_accel_codes;
	GdkModifierType gdk_mods;

	gtk_init (&argc, &argv);

	g_message ("gdk_keyval_from_name ('%s') == %d", KEY, gdk_keyval_from_name(KEY));

	gtk_accelerator_parse_with_keycode (KEY, &gdk_accel_key, &gdk_accel_codes, &gdk_mods);
	g_message ("gtk_accelerator_parse_full ('%s') returned keyval '%d' keycode[0]: '%d' mods: 0x%x",
		   KEY, gdk_accel_key, gdk_accel_codes ? gdk_accel_codes[0] : 0, gdk_mods);
	g_free (gdk_accel_codes);

	return 0;
}
