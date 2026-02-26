#include <assert.h>
#if HAVE_GLYCIN
#include <glycin-2/glycin.h>
#endif
#include "background-image.h"
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

#if HAVE_GLYCIN
bool load_background_image(const char *path, struct background_image *image) {
	bool success = false;

	GFile *file = g_file_new_for_path (path);
	if (!file) {
		swaybg_log(LOG_ERROR, "Failed to read background image at '%s'.", path);
		return false;
	}
	GlyLoader *loader = gly_loader_new (file);
	if (!loader) {
		swaybg_log(LOG_ERROR, "Failed to create image loader for '%s'.", path);
		goto err_after_file;
	}
	gly_loader_set_sandbox_selector(loader, GLY_SANDBOX_SELECTOR_AUTO);

	GlyMemoryFormatSelection formats = GLY_MEMORY_SELECTION_B8G8R8A8_PREMULTIPLIED;
	gly_loader_set_accepted_memory_formats(loader, formats);

	GError *error = NULL;
	GlyImage *gly_image = gly_loader_load(loader, &error);
	if (!gly_image) {
		swaybg_log(LOG_ERROR, "Failed to load image '%s': %s", path, error->message);
		g_error_free(error);
		goto err_after_loader;
	}

	GlyFrame *frame = gly_image_next_frame(gly_image, &error);
	if (!frame) {
		swaybg_log(LOG_ERROR, "Failed to load primary frame of image '%s': %s",
			path, error->message);
		g_error_free(error);
		goto err_after_image;
	}
	GlyMemoryFormat format = gly_frame_get_memory_format(frame);
	assert(format == GLY_MEMORY_B8G8R8A8_PREMULTIPLIED);

	uint32_t width = gly_frame_get_width(frame);
	uint32_t height = gly_frame_get_height(frame);
	assert(width > 0 && height > 0);

	uint32_t gly_stride = gly_frame_get_stride(frame);
	GBytes *bytes = gly_frame_get_buf_bytes(frame);
	gsize size = 0;
	const uint8_t *gly_data = (const uint8_t *)g_bytes_get_data(bytes, &size);

	if (width > INT_MAX || height > INT_MAX) {
		swaybg_log(LOG_ERROR,
			"Image dimensions %"PRIu32" x %"PRIu32" too large for cairo",
			width, height);
		goto err_after_frame;
	}

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (!surface) {
		swaybg_log(LOG_ERROR,
			"Failed to create cairo surface of size %"PRIu32" x %"PRIu32,
			width, height);
		goto err_after_frame;
	}

	unsigned char *cairo_data = cairo_image_surface_get_data(surface);
	int cairo_stride = cairo_image_surface_get_stride(surface);

	cairo_surface_flush (surface);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		surface = NULL;
		swaybg_log(LOG_ERROR, "Failed to flush cairo surface");
		goto err_after_frame;
	}

	// Convert GLY_MEMORY_B8G8R8A8_PREMULTIPLIED (whose channel order is
	// endianness independent) with CAIRO_FORMAT_ARGB32 (native endian uint32_t,
	// premultiplied).
	for (int y = 0; y < (int)height; y++) {
		for (int x = 0; x < (int)width; x++) {
			uint32_t *dst = (uint32_t *)&cairo_data[cairo_stride * y + x * 4];
			const uint8_t *src  = (const uint8_t*)&gly_data[gly_stride * y + x * 4];
			*dst = ((uint32_t)src[0] << 0) | ((uint32_t)src[1] << 8)
				| ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
		}
	}

	cairo_surface_mark_dirty(surface);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		surface = NULL;
		swaybg_log(LOG_ERROR, "Failed to mark cairo surface dirty");
		goto err_after_frame;
	}

	GlyCicp *cicp = gly_frame_get_color_cicp(frame);
	if (cicp) {
		image->has_cicp = true;
		image->cicp.primaries = cicp->color_primaries;
		image->cicp.transfer = cicp->transfer_characteristics;
		image->cicp.matrix = cicp->matrix_coefficients;
		image->cicp.range = cicp->video_full_range_flag;
		gly_cicp_free(cicp);
	} else {
		image->has_cicp = false;
	}

	image->cairo_surface = surface;
	success = true;

err_after_frame:
	g_object_unref(frame);
err_after_image:
	g_object_unref(gly_image);
err_after_loader:
	g_object_unref(loader);
err_after_file:
	g_object_unref(file);
	return success;
}

#else
bool load_background_image(const char *path, struct background_image *image) {
	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (!surface) {
		swaybg_log(LOG_ERROR, "Failed to read background image.");
		return false;
	}
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		swaybg_log(LOG_ERROR, "Failed to read background image: %s."
			"\nSwaybg was compiled without glycin support, so only"
			"\nPNG images can be loaded. This is the likely cause.",
			cairo_status_to_string(cairo_surface_status(surface)));
		return false;
	}
	image->cairo_surface = surface;
	image->has_cicp = false;
	return true;
}
#endif

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
