#include <assert.h>
#include <png.h>

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


static void error_handler(png_structp png_ptr, png_const_charp msg) {
	swaybg_log(LOG_ERROR, "Error in libpng when reading file: %s\n", msg);
	png_longjmp(png_ptr, -1);
}

static int read_chunk_callback(png_structp png_ptr, png_unknown_chunkp chunk_ptr) {
	struct background_image *image = png_get_user_chunk_ptr(png_ptr);
	if (chunk_ptr->size != 4 || strcmp((char*)chunk_ptr->name, "cICP")) {
		swaybg_log(LOG_ERROR, "Unexpected chunk: %s, size %zu", chunk_ptr->name, chunk_ptr->size);
		return 1;
	}

	image->cicp.primaries = chunk_ptr->data[0];
	image->cicp.transfer = chunk_ptr->data[1];
	// matrix and range are always the same, 0 and 1, for normal RGB PNG images
	image->cicp.matrix = chunk_ptr->data[2];
	image->cicp.range = chunk_ptr->data[3];
	image->has_cicp = true;
	return 0;
}

static bool load_png(const char *path, struct background_image *image) {
	struct background_image output = {0};
	uint8_t *image_data = NULL;
	uint8_t **rows = NULL;
	FILE *file = fopen(path, "rb");
	if (!file) {
		return false;
	}

	uint8_t header[8];
	if (fread(header, 1, 8, file) != 8) {
		fclose(file);
		return false;
	}

	bool is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png) {
		fclose(file);
		return false;
	}

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, error_handler, NULL);
	if (!png) {
		fclose(file);
		return false;
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		fclose(file);
		png_destroy_read_struct(&png, NULL, NULL);
		return false;
	}

	/* cICP is fairly new (as of writing, currently in the v3 PNG draft) and
	 * libpng has only gained support for it very recently. To avoid setting a
	 * very recent PNG dependency, parse the chunk ourselves.
	 *
	 * Also note: when cICP does appear, it has precedence over iCCP/sRGB/cHRM/gAMA.
	 *
	 * Other chunks to handle in the future: mDCV, cLLI
	 */
	uint8_t chunk_list[4] = {'c','I','C','P'};
	png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_ALWAYS, chunk_list, 1);
	png_set_read_user_chunk_fn(png, &output, read_chunk_callback);

	jmp_buf *jmpbuf = png_set_longjmp_fn(png, longjmp, sizeof(jmp_buf));
	if (setjmp(*jmpbuf)) {
		goto fail;
	}

	png_init_io(png, file);
	png_set_sig_bytes(png, 8);
	png_read_info(png, info);

	/* Configure libpng to produce RGBA order, 8 byte values */
	png_set_palette_to_rgb(png);
	png_set_swap(png);
	png_set_expand(png);
	png_set_strip_16(png);
	png_set_gray_to_rgb(png);
	png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);

	uint32_t width, height;
	int bit_depth, color_type;
	/* Read PNG properties after transformations have been set */
	png_read_update_info(png, info);
	png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	if (bit_depth != 8 || color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
		swaybg_log(LOG_ERROR, "Unexpected PNG bit depth=%d or color type=%d", bit_depth, color_type);
		goto fail;
	}

	size_t stride = png_get_rowbytes(png, info);
	image_data = calloc(height, stride);
	rows = calloc(height, sizeof(uint8_t *));
	if (!image_data || !rows) {
		swaybg_log(LOG_ERROR, "Failed to allocate image with height=%u, stride=%zu", height, stride);
		goto fail;
	}
	output.cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (!image_data) {
		swaybg_log(LOG_ERROR, "Failed to allocate cairo surface with height=%u, width=%u", height, width);
		goto fail;
	}

	for (uint32_t y = 0; y < height; y++) {
		rows[y] = &image_data[y * stride];
	}

	/* It would be more efficient to operate row-by-row, but this automatically
	 * handles interlacing. */
	png_read_image(png, rows);

	png_read_end(png, info);
	fclose(file);
	png_destroy_read_struct(&png, &info, NULL);

	unsigned char *data = cairo_image_surface_get_data(output.cairo_surface);
	size_t cairo_stride = cairo_image_surface_get_stride(output.cairo_surface);
	for (uint32_t y = 0; y < height; y++) {
		/* Premultiply the row and convert channel order to native-endian for Cairo. */
		for (size_t x = 0; x < width; x++) {
			uint8_t a = image_data[y * stride + x * 4 + 3];
			/* Rounding for premultiplied values is arbitrary, choosing downwards */
			uint8_t r = (image_data[y * stride + x * 4 + 0] * a) / 0xff;
			uint8_t g = (image_data[y * stride + x * 4 + 1] * a) / 0xff;
			uint8_t b = (image_data[y * stride + x * 4 + 2] * a) / 0xff;
			((uint32_t *)&data[y * cairo_stride])[x] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
	cairo_surface_mark_dirty(output.cairo_surface);

	free(rows);
	free(image_data);

	*image = output;
	return true;
fail:
	if (output.cairo_surface) {
		cairo_surface_destroy(output.cairo_surface);
	}
	free(rows);
	free(image_data);
	fclose(file);
	png_destroy_read_struct(&png, &info, NULL);
	return false;
}

bool load_background_image(const char *path, struct background_image *image) {
	struct background_image output = {0};
	const char *suffix = strrchr(path, '.');
	if (suffix && (!strcmp(suffix, ".png") || !strcmp(suffix, ".PNG"))) {
		if (load_png(path, image)) {
			return true;
		}
	}

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
	output.cairo_surface = gdk_cairo_image_surface_create_from_pixbuf(oriented);
	g_object_unref(oriented);
#else
	image = cairo_image_surface_create_from_png(path);
#endif // HAVE_GDK_PIXBUF
	if (!image) {
		swaybg_log(LOG_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(output.cairo_surface) != CAIRO_STATUS_SUCCESS) {
		swaybg_log(LOG_ERROR, "Failed to read background image: %s."
#if !HAVE_GDK_PIXBUF
				"\nSway was compiled without gdk_pixbuf support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif // !HAVE_GDK_PIXBUF
				, cairo_status_to_string(cairo_surface_status(output.cairo_surface)));
		return NULL;
	}

	*image = output;
	return true;
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
