#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
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

#if HAVE_GDK_PIXBUF
struct size_callback {
	void (*size_chooser)(void *data, int width,
		int height, int *scale_width, int *scale_height);
	void *data;
};

void size_prepared_callback(GdkPixbufLoader *loader, gint width, gint height,
		gpointer user_data) {
	struct size_callback *callback = user_data;
	int scaled_width, scaled_height;
	callback->size_chooser(callback->data, width, height,
		&scaled_width, &scaled_height);
	gdk_pixbuf_loader_set_size(loader, scaled_width, scaled_height);
}
#else
static cairo_status_t read_from_fd(void *closure, unsigned char *data,
		unsigned int length) {
	int fd = *((int *)closure);
	while (length > 0) {
		ssize_t n = read(fd, data, length);
		if (n > 0) {
			length -= n;
			data += n;
		} else if (errno == EINTR) {
			continue;
		} else {
			return CAIRO_STATUS_READ_ERROR;
		}
	}
	return CAIRO_STATUS_SUCCESS;
}
#endif // HAVE_GDK_PIXBUF

cairo_surface_t *load_background_image(int file_fd, void *data,
		void (*size_chooser)(void *data, int width,
			int height, int *scale_width, int *scale_height)) {
	cairo_surface_t *image;
#if HAVE_GDK_PIXBUF
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new();

	struct size_callback callback = {size_chooser, data};
	g_signal_connect (loader, "size-prepared",
		G_CALLBACK(size_prepared_callback), &callback);

	GError *err = NULL;
	char *buf = malloc(65536);
	while (true) {
		ssize_t count = read(file_fd, buf, 65536);
		if (count < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				swaybg_log_errno(LOG_ERROR, "Failed to read from background image");
				free(buf);
				g_object_unref(loader);
				return NULL;
			}
		} else if (count == 0) {
			break;
		}
		gdk_pixbuf_loader_write(loader, (guchar *)buf, count, &err);
		if (err) {
			swaybg_log(LOG_ERROR, "Failed to load background image (%s).",
					err->message);
			free(buf);
			g_object_unref(loader);
			return NULL;
		}
	}
	free(buf);
	gdk_pixbuf_loader_close(loader, &err);
	if (err) {
		swaybg_log(LOG_ERROR, "Failed to load background image (%s).",
				err->message);
		return NULL;
	}

	GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	if (!pixbuf) {
		g_object_unref(loader);
		swaybg_log(LOG_ERROR, "Failed to get background image pixbuf");
		return NULL;
	}

	image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	g_object_unref(loader);
#else
	image = cairo_image_surface_create_from_png_stream(read_from_fd, &file_fd);
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
#if !HAVE_GDK_PIXBUF
	int unused_width, unused_height;
	size_chooser(data, cairo_image_surface_get_width(image),
		cairo_image_surface_get_height(image),
		&unused_width, &unused_height);
	(void)unused_width;
	(void)unused_height;
#endif  // !HAVE_GDK_PIXBUF
	return image;
}

void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height,
		int render_width, int render_height) {
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	cairo_save(cairo);
	switch (mode) {
	case BACKGROUND_MODE_CENTER:
	case BACKGROUND_MODE_STRETCH:
	case BACKGROUND_MODE_FILL:
	case BACKGROUND_MODE_FIT: {
		cairo_translate (cairo, (buffer_width - render_width) / 2.0,
				 (buffer_height - render_height) / 2.0);
		cairo_scale(cairo, (double)render_width / width, (double)render_height / height);
		cairo_set_source_surface(cairo, image, 0.0, 0.0);
		break;
	}
	case BACKGROUND_MODE_TILE: {
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
		cairo_set_source(cairo, pattern);
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
