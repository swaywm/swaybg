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
void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height);

#endif
