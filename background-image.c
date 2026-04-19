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

static void render_background_image(cairo_t *cairo, cairo_surface_t *image,
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

/* CIE XYZ with D65 white point */
struct xyz_color {
	double x;
	double y;
	double z;
};

struct primaries {
	double gx, gy;
	double bx, by;
	double rx, ry;
	double wx, wy;
};

/** Return the color primaries for the given CICP value, falling back to an
 *  arbitrary value if unrecognized. */
struct primaries get_primaries(uint32_t value) {
	/* See H.273 specification, Table 2.
	 *
	 * Note: the white point values given in H.273 are rounded from the
	 * standard CIE 2 degree observer coordinates */
	switch (value) {
	default:
		swaybg_log(LOG_DEBUG, "Unexpected CICP primary value %u, colors may be wrong", value);
		return (struct primaries) {
			.gx = 0.300, .gy = 0.600,
			.bx = 0.150, .by = 0.060,
			.rx = 0.640, .ry = 0.330,
			.wx = 0.3127, .wy = 0.3290,
		};
	case 1:
		return (struct primaries) {
			.gx = 0.300, .gy = 0.600,
			.bx = 0.150, .by = 0.060,
			.rx = 0.640, .ry = 0.330,
			.wx = 0.3127, .wy = 0.3290,
		};

	case 5:
		return (struct primaries) {
			.gx = 0.29, .gy = 0.60,
			.bx = 0.15, .by = 0.06,
			.rx = 0.64, .ry = 0.33,
			.wx = 0.3127, .wy = 0.3290,
		};

	case 6:
	case 7:
		return (struct primaries) {
			.gx = 0.310, .gy = 0.595,
			.bx = 0.155, .by = 0.070,
			.rx = 0.630, .ry = 0.340,
			.wx = 0.3127, .wy = 0.3290,
		};

	case 9:
		return (struct primaries) {
			.gx = 0.170, .gy = 0.797,
			.bx = 0.131, .by = 0.046,
			.rx = 0.708, .ry = 0.292,
			.wx = 0.3127, .wy = 0.3290,
		};

	case 12:
		return (struct primaries) {
			.gx = 0.265, .gy = 0.690,
			.bx = 0.150, .by = 0.060,
			.rx = 0.680, .ry = 0.320,
			.wx = 0.3127, .wy = 0.3290,
		};

	case 22:
		return (struct primaries) {
			.gx = 0.295, .gy = 0.605,
			.bx = 0.155, .by = 0.077,
			.rx = 0.630, .ry = 0.340,
			.wx = 0.3127, .wy = 0.3290,
		};
	}
}

typedef double (*transfer_fn)(double);


static double linear_tf(double l) {
	return fmax(0.0, fmin(l, 1.0));
}

static double gamma22_tf(double l) {
	return pow(fmax(0.0, fmin(l, 1.0)), 1.0 / 2.2);
}

static double gamma28_tf(double l) {
	return pow(fmax(0.0, fmin(l, 1.0)), 1.0 / 2.8);
}

static double smpte240m_tf(double l) {
	l = fmax(0.0, fmin(l, 1.0));
	double alpha = 1.111572195921731;
	double beta = 0.022821585529445028;
	if (l <= beta) {
		return 4.0 * l;
	} else {
		return alpha * pow(l, 0.45) - (alpha - 1.0);
	}
}

static double log100_tf(double l) {
	return fmax(0.0, fmin(1.0, 1.0 + log10(l) / 2.0));
}

static double log316_tf(double l) {
	return fmax(0.0, fmin(1.0, 1.0 + log10(l) / 2.5));
}

static double srgb_tf(double l) {
	l = fmax(0.0, fmin(l, 1.0));
	/* `beta` is root of `(1-1/2.4) beta - beta^(1-1/2.4) + 1/(2.4 * 12.92)`,
	 * `alpha` is `12.92 * 2.4 beta^(1 - 1/2.4)`
	 *
	 * Note: this TF is often stated with alpha rounded to to 1.055,
	 * for which the matching `beta` would be 0.0031308 */
	double beta = 0.003041282560127519;
	double alpha = 1.0550107189475868;
	if (l <= beta) {
		return 12.92 * l;
	} else {
		return alpha * pow(l, 1.0 / 2.4) - (alpha - 1.0);
	}
}

static double srgb_tf_inverse(double v) {
	v = fmax(0.0, fmin(v, 1.0));
	double alpha = 1.0550107189475868;
	double beta = 0.003041282560127519;
	if (v <= beta * 12.92) {
		return v / 12.92;
	} else {
		return pow((v + (alpha - 1.0)) / alpha, 2.4);
	}
}

static double rec709_tf(double l) {
	l = fmax(0.0, fmin(l, 1.0));
	double alpha = 1.099296826809443;
	double beta = 0.018053968510807813;
	if (l <= beta) {
		return 4.5 * l;
	} else {
		return alpha * pow(l, 0.45) - (alpha - 1.0);
	}
}

static double pq_tf(double l) {
	double c1 = 107.0 / 128.0;
	double c2 = 2413.0 / 128.0;
	double c3 = 2392.0 / 128.0;
	double m = 2523.0 / 32.0;
	double n = 653.0 / 4096.0;
	// ref max 10^4 cd/m^2; the Wayland color protocol has
	// 203 cd/m^2 as the reference white
	double l0 = l * 203.0 / 10000.0;
	l0 = fmax(0.0, fmin(l0, 1.0));
	return pow((c1 + c2 * pow(l0, n)) / (1.0 + c3 * pow(l0, n)), m);
}

static double hlg_tf(double l) {
	l = fmax(0.0, fmin(l, 1.0));
	if (l >= 1.0 / 12.0) {
		double a = 0.17883277;
		double b = 0.28466892;
		double c = 0.55991073;
		return a * log(12.0 * l - b) + c;
	} else {
		return sqrt(3 * l);
	}
}

/** Return the inverse of the transfer function from "electrical" to optical
 * values.
 *
 * (The H.273 transfer functions may be either the EOTF or inverse of EOTF;
 * for historical reasons the two can differ.) */
static transfer_fn get_tf(uint32_t value) {
	switch (value) {
	case 1:
	case 6:
	case 14:
	case 15:
		return rec709_tf;
	case 4:
		return gamma22_tf;
	case 5:
		return gamma28_tf;
	case 7:
		return smpte240m_tf;
	case 8:
		return linear_tf;
	case 9:
		return log100_tf;
	case 10:
		return log316_tf;
	default:
		swaybg_log(LOG_DEBUG, "Unexpected CICP transfer characteristic value %u, colors may be wrong", value);
		return srgb_tf;
	case 13:
		return srgb_tf;
	case 16:
		return pq_tf;
	case 18:
		return hlg_tf;
	}
}

/** Return the transfer function from "electrical" to optical values.
 */
static transfer_fn get_tf_inverse(uint32_t value) {
	// Only sRGB is needed at the moment
	assert(value == 13);
	return srgb_tf_inverse;
}

/* bt709, "sRGB" transfer function, full range */
static const struct cicp default_cicp = {
	.primaries = 1,
	.transfer = 13,
	.matrix = 0,
	.range = 1,
};

static void invert_3x3(const double in[3][3], double out[3][3]) {
	double x00 = in[1][1] * in[2][2] - in[1][2] * in[2][1];
	double x01 = -(in[1][0] * in[2][2] - in[1][2] * in[2][0]);
	double x02 = in[1][0] * in[2][1] - in[1][1] * in[2][0];
	double x10 = -(in[0][1] * in[2][2] - in[0][2] * in[2][1]);
	double x11 = in[0][0] * in[2][2] - in[0][2] * in[2][0];
	double x12 = -(in[0][0] * in[2][1] - in[0][1] * in[2][0]);
	double x20 = in[0][1] * in[1][2] - in[0][2] * in[1][1];
	double x21 = -(in[0][0] * in[1][2] - in[0][2] * in[1][0]);
	double x22 = in[0][0] * in[1][1] - in[0][1] * in[1][0];

	double det = in[0][0] * x00 + in[0][1] * x01 + in[0][2] * x02;
	assert(det != 0);
	out[0][0] = x00 / det;
	out[1][0] = x01 / det;
	out[2][0] = x02 / det;
	out[0][1] = x10 / det;
	out[1][1] = x11 / det;
	out[2][1] = x12 / det;
	out[0][2] = x20 / det;
	out[1][2] = x21 / det;
	out[2][2] = x22 / det;
}

static void swap(double *a, double *b) {
	double tmp = *a;
	*a = *b;
	*b = tmp;
}

static void transpose_3x3(double mtx[3][3]) {
	swap(&mtx[0][1], &mtx[1][0]);
	swap(&mtx[0][2], &mtx[2][0]);
	swap(&mtx[1][2], &mtx[2][1]);
}

static void mul_3x3(const double mtx[3][3], const double in[3], double out[3]) {
	out[0] = in[0] * mtx[0][0] + in[1] * mtx[0][1] + in[2] * mtx[0][2];
	out[1] = in[0] * mtx[1][0] + in[1] * mtx[1][1] + in[2] * mtx[1][2];
	out[2] = in[0] * mtx[2][0] + in[1] * mtx[2][1] + in[2] * mtx[2][2];
}

struct xyz_color srgb_to_xyz(uint32_t rgbx) {
	struct primaries p = get_primaries(1);
	transfer_fn transfer_inv =  get_tf_inverse(13);
	double er = (double)((rgbx >> 24) & 0xff) / 255.0;
	double eg = (double)((rgbx >> 16) & 0xff) / 255.0;
	double eb = (double)((rgbx >> 8) & 0xff) / 255.0;

	double o[3] = {transfer_inv(er), transfer_inv(eg), transfer_inv(eb)};

	double xyz_base[3][3] = {
		{p.rx / p.ry, 1.0, (1.0 - (p.rx + p.ry)) / p.ry},
		{p.gx / p.gy, 1.0, (1.0 - (p.gx + p.gy)) / p.gy},
		{p.bx / p.by, 1.0, (1.0 - (p.bx + p.by)) / p.by},
	};
	transpose_3x3(xyz_base);

	double xyz_base_inv[3][3];
	invert_3x3(xyz_base, xyz_base_inv);

	double xyz_white[3] = {p.wx / p.wy, 1.0, (1.0 - (p.wx + p.wy)) / p.wy};
	double scale[3];
	mul_3x3(xyz_base_inv, xyz_white, scale);
	double rgb_to_xyz[3][3] = {
		{xyz_base[0][0] * scale[0], xyz_base[0][1] * scale[1], xyz_base[0][2] * scale[2]},
		{xyz_base[1][0] * scale[0], xyz_base[1][1] * scale[1], xyz_base[1][2] * scale[2]},
		{xyz_base[2][0] * scale[0], xyz_base[2][1] * scale[1], xyz_base[2][2] * scale[2]},
	};

	double xyz_to_rgb[3][3];
	invert_3x3(rgb_to_xyz, xyz_to_rgb);

	double xyz[3];
	mul_3x3(rgb_to_xyz, o, xyz);

	return (struct xyz_color) {
		.x = xyz[0],
		.y = xyz[1],
		.z = xyz[2],
	};
}

/** Convert an XYZ color to an encoded RGB color using CICP.
 *
 * TODO: add support for white point adaptation to allow translating between
 * different illuminants; the exact method chosen should ideally match what
 * Wayland compositors do.
 */
uint32_t xyz_to_cicp(struct xyz_color c, const struct cicp *cicp) {
	struct primaries p = get_primaries(cicp->primaries);
	transfer_fn transfer = get_tf(cicp->transfer);
	if (cicp->range != 1) {
		swaybg_log(LOG_DEBUG, "Unexpected CICP range value %u, colors may be wrong", cicp->range);
	}
	if (cicp->matrix != 0) {
		swaybg_log(LOG_DEBUG, "Unexpected CICP matrix coefficient value %u, colors may be wrong", cicp->matrix);
	}


	double xyz_base[3][3] = {
		{p.rx / p.ry, 1.0, (1.0 - (p.rx + p.ry)) / p.ry},
		{p.gx / p.gy, 1.0, (1.0 - (p.gx + p.gy)) / p.gy},
		{p.bx / p.by, 1.0, (1.0 - (p.bx + p.by)) / p.by},
	};
	transpose_3x3(xyz_base);

	double xyz_base_inv[3][3];
	invert_3x3(xyz_base, xyz_base_inv);

	double xyz_white[3] = {p.wx / p.wy, 1.0, (1.0 - (p.wx + p.wy)) / p.wy};
	double scale[3];
	mul_3x3(xyz_base_inv, xyz_white, scale);
	double rgb_to_xyz[3][3] = {
		{xyz_base[0][0] * scale[0], xyz_base[0][1] * scale[1], xyz_base[0][2] * scale[2]},
		{xyz_base[1][0] * scale[0], xyz_base[1][1] * scale[1], xyz_base[1][2] * scale[2]},
		{xyz_base[2][0] * scale[0], xyz_base[2][1] * scale[1], xyz_base[2][2] * scale[2]},
	};
	double xyz_to_rgb[3][3];
	invert_3x3(rgb_to_xyz, xyz_to_rgb);

	double xyz[3] = {c.x, c.y, c.z};
	double rgb[3];
	mul_3x3(xyz_to_rgb, xyz, rgb);

	uint8_t er = round(transfer(rgb[0]) * 255.0);
	uint8_t eg = round(transfer(rgb[1]) * 255.0);
	uint8_t eb = round(transfer(rgb[2]) * 255.0);

	return ((uint32_t)er << 24) | ((uint32_t)eg << 16) | ((uint32_t)eb << 8) | 0xff;
}


void render_background(cairo_t *cairo, const struct background_image *image,
		enum background_mode mode, int buffer_width, int buffer_height,
		uint32_t bg_color_srgb) {

	// Map the background color from sRGB to the same colorspace as the image.
	// Blending may not be particularly accurate, but accurate blending is
	// complicated to implement efficiently, and the discrepancy may be hard
	// to notice except for wallpapers heavily using alpha transparency.

	uint32_t bg_color = xyz_to_cicp(srgb_to_xyz(bg_color_srgb),
		image && image->has_cicp ? &image->cicp : &default_cicp);

	cairo_set_source_u32(cairo, bg_color);
	cairo_paint(cairo);

	if (image && image->cairo_surface) {
		render_background_image(cairo, image->cairo_surface,
			mode, buffer_width, buffer_height);
	}

}
