#ifndef __CSD_AMBIENT_CURVE_H
#define __CSD_AMBIENT_CURVE_H

#include <glib.h>

G_BEGIN_DECLS

/* Lux readings at which the display should be 0, 20, 40, 60, 80 and 100%. */
#define CSD_AMBIENT_CURVE_POINTS 6

/* A reading within this fraction (or this many lux, whichever is larger)
 * of the last applied reading is treated as unchanged. */
#define CSD_AMBIENT_CURVE_BAND_FRACTION 0.10
#define CSD_AMBIENT_CURVE_BAND_LUX 1.0

/* A reading outside the band must stay outside it for this long before
 * it is applied. */
#define CSD_AMBIENT_CURVE_SETTLE_USEC (2 * G_USEC_PER_SEC)

typedef struct {
    gdouble points[CSD_AMBIENT_CURVE_POINTS];
    gdouble applied_reading;
    gint    applied_percentage;
    gint64  candidate_since;
} CsdAmbientCurve;

void csd_ambient_curve_init   (CsdAmbientCurve *curve,
                               const gdouble   *points);
gint csd_ambient_curve_lookup (const CsdAmbientCurve *curve,
                               gdouble                lux);
void csd_ambient_curve_adjust (CsdAmbientCurve *curve,
                               gdouble          lux,
                               gint             percentage);
gint csd_ambient_curve_update (CsdAmbientCurve *curve,
                               gdouble          lux,
                               gint64           now,
                               gboolean        *wait);

G_END_DECLS

#endif /* __CSD_AMBIENT_CURVE_H */
