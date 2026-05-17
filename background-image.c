#include <assert.h>
#include <stdlib.h>

#include "background-image.h"
#include "log.h"

#if HAVE_KEULIM
#include <keulim.h>
#endif

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

#if HAVE_KEULIM

static uint8_t to_premult(uint8_t x, uint8_t a) {
	// We want to compute the rounded value of x * a / 255. We can add 0.5 and
	// truncate to approximate rounding. 127/255 ≈ 0.5.
	return (x * a + 127) / 255;
}

/**
 * Create a cairo surface from 8-bit unpacked RGB(A) pixel data, with straight
 * alpha.
 */
static cairo_surface_t *create_surface_from_data(const uint8_t *src_pixels,
		size_t width, size_t height, size_t src_stride, bool has_alpha) {
	size_t src_bytes_per_pixel = has_alpha ? 4 : 3;
	cairo_format_t format = has_alpha ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;

	cairo_surface_t *surface = cairo_image_surface_create(format, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		swaybg_log(LOG_ERROR, "Failed to create cairo surface");
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_surface_flush(surface);

	uint8_t *dst_pixels = cairo_image_surface_get_data(surface);
	int dst_stride = cairo_image_surface_get_stride(surface);
	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < width; x++) {
			const uint8_t *src = &src_pixels[y * src_stride + x * src_bytes_per_pixel];
			uint8_t *dst = &dst_pixels[y * dst_stride + x * sizeof(uint32_t)];

			uint8_t r = src[0];
			uint8_t g = src[1];
			uint8_t b = src[2];
			uint8_t a = has_alpha ? src[3] : 0xFF;

			// Convert from straight alpha to pre-multiplied alpha
			r = to_premult(r, a);
			g = to_premult(g, a);
			b = to_premult(b, a);

			// Convert from unpacked RGBA to native-endian packed ARGB
			uint32_t packed = 0;
			packed |= (uint32_t)r << 16;
			packed |= (uint32_t)g << 8;
			packed |= b;
			packed |= (uint32_t)a << 24;

			memcpy(dst, &packed, sizeof(packed));
		}
	}

	cairo_surface_mark_dirty(surface);

	return surface;
}

static cairo_surface_t *load_image_with_keulim(const char *path) {
	FILE *f = fopen(path, "r");
	if (f == NULL) {
		swaybg_log(LOG_ERROR, "Failed to open file");
		return NULL;
	}

	struct klm_decoder *dec = klm_decoder_create_with_file(f);
	if (dec == NULL) {
		swaybg_log(LOG_ERROR, "Failed to create image decoder");
		goto error_file;
	}

	const struct klm_decoder_info *info = klm_decoder_read_info(dec);
	if (info == NULL) {
		swaybg_log(LOG_ERROR, "Failed to read image info");
		goto error_dec;
	}

	enum klm_format format;
	size_t bytes_per_pixel = 0;
	bool has_alpha = false;
	for (size_t i = 0; i < info->formats_len; i++) {
		format = info->formats[i];
		if (format == KLM_FORMAT_R8G8B8) {
			bytes_per_pixel = 3;
			break;
		} else if (format == KLM_FORMAT_R8G8B8A8) {
			bytes_per_pixel = 4;
			has_alpha = true;
			break;
		}
	}
	if (bytes_per_pixel == 0) {
		swaybg_log(LOG_ERROR, "Unsupported image pixel format");
		goto error_dec;
	}

	size_t stride = bytes_per_pixel * info->width;
	size_t size = stride * info->height;
	uint8_t *buffer = malloc(size);
	if (buffer == NULL) {
		swaybg_log(LOG_ERROR, "Failed to allocate buffer");
		goto error_buffer;
	}

	bool to_srgb_gamma22 = info->color_primaries != 0 && info->color_transfer_function != 0;
	struct klm_decoder_read_frame_options options = {
		.format = format,
		.buffer = buffer,
		.size = size,
		.stride = stride,
		.color_primaries = to_srgb_gamma22 ? KLM_COLOR_PRIMARIES_SRGB : 0,
		.color_transfer_function = to_srgb_gamma22 ? KLM_COLOR_TRANSFER_FUNCTION_GAMMA22 : 0,
	};
	if (!klm_decoder_read_frame(dec, &options)) {
		swaybg_log(LOG_ERROR, "Failed to decode frame");
		goto error_buffer;
	}

	cairo_surface_t *surface = create_surface_from_data(buffer,
		info->width, info->height, stride, has_alpha);
	if (surface == NULL) {
		goto error_buffer;
	}

	free(buffer);
	klm_decoder_destroy(dec);
	fclose(f);
	return surface;

error_buffer:
	free(buffer);
error_dec:
	klm_decoder_destroy(dec);
error_file:
	fclose(f);
	return NULL;
}

#endif

cairo_surface_t *load_background_image(const char *path) {
	cairo_surface_t *image;
#if HAVE_KEULIM
	image = load_image_with_keulim(path);
#else
	image = cairo_image_surface_create_from_png(path);
#endif
	if (!image) {
		swaybg_log(LOG_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		swaybg_log(LOG_ERROR, "Failed to read background image: %s."
#if !HAVE_KEULIM
				"\nSway was compiled without keulim support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif
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
