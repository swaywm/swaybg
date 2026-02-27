#include <assert.h>
#include "background-image.h"
#include "cairo_util.h"
#include "log.h"

enum background_mode parse_background_mode(const char *mode) {
	if (strcmp(mode, "stretch") == 0) {
		return BACKGROUND_MODE_STRETCH;
	} else if (strcmp(mode, "fill") == 0) {
		return BACKGROUND_MODE_FILL;
	} else if (strcmp(mode, "fit") == 0) {
		return BACKGROUND_MODE_FIT;
	} else if (strcmp(mode, "center") == 0) {
		return BACKGROUND_MODE_CENTER;
	} else if (strcmp(mode, "tile") == 0) {
		return BACKGROUND_MODE_TILE;
	} else if (strcmp(mode, "solid_color") == 0) {
		return BACKGROUND_MODE_SOLID_COLOR;
	}
	swaybg_log(LOG_ERROR, "Unsupported background mode: %s", mode);
	return BACKGROUND_MODE_INVALID;
}

cairo_surface_t *load_background_image(const char *path) {
	cairo_surface_t *image;
#if HAVE_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		swaybg_log(LOG_ERROR, "Failed to load background image (%s).",
				err->message);
		return NULL;
	}
	// Correct for embedded image orientation; typical images are not
	// rotated and will be handled efficiently
	GdkPixbuf *oriented = gdk_pixbuf_apply_embedded_orientation(pixbuf);
	g_object_unref(pixbuf);
	image = gdk_cairo_image_surface_create_from_pixbuf(oriented);
	g_object_unref(oriented);
#else
	image = cairo_image_surface_create_from_png(path);
#endif // HAVE_GDK_PIXBUF
	if (!image) {
		swaybg_log(LOG_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		swaybg_log(LOG_ERROR, "Failed to read background image: %s."
#if !HAVE_GDK_PIXBUF
				"\nSway was compiled without gdk_pixbuf support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif // !HAVE_GDK_PIXBUF
				, cairo_status_to_string(cairo_surface_status(image)));
		return NULL;
	}
	return image;
}

void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height) {
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	cairo_save(cairo);
	switch (mode) {
	case BACKGROUND_MODE_STRETCH:
		cairo_scale(cairo,
				(double)buffer_width / width,
				(double)buffer_height / height);
		cairo_set_source_surface(cairo, image, 0, 0);
		break;
	case BACKGROUND_MODE_FILL: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		} else {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		}
		break;
	}
	case BACKGROUND_MODE_FIT: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		} else {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		}
		break;
	}
	case BACKGROUND_MODE_CENTER:
		cairo_set_source_surface(cairo, image,
				(double)buffer_width / 2 - width / 2,
				(double)buffer_height / 2 - height / 2);
		break;
	case BACKGROUND_MODE_TILE: {
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
		cairo_set_source(cairo, pattern);
		cairo_pattern_destroy(pattern);
		break;
	}
	case BACKGROUND_MODE_SOLID_COLOR:
	case BACKGROUND_MODE_INVALID:
		assert(0);
		break;
	}
	cairo_paint(cairo);
	cairo_restore(cairo);
}

#if HAVE_RSVG
void render_background_image_svg(cairo_t *cairo, RsvgHandle *svg,
		enum background_mode mode, int buffer_width, int buffer_height) {
	gdouble nat_width, nat_height;
	if (!rsvg_handle_get_intrinsic_size_in_pixels(svg, &nat_width, &nat_height)) {
		nat_width = buffer_width;
		nat_height = buffer_height;
	}
	RsvgRectangle viewport;
	GError *error = NULL;;
	switch (mode) {
	case BACKGROUND_MODE_STRETCH:
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = nat_width;
		viewport.height = nat_height;
		cairo_scale(cairo, buffer_width / nat_width, buffer_height / nat_height);
		rsvg_handle_render_document(svg, cairo, &viewport, &error);
		break;
	case BACKGROUND_MODE_FILL: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = nat_width / nat_height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_width / nat_width;

			viewport.x = 0;
			viewport.y = (double)buffer_height / 2 - scale * nat_height / 2;
			viewport.width = buffer_width;
			viewport.height = scale * nat_height;
		} else {
			double scale = (double)buffer_height / nat_height;

			viewport.x = (double)buffer_width / 2 - scale *nat_width / 2;
			viewport.y = 0;
			viewport.width = scale * nat_width;
			viewport.height = buffer_height;
		}

		rsvg_handle_render_document(svg, cairo, &viewport, &error);
		break;
	}
	case BACKGROUND_MODE_FIT: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio =  nat_width / nat_height;

		if (window_ratio < bg_ratio) {
			double scale = (double)buffer_width / nat_width;

			viewport.x = 0;
			viewport.y = (double)buffer_height / 2 - scale * nat_height / 2;
			viewport.width = buffer_width;
			viewport.height = scale * nat_height;
		} else {
			double scale = (double)buffer_height / nat_height;

			viewport.x = (double)buffer_width / 2 - scale * nat_width / 2;
			viewport.y = 0;
			viewport.width = scale * nat_width;
			viewport.height = buffer_height;
		}
		rsvg_handle_render_document(svg, cairo, &viewport, &error);
		break;
	}
	case BACKGROUND_MODE_CENTER: {
		viewport.x = (double)buffer_width / 2 - nat_width / 2;
		viewport.y = (double)buffer_height / 2 - nat_height / 2;
		viewport.width = nat_width;
		viewport.height = nat_height;
		rsvg_handle_render_document(svg, cairo, &viewport, &error);
		break;
	}
	case BACKGROUND_MODE_TILE: {
		for (int x = 0; x * nat_width < buffer_width; x++) {
			for (int y = 0; y * nat_height < buffer_height; y++) {
				viewport.x = nat_width * x;
				viewport.y = nat_height * y;
				viewport.width = nat_width;
				viewport.height = nat_height;
				rsvg_handle_render_document(svg, cairo, &viewport, &error);
			}
		}
		break;
	}
	case BACKGROUND_MODE_SOLID_COLOR:
	case BACKGROUND_MODE_INVALID:
		assert(0);
		break;
	}
}
#endif
