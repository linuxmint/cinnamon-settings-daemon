/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Author: Olivier Fourdan <ofourdan@redhat.com>
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>

#include "csd-wacom-osd-window.h"
#include "csd-wacom-device.h"
#include "csd-enums.h"

#define ROTATION_KEY                "rotation"
#define ACTION_TYPE_KEY             "action-type"
#define CUSTOM_ACTION_KEY           "custom-action"
#define CUSTOM_ELEVATOR_ACTION_KEY  "custom-elevator-action"
#define RES_PATH                    "/org/cinnamon/settings-daemon/plugins/wacom/"

#define BACK_OPACITY		0.8
#define INACTIVE_COLOR		"#ededed"
#define ACTIVE_COLOR		"#729fcf"
#define STROKE_COLOR		"#000000"
#define DARK_COLOR		"#535353"
#define BACK_COLOR		"#000000"

#define ELEVATOR_TIMEOUT	250 /* ms */

static struct {
	const gchar     *color_name;
	const gchar     *color_value;
} css_color_table[] = {
	{ "inactive_color", INACTIVE_COLOR },
	{ "active_color",   ACTIVE_COLOR   },
	{ "stroke_color",   STROKE_COLOR   },
	{ "dark_color",     DARK_COLOR     },
	{ "back_color",     BACK_COLOR     }
};

static gchar *
replace_string (gchar **string, const gchar *search, const char *replacement)
{
	GRegex *regex;
	gchar *res;

	g_return_val_if_fail (*string != NULL, NULL);
	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (search != NULL, *string);
	g_return_val_if_fail (replacement != NULL, *string);

	regex = g_regex_new (search, 0, 0, NULL);
	res = g_regex_replace_literal (regex, *string, -1, 0, replacement, 0, NULL);
	g_regex_unref (regex);
	/* The given string is freed and replaced by the resulting replacement */
	g_free (*string);
	*string = res;

	return res;
}

static gchar
get_last_char (gchar *string)
{
	size_t pos;

	g_return_val_if_fail (string != NULL, '\0');
	pos = strlen (string);
	g_return_val_if_fail (pos > 0, '\0');

	return string[pos - 1];
}

static double
get_rotation_in_radian (CsdWacomRotation rotation)
{
	switch (rotation) {
	case CSD_WACOM_ROTATION_NONE:
		return 0.0;
		break;
	case CSD_WACOM_ROTATION_HALF:
		return G_PI;
		break;
	/* We only support left-handed/right-handed */
	case CSD_WACOM_ROTATION_CCW:
	case CSD_WACOM_ROTATION_CW:
	default:
		break;
	}

	/* Fallback */
	return 0.0;
}

static gboolean
get_sub_location (RsvgHandle *handle,
                  const char *sub,
                  cairo_t    *cr,
                  double     *x,
                  double     *y)
{
	RsvgPositionData  position;
	double tx, ty;

	if (!rsvg_handle_get_position_sub (handle, &position, sub)) {
		g_warning ("Failed to retrieve '%s' position", sub);
		return FALSE;
	}

	tx = (double) position.x;
	ty = (double) position.y;
	cairo_user_to_device (cr, &tx, &ty);

	if (x)
		*x = tx;
	if (y)
		*y = ty;

	return TRUE;
}

static gboolean
get_image_size (const char *filename, int *width, int *height)
{
	RsvgHandle       *handle;
	RsvgDimensionData dimensions;
	GError* error = NULL;

	if (filename == NULL)
		return FALSE;

	handle = rsvg_handle_new_from_file (filename, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
	}
	if (handle == NULL)
		return FALSE;

	/* Compute image size */
	rsvg_handle_get_dimensions (handle, &dimensions);
	g_object_unref (handle);

	if (dimensions.width == 0 || dimensions.height == 0)
		return FALSE;

	if (width)
		*width = dimensions.width;

	if (height)
		*height = dimensions.height;

	return TRUE;
}

static int
get_pango_vertical_offset (PangoLayout *layout)
{
	const PangoFontDescription *desc;
	PangoContext               *context;
	PangoLanguage              *language;
	PangoFontMetrics           *metrics;
	int                         baseline;
	int                         strikethrough;
	int                         thickness;

	context = pango_layout_get_context (layout);
	language = pango_language_get_default ();
	desc = pango_layout_get_font_description (layout);
	metrics = pango_context_get_metrics (context, desc, language);

	baseline = pango_layout_get_baseline (layout);
	strikethrough =  pango_font_metrics_get_strikethrough_position (metrics);
	thickness =  pango_font_metrics_get_underline_thickness (metrics);

	return PANGO_PIXELS (baseline - strikethrough - thickness / 2);
}

#define CSD_TYPE_WACOM_OSD_BUTTON         (csd_wacom_osd_button_get_type ())
#define CSD_WACOM_OSD_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_WACOM_OSD_BUTTON, CsdWacomOSDButton))
#define CSD_WACOM_OSD_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_WACOM_OSD_BUTTON, CsdWacomOSDButtonClass))
#define CSD_IS_WACOM_OSD_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_WACOM_OSD_BUTTON))
#define CSD_IS_WACOM_OSD_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_WACOM_OSD_BUTTON))
#define CSD_WACOM_OSD_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_WACOM_OSD_BUTTON, CsdWacomOSDButtonClass))

typedef struct CsdWacomOSDButtonPrivate CsdWacomOSDButtonPrivate;

typedef struct {
        GObject                   parent;
        CsdWacomOSDButtonPrivate *priv;
} CsdWacomOSDButton;

typedef struct {
        GObjectClass              parent_class;
} CsdWacomOSDButtonClass;

GType                     csd_wacom_osd_button_get_type        (void) G_GNUC_CONST;

enum {
	PROP_OSD_BUTTON_0,
	PROP_OSD_BUTTON_ID,
	PROP_OSD_BUTTON_CLASS,
	PROP_OSD_BUTTON_LABEL,
	PROP_OSD_BUTTON_ACTIVE,
	PROP_OSD_BUTTON_VISIBLE,
	PROP_OSD_BUTTON_AUTO_OFF
};

#define CSD_WACOM_OSD_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
					     CSD_TYPE_WACOM_OSD_BUTTON, \
					     CsdWacomOSDButtonPrivate))
#define MATCH_ID(b,s) (g_strcmp0 (b->priv->id, s) == 0)

struct CsdWacomOSDButtonPrivate {
	GtkWidget                *widget;
	char                     *id;
	char                     *class;
	char                     *label;
	double                    label_x;
	double                    label_y;
	CsdWacomTabletButtonType  type;
	CsdWacomTabletButtonPos   position;
	gboolean                  active;
	gboolean                  visible;
	guint                     auto_off;
	guint                     timeout;
};

static void     csd_wacom_osd_button_class_init  (CsdWacomOSDButtonClass *klass);
static void     csd_wacom_osd_button_init        (CsdWacomOSDButton      *osd_button);
static void     csd_wacom_osd_button_finalize    (GObject                *object);

G_DEFINE_TYPE (CsdWacomOSDButton, csd_wacom_osd_button, G_TYPE_OBJECT)

static void
csd_wacom_osd_button_set_id (CsdWacomOSDButton *osd_button,
			     const gchar       *id)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->id = g_strdup (id);
}

static void
csd_wacom_osd_button_set_class (CsdWacomOSDButton *osd_button,
			        const gchar       *class)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->class = g_strdup (class);
}

static gchar*
csd_wacom_osd_button_get_label_class (CsdWacomOSDButton *osd_button)
{
	gchar *label_class;

	label_class = g_strconcat ("#Label", osd_button->priv->class, NULL);

	return (label_class);
}

static void
csd_wacom_osd_button_set_label (CsdWacomOSDButton *osd_button,
				const gchar       *str)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	g_free (osd_button->priv->label);
	osd_button->priv->label = g_strdup (str ? str : "");
}

static void
csd_wacom_osd_button_set_button_type (CsdWacomOSDButton        *osd_button,
				      CsdWacomTabletButtonType  type)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->type = type;
}

static void
csd_wacom_osd_button_set_position (CsdWacomOSDButton        *osd_button,
				   CsdWacomTabletButtonPos   position)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->position = position;
}

static void
csd_wacom_osd_button_set_location (CsdWacomOSDButton        *osd_button,
				   double                    x,
				   double                    y)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->label_x = x;
	osd_button->priv->label_y = y;
}

static void
csd_wacom_osd_button_set_auto_off (CsdWacomOSDButton        *osd_button,
				   guint                     timeout)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->auto_off = timeout;
}

static void
csd_wacom_osd_button_redraw (CsdWacomOSDButton *osd_button)
{
	GdkWindow *window;

	g_return_if_fail (GTK_IS_WIDGET (osd_button->priv->widget));

	window = gtk_widget_get_window (GTK_WIDGET (osd_button->priv->widget));
	gdk_window_invalidate_rect (window, NULL, FALSE);
}

static gboolean
csd_wacom_osd_button_timer (CsdWacomOSDButton *osd_button)
{
	/* Auto de-activate the button */
	osd_button->priv->active = FALSE;
	csd_wacom_osd_button_redraw (osd_button);

	return FALSE;
}

static void
csd_wacom_osd_button_set_active (CsdWacomOSDButton *osd_button,
				 gboolean           active)
{
	gboolean previous_state;

	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	previous_state = osd_button->priv->active;
	if (osd_button->priv->auto_off > 0) {
		/* For auto-off buttons, apply only if active, de-activation is done in the timeout */
		if (active == TRUE)
			osd_button->priv->active = active;

		if (osd_button->priv->timeout) {
			g_source_remove (osd_button->priv->timeout);
			osd_button->priv->timeout = 0;
		}
		osd_button->priv->timeout = g_timeout_add (osd_button->priv->auto_off,
		                                           (GSourceFunc) csd_wacom_osd_button_timer,
		                                           osd_button);
	} else {
		/* Whereas for other buttons, apply the change straight away */
		osd_button->priv->active = active;
	}

	if (previous_state != osd_button->priv->active)
		csd_wacom_osd_button_redraw (osd_button);
}

static void
csd_wacom_osd_button_set_visible (CsdWacomOSDButton *osd_button,
				  gboolean           visible)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	osd_button->priv->visible = visible;
}

static CsdWacomOSDButton *
csd_wacom_osd_button_new (GtkWidget *widget,
                          gchar *id)
{
	CsdWacomOSDButton *osd_button;

	osd_button = CSD_WACOM_OSD_BUTTON (g_object_new (CSD_TYPE_WACOM_OSD_BUTTON,
	                                                 "id", id,
	                                                 NULL));
	osd_button->priv->widget = widget;

	return osd_button;
}

static void
csd_wacom_osd_button_set_property (GObject        *object,
				   guint           prop_id,
				   const GValue   *value,
				   GParamSpec     *pspec)
{
	CsdWacomOSDButton *osd_button;

	osd_button = CSD_WACOM_OSD_BUTTON (object);

	switch (prop_id) {
	case PROP_OSD_BUTTON_ID:
		csd_wacom_osd_button_set_id (osd_button, g_value_get_string (value));
		break;
	case PROP_OSD_BUTTON_CLASS:
		csd_wacom_osd_button_set_class (osd_button, g_value_get_string (value));
		break;
	case PROP_OSD_BUTTON_LABEL:
		csd_wacom_osd_button_set_label (osd_button, g_value_get_string (value));
		break;
	case PROP_OSD_BUTTON_ACTIVE:
		csd_wacom_osd_button_set_active (osd_button, g_value_get_boolean (value));
		break;
	case PROP_OSD_BUTTON_VISIBLE:
		csd_wacom_osd_button_set_visible (osd_button, g_value_get_boolean (value));
		break;
	case PROP_OSD_BUTTON_AUTO_OFF:
		csd_wacom_osd_button_set_auto_off (osd_button, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
csd_wacom_osd_button_get_property (GObject        *object,
				   guint           prop_id,
				   GValue         *value,
				   GParamSpec     *pspec)
{
	CsdWacomOSDButton *osd_button;

	osd_button = CSD_WACOM_OSD_BUTTON (object);

	switch (prop_id) {
	case PROP_OSD_BUTTON_ID:
		g_value_set_string (value, osd_button->priv->id);
		break;
	case PROP_OSD_BUTTON_CLASS:
		g_value_set_string (value, osd_button->priv->class);
		break;
	case PROP_OSD_BUTTON_LABEL:
		g_value_set_string (value, osd_button->priv->label);
		break;
	case PROP_OSD_BUTTON_ACTIVE:
		g_value_set_boolean (value, osd_button->priv->active);
		break;
	case PROP_OSD_BUTTON_VISIBLE:
		g_value_set_boolean (value, osd_button->priv->visible);
		break;
	case PROP_OSD_BUTTON_AUTO_OFF:
		g_value_set_uint (value, osd_button->priv->auto_off);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
csd_wacom_osd_button_class_init (CsdWacomOSDButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = csd_wacom_osd_button_set_property;
	object_class->get_property = csd_wacom_osd_button_get_property;
	object_class->finalize = csd_wacom_osd_button_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_ID,
	                                 g_param_spec_string ("id",
	                                                      "Button Id",
	                                                      "The Wacom Button ID",
	                                                      "",
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_CLASS,
	                                 g_param_spec_string ("class",
	                                                      "Button Class",
	                                                      "The Wacom Button Class",
	                                                      "",
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_LABEL,
	                                 g_param_spec_string ("label",
	                                                      "Label",
	                                                      "The button label",
	                                                      "",
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_ACTIVE,
	                                 g_param_spec_boolean ("active",
	                                                       "Active",
	                                                       "Whether the button is active",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_VISIBLE,
	                                 g_param_spec_boolean ("visible",
	                                                       "Visible",
	                                                       "Whether the button is visible",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_OSD_BUTTON_AUTO_OFF,
	                                 g_param_spec_uint    ("auto-off",
	                                                       "Auto Off",
	                                                       "Timeout before button disables itself automatically",
	                                                       0,
	                                                       G_MAXUINT,
	                                                       0, /* disabled by default */
	                                                       G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (CsdWacomOSDButtonPrivate));
}

static void
csd_wacom_osd_button_init (CsdWacomOSDButton *osd_button)
{
	osd_button->priv = CSD_WACOM_OSD_BUTTON_GET_PRIVATE (osd_button);
}

static void
csd_wacom_osd_button_finalize (GObject *object)
{
	CsdWacomOSDButton *osd_button;
	CsdWacomOSDButtonPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (object));

	osd_button = CSD_WACOM_OSD_BUTTON (object);

	g_return_if_fail (osd_button->priv != NULL);

	priv = osd_button->priv;

	if (priv->timeout > 0) {
		g_source_remove (priv->timeout);
		priv->timeout = 0;
	}
	g_clear_pointer (&priv->id, g_free);
	g_clear_pointer (&priv->class, g_free);
	g_clear_pointer (&priv->label, g_free);

	G_OBJECT_CLASS (csd_wacom_osd_button_parent_class)->finalize (object);
}

/* Compute the new actual position once rotation is applied */
static CsdWacomTabletButtonPos
get_actual_position (CsdWacomTabletButtonPos position,
		     CsdWacomRotation        rotation)
{
	switch (rotation) {
	case CSD_WACOM_ROTATION_NONE:
		return position;
		break;
	case CSD_WACOM_ROTATION_HALF:
		if (position == WACOM_TABLET_BUTTON_POS_LEFT)
			return WACOM_TABLET_BUTTON_POS_RIGHT;
		if (position == WACOM_TABLET_BUTTON_POS_RIGHT)
			return WACOM_TABLET_BUTTON_POS_LEFT;
		if (position == WACOM_TABLET_BUTTON_POS_TOP)
			return WACOM_TABLET_BUTTON_POS_BOTTOM;
		if (position == WACOM_TABLET_BUTTON_POS_BOTTOM)
			return WACOM_TABLET_BUTTON_POS_TOP;
		break;
	/* We only support left-handed/right-handed */
	case CSD_WACOM_ROTATION_CCW:
	case CSD_WACOM_ROTATION_CW:
	default:
		break;
	}
	/* fallback */
	return position;
}

static void
csd_wacom_osd_button_draw_label (CsdWacomOSDButton *osd_button,
			         GtkStyleContext   *style_context,
			         PangoContext      *pango_context,
			         cairo_t           *cr,
			         CsdWacomRotation   rotation)
{
	CsdWacomOSDButtonPrivate *priv;
	PangoLayout              *layout;
	PangoRectangle            logical_rect;
	CsdWacomTabletButtonPos   actual_position;
	double                    lx, ly;
	gchar                    *markup;

	g_return_if_fail (CSD_IS_WACOM_OSD_BUTTON (osd_button));

	priv = osd_button->priv;
	if (priv->visible == FALSE)
		return;

	actual_position = get_actual_position (priv->position, rotation);
	layout = pango_layout_new (pango_context);
	if (priv->active)
		markup = g_strdup_printf ("<span foreground=\"" ACTIVE_COLOR "\" weight=\"normal\">%s</span>", priv->label);
	else
		markup = g_strdup_printf ("<span foreground=\"" INACTIVE_COLOR "\" weight=\"normal\">%s</span>", priv->label);
	pango_layout_set_markup (layout, markup, -1);
	g_free (markup);

	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	switch (actual_position) {
	case WACOM_TABLET_BUTTON_POS_LEFT:
		pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
		lx = priv->label_x + logical_rect.x;
		ly = priv->label_y + logical_rect.y - get_pango_vertical_offset (layout);
		break;
	case WACOM_TABLET_BUTTON_POS_RIGHT:
		pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);
		lx = priv->label_x + logical_rect.x - logical_rect.width;
		ly = priv->label_y + logical_rect.y - get_pango_vertical_offset (layout);
		break;
	case WACOM_TABLET_BUTTON_POS_TOP:
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
		lx = priv->label_x + logical_rect.x - logical_rect.width / 2;
		ly = priv->label_y + logical_rect.y;
		break;
	case WACOM_TABLET_BUTTON_POS_BOTTOM:
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
		lx = priv->label_x + logical_rect.x - logical_rect.width / 2;
		ly = priv->label_y + logical_rect.y - logical_rect.height;
		break;
	default:
		g_warning ("Unhandled button position");
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
		lx = priv->label_x + logical_rect.x - logical_rect.width / 2;
		ly = priv->label_y + logical_rect.y - logical_rect.height / 2;
		break;
	}
	gtk_render_layout (style_context, cr, lx, ly, layout);
	g_object_unref (layout);
}

enum {
  PROP_OSD_WINDOW_0,
  PROP_OSD_WINDOW_MESSAGE,
  PROP_OSD_WINDOW_CSD_WACOM_DEVICE
};

#define CSD_WACOM_OSD_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
					     CSD_TYPE_WACOM_OSD_WINDOW, \
					     CsdWacomOSDWindowPrivate))

struct CsdWacomOSDWindowPrivate
{
	RsvgHandle               *handle;
	CsdWacomDevice           *pad;
	CsdWacomRotation          rotation;
	GdkRectangle              screen_area;
	GdkRectangle              monitor_area;
	GdkRectangle              tablet_area;
	char                     *message;
	GList                    *buttons;
};

static void     csd_wacom_osd_window_class_init  (CsdWacomOSDWindowClass *klass);
static void     csd_wacom_osd_window_init        (CsdWacomOSDWindow      *osd_window);
static void     csd_wacom_osd_window_finalize    (GObject                *object);

G_DEFINE_TYPE (CsdWacomOSDWindow, csd_wacom_osd_window, GTK_TYPE_WINDOW)

static RsvgHandle *
load_rsvg_with_base (const char  *css_string,
		     const char  *original_layout_path,
		     GError     **error)
{
	RsvgHandle *handle;
	char *dirname;

	handle = rsvg_handle_new ();

	dirname = g_path_get_dirname (original_layout_path);
	rsvg_handle_set_base_uri (handle, dirname);
	g_free (dirname);

	if (!rsvg_handle_write (handle,
				(guint8 *) css_string,
				strlen (css_string),
				error)) {
		g_object_unref (handle);
		return NULL;
	}
	if (!rsvg_handle_close (handle, error)) {
		g_object_unref (handle);
		return NULL;
	}

	return handle;
}

static void
csd_wacom_osd_window_update (CsdWacomOSDWindow *osd_window)
{
	GError      *error = NULL;
	gchar       *width, *height;
	gchar       *buttons_section;
	gchar       *css_string;
	const gchar *layout_file;
	GBytes      *css_data;
        guint i;
	GList *l;

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));
	g_return_if_fail (CSD_IS_WACOM_DEVICE (osd_window->priv->pad));

	css_data = g_resources_lookup_data (RES_PATH "tablet-layout.css", 0, &error);
	if (error != NULL) {
		g_printerr ("GResource error: %s\n", error->message);
		g_clear_pointer (&error, g_error_free);
	}
	if (css_data == NULL)
		return;
	css_string = g_strdup ((gchar *) g_bytes_get_data (css_data, NULL));
	g_bytes_unref(css_data);

	width = g_strdup_printf ("%d", osd_window->priv->tablet_area.width);
	replace_string (&css_string, "layout_width", width);
	g_free (width);

	height = g_strdup_printf ("%d", osd_window->priv->tablet_area.height);
	replace_string (&css_string, "layout_height", height);
	g_free (height);

	/* Build the buttons section */
	buttons_section = g_strdup ("");
	for (l = osd_window->priv->buttons; l != NULL; l = l->next) {
		CsdWacomOSDButton *osd_button = l->data;

		if (osd_button->priv->visible == FALSE)
			continue;

		if (osd_button->priv->active) {
			buttons_section = g_strconcat (buttons_section,
			                               ".", osd_button->priv->class, " {\n"
			                               "      stroke:   active_color !important;\n"
			                               "      fill:     active_color !important;\n"
			                               "    }\n",
			                               NULL);
		}
	}
	replace_string (&css_string, "buttons_section", buttons_section);
	g_free (buttons_section);

        for (i = 0; i < G_N_ELEMENTS (css_color_table); i++)
		replace_string (&css_string,
		                css_color_table[i].color_name,
		                css_color_table[i].color_value);

	layout_file = csd_wacom_device_get_layout_path (osd_window->priv->pad);
	replace_string (&css_string, "layout_file", layout_file);

	/* Render the SVG with the CSS applied */
	g_clear_object (&osd_window->priv->handle);
	osd_window->priv->handle = load_rsvg_with_base (css_string, layout_file, &error);
	if (osd_window->priv->handle == NULL) {
		g_debug ("CSS applied:\n%s\n", css_string);
		g_printerr ("RSVG error: %s\n", error->message);
		g_clear_error (&error);
	}
	g_free (css_string);
}

static void
csd_wacom_osd_window_draw_message (CsdWacomOSDWindow   *osd_window,
				   GtkStyleContext     *style_context,
				   PangoContext        *pango_context,
				   cairo_t             *cr)
{
	GdkRectangle  *monitor_area = &osd_window->priv->monitor_area;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	char *markup;
	double x;
	double y;

	if (osd_window->priv->message == NULL)
		return;

	layout = pango_layout_new (pango_context);
	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

	markup = g_strdup_printf ("<span foreground=\"white\">%s</span>", osd_window->priv->message);
	pango_layout_set_markup (layout, markup, -1);
	g_free (markup);

	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	x = (monitor_area->width - logical_rect.width) / 2 + logical_rect.x;
	y = (monitor_area->height - logical_rect.height) / 2 + logical_rect.y;

	gtk_render_layout (style_context, cr, x, y, layout);
	g_object_unref (layout);
}

static void
csd_wacom_osd_window_draw_labels (CsdWacomOSDWindow   *osd_window,
				  GtkStyleContext     *style_context,
				  PangoContext        *pango_context,
				  cairo_t             *cr)
{
	GList *l;

	for (l = osd_window->priv->buttons; l != NULL; l = l->next) {
		CsdWacomOSDButton *osd_button = l->data;

		if (osd_button->priv->visible == FALSE)
			continue;

		csd_wacom_osd_button_draw_label (osd_button,
			                         style_context,
			                         pango_context,
			                         cr,
			                         osd_window->priv->rotation);
	}
}

static void
csd_wacom_osd_window_place_buttons (CsdWacomOSDWindow *osd_window,
				    cairo_t           *cr)
{
	GList            *l;

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));

	for (l = osd_window->priv->buttons; l != NULL; l = l->next) {
		CsdWacomOSDButton *osd_button = l->data;
		double             label_x, label_y;
		gchar             *sub;

		sub = csd_wacom_osd_button_get_label_class (osd_button);
		if (!get_sub_location (osd_window->priv->handle, sub, cr, &label_x, &label_y)) {
			g_warning ("Failed to retrieve %s position", sub);
			g_free (sub);
			continue;
		}
		g_free (sub);
		csd_wacom_osd_button_set_location (osd_button, label_x, label_y);
	}
}

/* Note: this function does modify the given cairo context */
static void
csd_wacom_osd_window_adjust_cairo (CsdWacomOSDWindow *osd_window,
                                   cairo_t           *cr)
{
	double         scale, twidth, theight;
	GdkRectangle  *tablet_area  = &osd_window->priv->tablet_area;
	GdkRectangle  *screen_area  = &osd_window->priv->screen_area;
	GdkRectangle  *monitor_area = &osd_window->priv->monitor_area;

	/* Rotate */
	cairo_rotate (cr, get_rotation_in_radian (osd_window->priv->rotation));

	/* Scale to fit in window */
	scale = MIN ((double) monitor_area->width / tablet_area->width,
	             (double) monitor_area->height / tablet_area->height);
	cairo_scale (cr, scale, scale);

	/* Center the result in window */
	twidth = (double) tablet_area->width;
	theight = (double) tablet_area->height;
	cairo_user_to_device_distance (cr, &twidth, &theight);

	twidth = ((double) monitor_area->width - twidth) / 2.0;
	theight = ((double) monitor_area->height - theight) / 2.0;
	cairo_device_to_user_distance (cr, &twidth, &theight);

	twidth = twidth + (double) (monitor_area->x - screen_area->x);
	theight = theight + (double) (monitor_area->y - screen_area->y);

	cairo_translate (cr, twidth, theight);
}

static gboolean
csd_wacom_osd_window_draw (GtkWidget *widget,
			   cairo_t   *cr)
{
	CsdWacomOSDWindow *osd_window = CSD_WACOM_OSD_WINDOW (widget);

	g_return_val_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window), FALSE);
	g_return_val_if_fail (CSD_IS_WACOM_DEVICE (osd_window->priv->pad), FALSE);

	if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget))) {
		GtkStyleContext     *style_context;
		PangoContext        *pango_context;

		style_context = gtk_widget_get_style_context (widget);
		pango_context = gtk_widget_get_pango_context (widget);

		cairo_set_source_rgba (cr, 0, 0, 0, BACK_OPACITY);
		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint (cr);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

		/* Save original matrix */
		cairo_save (cr);

		/* Apply new cairo transformation matrix */
		csd_wacom_osd_window_adjust_cairo (osd_window, cr);

		/* And render the tablet layout */
		csd_wacom_osd_window_update (osd_window);
		rsvg_handle_render_cairo (osd_window->priv->handle, cr);

		csd_wacom_osd_window_place_buttons (osd_window, cr);

		/* Reset to original matrix */
		cairo_restore (cr);

		/* Draw button labels and message */
		csd_wacom_osd_window_draw_labels (osd_window,
		                                  style_context,
		                                  pango_context,
		                                  cr);
		csd_wacom_osd_window_draw_message (osd_window,
		                                   style_context,
		                                   pango_context,
		                                   cr);
	}

	return FALSE;
}

static gchar *
get_escaped_accel_shortcut (const gchar *accel)
{
	guint keyval;
	GdkModifierType mask;
	gchar *str, *label;

	if (accel == NULL || accel[0] == '\0')
		return g_strdup (_("Do Nothing"));

	gtk_accelerator_parse (accel, &keyval, &mask);

	str = gtk_accelerator_get_label (keyval, mask);
	label = g_markup_printf_escaped (_("Send Keystroke %s"), str);
	g_free (str);

	return label;
}

static gchar *
get_tablet_button_label_normal (CsdWacomDevice       *device,
				CsdWacomTabletButton *button)
{
	CsdWacomActionType type;
	gchar *name, *str;

	type = g_settings_get_enum (button->settings, ACTION_TYPE_KEY);
	if (type == CSD_WACOM_ACTION_TYPE_NONE)
		return g_strdup (_("Do Nothing"));

	if (type == CSD_WACOM_ACTION_TYPE_HELP)
		return g_strdup (_("Show On-Screen Help"));

	if (type == CSD_WACOM_ACTION_TYPE_SWITCH_MONITOR)
		return g_strdup (_("Switch Monitor"));

	str = g_settings_get_string (button->settings, CUSTOM_ACTION_KEY);
	if (str == NULL || *str == '\0') {
		g_free (str);
		return g_strdup (_("Do Nothing"));
	}

	name = get_escaped_accel_shortcut (str);
	g_free (str);

	return name;
}

static gchar *
get_tablet_button_label_touch  (CsdWacomDevice       *device,
				CsdWacomTabletButton *button,
				GtkDirectionType      dir)
{
	char **strv, *name, *str;

	strv = g_settings_get_strv (button->settings, CUSTOM_ELEVATOR_ACTION_KEY);
	name = NULL;

	if (strv) {
		if (g_strv_length (strv) >= 1 && dir == GTK_DIR_UP)
			name = g_strdup (strv[0]);
		else if (g_strv_length (strv) >= 2 && dir == GTK_DIR_DOWN)
			name = g_strdup (strv[1]);
		g_strfreev (strv);
	}

	str = get_escaped_accel_shortcut (name);
	g_free (name);
	name = str;

	/* With multiple modes, also show the current mode for that action */
	if (csd_wacom_device_get_num_modes (device, button->group_id) > 1) {
		name = g_strdup_printf (_("Mode %d: %s"), button->idx + 1, str);
		g_free (str);
	}

	return name;
}

static gchar *
get_tablet_button_label (CsdWacomDevice       *device,
	                 CsdWacomTabletButton *button,
	                 GtkDirectionType      dir)
{
	g_return_val_if_fail (button, NULL);

	if (!button->settings)
		goto out;

	switch (button->type) {
	case WACOM_TABLET_BUTTON_TYPE_NORMAL:
		return get_tablet_button_label_normal (device, button);
		break;
	case WACOM_TABLET_BUTTON_TYPE_RING:
	case WACOM_TABLET_BUTTON_TYPE_STRIP:
		return get_tablet_button_label_touch (device, button, dir);
		break;
	case WACOM_TABLET_BUTTON_TYPE_HARDCODED:
	default:
		break;
	}
out:
	return g_strdup (button->name);
}

static gchar*
get_tablet_button_class_name (CsdWacomTabletButton *tablet_button,
                              GtkDirectionType      dir)
{
	gchar *id;
	gchar  c;

	id = tablet_button->id;
	switch (tablet_button->type) {
	case WACOM_TABLET_BUTTON_TYPE_RING:
		if (id[0] == 'l') /* left-ring */
			return g_strdup_printf ("Ring%s", (dir == GTK_DIR_UP ? "CCW" : "CW"));
		if (id[0] == 'r') /* right-ring */
			return g_strdup_printf ("Ring2%s", (dir == GTK_DIR_UP ? "CCW" : "CW"));
		g_warning ("Unknown ring type '%s'", id);
		return NULL;
		break;
	case WACOM_TABLET_BUTTON_TYPE_STRIP:
		if (id[0] == 'l') /* left-strip */
			return g_strdup_printf ("Strip%s", (dir == GTK_DIR_UP ? "Up" : "Down"));
		if (id[0] == 'r') /* right-strip */
			return g_strdup_printf ("Strip2%s", (dir == GTK_DIR_UP ? "Up" : "Down"));
		g_warning ("Unknown strip type '%s'", id);
		return NULL;
		break;
	case WACOM_TABLET_BUTTON_TYPE_NORMAL:
	case WACOM_TABLET_BUTTON_TYPE_HARDCODED:
		c = get_last_char (id);
		return g_strdup_printf ("%c", g_ascii_toupper (c));
		break;
	default:
		g_warning ("Unknown button type '%s'", id);
		break;
	}

	return NULL;
}

static gchar*
get_tablet_button_id_name (CsdWacomTabletButton *tablet_button,
                           GtkDirectionType      dir)
{
	gchar *id;
	gchar  c;

	id = tablet_button->id;
	switch (tablet_button->type) {
	case WACOM_TABLET_BUTTON_TYPE_RING:
		return g_strconcat (id, (dir == GTK_DIR_UP ? "-ccw" : "-cw"), NULL);
		break;
	case WACOM_TABLET_BUTTON_TYPE_STRIP:
		return g_strconcat (id, (dir == GTK_DIR_UP ? "-up" : "-down"), NULL);
		break;
	case WACOM_TABLET_BUTTON_TYPE_NORMAL:
	case WACOM_TABLET_BUTTON_TYPE_HARDCODED:
		c = get_last_char (id);
		return g_strdup_printf ("%c", g_ascii_toupper (c));
		break;
	default:
		g_warning ("Unknown button type '%s'", id);
		break;
	}

	return NULL;
}

static gint
get_elevator_current_mode (CsdWacomOSDWindow    *osd_window,
                           CsdWacomTabletButton *elevator_button)
{
	GList *list, *l;
	gint   mode;

	mode = 1;
	/* Search in the list of buttons the corresponding
	 * mode-switch button and get the current mode
	 */
	list = csd_wacom_device_get_buttons (osd_window->priv->pad);
	for (l = list; l != NULL; l = l->next) {
		CsdWacomTabletButton *tablet_button = l->data;

		if (tablet_button->type != WACOM_TABLET_BUTTON_TYPE_HARDCODED)
			continue;
		if (elevator_button->group_id != tablet_button->group_id)
			continue;
		mode = csd_wacom_device_get_current_mode (osd_window->priv->pad,
		                                          tablet_button->group_id);
		break;
	}
	g_list_free (list);

	return mode;
}

static CsdWacomOSDButton *
csd_wacom_osd_window_add_button_with_dir (CsdWacomOSDWindow    *osd_window,
                                          CsdWacomTabletButton *tablet_button,
                                          guint                 timeout,
                                          GtkDirectionType      dir)
{
	CsdWacomOSDButton    *osd_button;
	gchar                *str;

	str = get_tablet_button_id_name (tablet_button, dir);
	osd_button = csd_wacom_osd_button_new (GTK_WIDGET (osd_window), str);
	g_free (str);

	str = get_tablet_button_class_name (tablet_button, dir);
	csd_wacom_osd_button_set_class (osd_button, str);
	g_free (str);

	str = get_tablet_button_label (osd_window->priv->pad, tablet_button, dir);
	csd_wacom_osd_button_set_label (osd_button, str);
	g_free (str);

	csd_wacom_osd_button_set_button_type (osd_button, tablet_button->type);
	csd_wacom_osd_button_set_position (osd_button, tablet_button->pos);
	csd_wacom_osd_button_set_auto_off (osd_button, timeout);
	osd_window->priv->buttons = g_list_append (osd_window->priv->buttons, osd_button);

	return osd_button;
}

static void
csd_wacom_osd_window_add_tablet_button (CsdWacomOSDWindow    *osd_window,
                                        CsdWacomTabletButton *tablet_button)
{
	CsdWacomOSDButton    *osd_button;
	gint                  mode;

	switch (tablet_button->type) {
	case WACOM_TABLET_BUTTON_TYPE_NORMAL:
	case WACOM_TABLET_BUTTON_TYPE_HARDCODED:
		osd_button = csd_wacom_osd_window_add_button_with_dir (osd_window,
		                                                       tablet_button,
		                                                       0,
		                                                       0);
		csd_wacom_osd_button_set_visible (osd_button, TRUE);
		break;
	case WACOM_TABLET_BUTTON_TYPE_RING:
	case WACOM_TABLET_BUTTON_TYPE_STRIP:
		mode = get_elevator_current_mode (osd_window, tablet_button) - 1;

		/* Add 2 buttons per elevator, one "Up"... */
		osd_button = csd_wacom_osd_window_add_button_with_dir (osd_window,
		                                                       tablet_button,
		                                                       ELEVATOR_TIMEOUT,
		                                                       GTK_DIR_UP);
		csd_wacom_osd_button_set_visible (osd_button, tablet_button->idx == mode);

		/* ... and one "Down" */
		osd_button = csd_wacom_osd_window_add_button_with_dir (osd_window,
		                                                       tablet_button,
		                                                       ELEVATOR_TIMEOUT,
		                                                       GTK_DIR_DOWN);
		csd_wacom_osd_button_set_visible (osd_button, tablet_button->idx == mode);

		break;
	default:
		g_warning ("Unknown button type");
		break;
	}
}

/*
 * Returns the rotation to apply a device to get a representation relative to
 * the current rotation of the output.
 * (This function is _not_ the same as in csd-wacom-manager.c)
 */
static CsdWacomRotation
display_relative_rotation (CsdWacomRotation device_rotation,
			   CsdWacomRotation output_rotation)
{
	CsdWacomRotation rotations[] = { CSD_WACOM_ROTATION_HALF,
	                                 CSD_WACOM_ROTATION_CW,
	                                 CSD_WACOM_ROTATION_NONE,
	                                 CSD_WACOM_ROTATION_CCW };
	guint i;

	if (device_rotation == output_rotation)
		return CSD_WACOM_ROTATION_NONE;

	if (output_rotation == CSD_WACOM_ROTATION_NONE)
		return device_rotation;

	for (i = 0; i < G_N_ELEMENTS (rotations); i++) {
		if (device_rotation == rotations[i])
			break;
	}

	if (output_rotation == CSD_WACOM_ROTATION_HALF)
		return rotations[(i + G_N_ELEMENTS (rotations) - 2) % G_N_ELEMENTS (rotations)];

	if (output_rotation == CSD_WACOM_ROTATION_CW)
		return rotations[(i + 1) % G_N_ELEMENTS (rotations)];

	if (output_rotation == CSD_WACOM_ROTATION_CCW)
		return rotations[(i + G_N_ELEMENTS (rotations) - 1) % G_N_ELEMENTS (rotations)];

	/* fallback */
	return CSD_WACOM_ROTATION_NONE;
}

static void
csd_wacom_osd_window_mapped (GtkWidget *widget,
                             gpointer   data)
{
	CsdWacomOSDWindow *osd_window = CSD_WACOM_OSD_WINDOW (widget);

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));

	/* Position the window at its expected position before moving
	 * to fullscreen, so the window will be on the right monitor.
	 */
	gtk_window_move (GTK_WINDOW (osd_window),
	                 osd_window->priv->screen_area.x,
	                 osd_window->priv->screen_area.y);

	gtk_window_fullscreen (GTK_WINDOW (osd_window));
	gtk_window_set_keep_above (GTK_WINDOW (osd_window), TRUE);
}

static void
csd_wacom_osd_window_realized (GtkWidget *widget,
                               gpointer   data)
{
	CsdWacomOSDWindow *osd_window = CSD_WACOM_OSD_WINDOW (widget);
	GdkWindow         *gdk_window;
	GdkRGBA            transparent;
	GdkScreen         *screen;
	GdkCursor         *cursor;
	gint               monitor;
	gboolean           status;

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));
	g_return_if_fail (CSD_IS_WACOM_DEVICE (osd_window->priv->pad));

	if (!gtk_widget_get_realized (widget))
		return;

	screen = gtk_widget_get_screen (widget);
	gdk_window = gtk_widget_get_window (widget);

	transparent.red = transparent.green = transparent.blue = 0.0;
	transparent.alpha = BACK_OPACITY;
	gdk_window_set_background_rgba (gdk_window, &transparent);

	cursor = gdk_cursor_new (GDK_BLANK_CURSOR);
	gdk_window_set_cursor (gdk_window, cursor);
	g_object_unref (cursor);

	/* Determine the monitor for that device and set appropriate fullscreen mode*/
	monitor = csd_wacom_device_get_display_monitor (osd_window->priv->pad);
	if (monitor == CSD_WACOM_SET_ALL_MONITORS) {
		/* Covers the entire screen */
		osd_window->priv->screen_area.x = 0;
		osd_window->priv->screen_area.y = 0;
		osd_window->priv->screen_area.width = gdk_screen_get_width (screen);
		osd_window->priv->screen_area.height = gdk_screen_get_height (screen);
		gdk_screen_get_monitor_geometry (screen, 0, &osd_window->priv->monitor_area);
		gdk_window_set_fullscreen_mode (gdk_window, GDK_FULLSCREEN_ON_ALL_MONITORS);
	} else {
		gdk_screen_get_monitor_geometry (screen, monitor, &osd_window->priv->screen_area);
		osd_window->priv->monitor_area = osd_window->priv->screen_area;
		gdk_window_set_fullscreen_mode (gdk_window, GDK_FULLSCREEN_ON_CURRENT_MONITOR);
	}

	gtk_window_set_default_size (GTK_WINDOW (osd_window),
	                             osd_window->priv->screen_area.width,
	                             osd_window->priv->screen_area.height);

	status = get_image_size (csd_wacom_device_get_layout_path (osd_window->priv->pad),
	                         &osd_window->priv->tablet_area.width,
	                         &osd_window->priv->tablet_area.height);
	if (status == FALSE)
		osd_window->priv->tablet_area = osd_window->priv->monitor_area;
}

static void
csd_wacom_osd_window_set_device (CsdWacomOSDWindow *osd_window,
				 CsdWacomDevice    *device)
{
	CsdWacomRotation  device_rotation;
	CsdWacomRotation  output_rotation;
	GSettings        *settings;
	GList            *list, *l;

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));
	g_return_if_fail (CSD_IS_WACOM_DEVICE (device));

	/* If we had a layout previously handled, get rid of it */
	if (osd_window->priv->handle)
		g_object_unref (osd_window->priv->handle);
	osd_window->priv->handle = NULL;

	/* Bind the device with the OSD window */
	if (osd_window->priv->pad)
		g_object_weak_unref (G_OBJECT(osd_window->priv->pad),
		                     (GWeakNotify) gtk_widget_destroy,
		                     osd_window);
	osd_window->priv->pad = device;
	g_object_weak_ref (G_OBJECT(osd_window->priv->pad),
	                   (GWeakNotify) gtk_widget_destroy,
	                   osd_window);

	/* Capture current rotation, we do not update that later, OSD window is meant to be short lived */
	settings = csd_wacom_device_get_settings (osd_window->priv->pad);
	device_rotation = g_settings_get_enum (settings, ROTATION_KEY);
	output_rotation = csd_wacom_device_get_display_rotation (osd_window->priv->pad);
	osd_window->priv->rotation = display_relative_rotation (device_rotation, output_rotation);

	/* Create the buttons */
	list = csd_wacom_device_get_buttons (device);
	for (l = list; l != NULL; l = l->next) {
		CsdWacomTabletButton *tablet_button = l->data;

		csd_wacom_osd_window_add_tablet_button (osd_window, tablet_button);
	}
	g_list_free (list);
}

CsdWacomDevice *
csd_wacom_osd_window_get_device (CsdWacomOSDWindow *osd_window)
{
	g_return_val_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window), NULL);

	return osd_window->priv->pad;
}

void
csd_wacom_osd_window_set_message (CsdWacomOSDWindow *osd_window,
				  const gchar       *str)
{
	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));

	g_free (osd_window->priv->message);
	osd_window->priv->message = g_strdup (str);
}

const char *
csd_wacom_osd_window_get_message (CsdWacomOSDWindow *osd_window)
{
	g_return_val_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window), NULL);

	return osd_window->priv->message;
}

static void
csd_wacom_osd_window_set_property (GObject        *object,
				   guint           prop_id,
				   const GValue   *value,
				   GParamSpec     *pspec)
{
	CsdWacomOSDWindow *osd_window;

	osd_window = CSD_WACOM_OSD_WINDOW (object);

	switch (prop_id) {
	case PROP_OSD_WINDOW_MESSAGE:
		csd_wacom_osd_window_set_message (osd_window, g_value_get_string (value));
		break;
	case PROP_OSD_WINDOW_CSD_WACOM_DEVICE:
		csd_wacom_osd_window_set_device (osd_window, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
csd_wacom_osd_window_get_property (GObject        *object,
				   guint           prop_id,
				   GValue         *value,
				   GParamSpec     *pspec)
{
	CsdWacomOSDWindow *osd_window;

	osd_window = CSD_WACOM_OSD_WINDOW (object);

	switch (prop_id) {
	case PROP_OSD_WINDOW_MESSAGE:
		g_value_set_string (value, osd_window->priv->message);
		break;
	case PROP_OSD_WINDOW_CSD_WACOM_DEVICE:
		g_value_set_object (value, (GObject*) osd_window->priv->pad);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

void
csd_wacom_osd_window_set_active (CsdWacomOSDWindow    *osd_window,
				 CsdWacomTabletButton *button,
				 GtkDirectionType      dir,
				 gboolean              active)
{
	GList     *l;
	gchar     *id;

	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (osd_window));
	g_return_if_fail (button != NULL);

	id = get_tablet_button_id_name (button, dir);
	for (l = osd_window->priv->buttons; l != NULL; l = l->next) {
		CsdWacomOSDButton *osd_button = l->data;
		if (MATCH_ID (osd_button, id))
			csd_wacom_osd_button_set_active (osd_button, active);
	}
	g_free (id);
}

void
csd_wacom_osd_window_set_mode (CsdWacomOSDWindow    *osd_window,
                               gint                  group_id,
                               gint                  mode)
{
	GList                *list, *l;

	list = csd_wacom_device_get_buttons (osd_window->priv->pad);
	for (l = list; l != NULL; l = l->next) {
		CsdWacomTabletButton *tablet_button = l->data;
		GList                *l2;
		gchar                *id_up, *id_down;

		if (tablet_button->type != WACOM_TABLET_BUTTON_TYPE_STRIP &&
		    tablet_button->type != WACOM_TABLET_BUTTON_TYPE_RING)
			continue;
		if (tablet_button->group_id != group_id)
			continue;

		id_up = get_tablet_button_id_name (tablet_button, GTK_DIR_UP);
		id_down = get_tablet_button_id_name (tablet_button, GTK_DIR_DOWN);

		for (l2 = osd_window->priv->buttons; l2 != NULL; l2 = l2->next) {
			CsdWacomOSDButton *osd_button = l2->data;
			gboolean           visible = (tablet_button->idx == mode - 1);

			if (MATCH_ID (osd_button, id_up) || MATCH_ID (osd_button, id_down))
				csd_wacom_osd_button_set_visible (osd_button, visible);
		}

		g_free (id_up);
		g_free (id_down);

	}
	g_list_free (list);
}

GtkWidget *
csd_wacom_osd_window_new (CsdWacomDevice       *pad,
                          const gchar          *message)
{
	CsdWacomOSDWindow *osd_window;
	GdkScreen         *screen;
	GdkVisual         *visual;

	osd_window = CSD_WACOM_OSD_WINDOW (g_object_new (CSD_TYPE_WACOM_OSD_WINDOW,
	                                                 "type",              GTK_WINDOW_TOPLEVEL,
	                                                 "skip-pager-hint",   TRUE,
	                                                 "skip-taskbar-hint", TRUE,
	                                                 "focus-on-map",      TRUE,
	                                                 "decorated",         FALSE,
	                                                 "deletable",         FALSE,
	                                                 "accept-focus",      TRUE,
	                                                 "wacom-device",      pad,
	                                                 "message",           message,
	                                                 NULL));

	/* Must set the visual before realizing the window */
	gtk_widget_set_app_paintable (GTK_WIDGET (osd_window), TRUE);
	screen = gdk_screen_get_default ();
	visual = gdk_screen_get_rgba_visual (screen);
	if (visual == NULL)
		visual = gdk_screen_get_system_visual (screen);
	gtk_widget_set_visual (GTK_WIDGET (osd_window), visual);

	g_signal_connect (GTK_WIDGET (osd_window), "realize",
	                  G_CALLBACK (csd_wacom_osd_window_realized),
	                  NULL);
	g_signal_connect (GTK_WIDGET (osd_window), "map",
	                  G_CALLBACK (csd_wacom_osd_window_mapped),
	                  NULL);

	return GTK_WIDGET (osd_window);
}

static void
csd_wacom_osd_window_class_init (CsdWacomOSDWindowClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (klass);
	widget_class  = GTK_WIDGET_CLASS (klass);

	gobject_class->set_property = csd_wacom_osd_window_set_property;
	gobject_class->get_property = csd_wacom_osd_window_get_property;
	gobject_class->finalize     = csd_wacom_osd_window_finalize;
	widget_class->draw          = csd_wacom_osd_window_draw;

	g_object_class_install_property (gobject_class,
	                                 PROP_OSD_WINDOW_MESSAGE,
	                                 g_param_spec_string ("message",
	                                                      "Window message",
	                                                      "The message shown in the OSD window",
	                                                      "",
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class,
	                                 PROP_OSD_WINDOW_CSD_WACOM_DEVICE,
	                                 g_param_spec_object ("wacom-device",
	                                                      "Wacom device",
	                                                      "The Wacom device represented by the OSD window",
	                                                      CSD_TYPE_WACOM_DEVICE,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (CsdWacomOSDWindowPrivate));
}

static void
csd_wacom_osd_window_init (CsdWacomOSDWindow *osd_window)
{
	osd_window->priv = CSD_WACOM_OSD_WINDOW_GET_PRIVATE (osd_window);
}

static void
csd_wacom_osd_window_finalize (GObject *object)
{
	CsdWacomOSDWindow *osd_window;
	CsdWacomOSDWindowPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (CSD_IS_WACOM_OSD_WINDOW (object));

	osd_window = CSD_WACOM_OSD_WINDOW (object);
	g_return_if_fail (osd_window->priv != NULL);

	priv = osd_window->priv;
	g_clear_object (&priv->handle);
	g_clear_pointer (&priv->message, g_free);
	if (priv->buttons) {
		g_list_free_full (priv->buttons, g_object_unref);
		priv->buttons = NULL;
	}

	G_OBJECT_CLASS (csd_wacom_osd_window_parent_class)->finalize (object);
}
