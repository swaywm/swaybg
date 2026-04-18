#ifndef _SWAY_BACKGROUND_IMAGE_H
#define _SWAY_BACKGROUND_IMAGE_H

#include "cairo_util.h"
#include <stdbool.h>

enum background_mode {
	BACKGROUND_MODE_STRETCH,
	BACKGROUND_MODE_FILL,
	BACKGROUND_MODE_FIT,
	BACKGROUND_MODE_CENTER,
	BACKGROUND_MODE_TILE,
	BACKGROUND_MODE_SOLID_COLOR,
	BACKGROUND_MODE_INVALID,
};

/** CICP (coding-independent code point) values */
struct cicp {
	uint8_t primaries;
	uint8_t transfer;
	uint8_t matrix;
	uint8_t range;
};

struct background_image {
	cairo_surface_t *cairo_surface;
	struct cicp cicp;
	bool has_cicp;
};

enum background_mode parse_background_mode(const char *mode);
/** On success, this returns true and fills *image. */
bool load_background_image(const char *path, struct background_image *image);

/** Render `image` (if provided) according to the given mode.
 *
 * This will produce a buffer in the color space indicated by the image's CICP,
 * if present. `bg_color_srgb` is the sRGB background color and will be
 * transformed to remain accurate if the output is rendered using the CICP.
 *
 * `buffer_width` and `buffer_height` are the target buffer dimensions
 * (in physical pixels). Physical pixels are assumed square.
 */
void render_background(cairo_t *cairo, const struct background_image *image,
		enum background_mode mode, int buffer_width, int buffer_height,
		uint32_t bg_color_srgb);

#endif
