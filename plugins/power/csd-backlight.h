#ifndef __CSD_BACKLIGHT_H
#define __CSD_BACKLIGHT_H

#include <gio/gio.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-rr.h>

G_BEGIN_DECLS

#define CSD_TYPE_BACKLIGHT (csd_backlight_get_type ())
G_DECLARE_FINAL_TYPE (CsdBacklight, csd_backlight, CSD, BACKLIGHT, GObject)

CsdBacklight  *csd_backlight_new                 (GSettings     *settings);
void           csd_backlight_set_rr_screen       (CsdBacklight  *self,
                                                  GnomeRRScreen *screen);
gint           csd_backlight_get_percentage      (CsdBacklight  *self,
                                                  GError       **error);
gboolean       csd_backlight_set_percentage      (CsdBacklight  *self,
                                                  guint          percentage,
                                                  GError       **error);
gint           csd_backlight_step_up             (CsdBacklight  *self,
                                                  GError       **error);
gint           csd_backlight_step_down           (CsdBacklight  *self,
                                                  GError       **error);
GnomeRROutput *csd_backlight_find_builtin_output (GnomeRRScreen *screen);

G_END_DECLS

#endif /* __CSD_BACKLIGHT_H */
