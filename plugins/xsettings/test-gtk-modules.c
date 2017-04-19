

#include "csd-xsettings-gtk.h"

static void
gtk_modules_callback (CsdXSettingsGtk       *gtk,
		      GParamSpec            *spec,
		      gpointer               user_data)
{
	const char *modules;

	modules = csd_xsettings_gtk_get_modules (gtk);
	g_message ("GTK+ modules list changed to: %s", modules ? modules : "(empty)");
}

int main (int argc, char **argv)
{
	GMainLoop *loop;
	CsdXSettingsGtk *gtk;


	gtk = csd_xsettings_gtk_new ();
        g_signal_connect (G_OBJECT (gtk), "notify::gtk-modules",
			  G_CALLBACK (gtk_modules_callback), NULL);

	gtk_modules_callback (gtk, NULL, NULL);

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return 0;
}
