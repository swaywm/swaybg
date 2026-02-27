#ifndef _SWAY_CAIRO_UTIL_H
#define _SWAY_CAIRO_UTIL_H

#include <stdint.h>
#include <cairo.h>
#include <wayland-client.h>
#if HAVE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

void cairo_set_source_u32(cairo_t *cairo, uint32_t color);

void cairo_rgb30_swap_rb(cairo_surface_t *surface);

#if HAVE_GDK_PIXBUF

cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(
		const GdkPixbuf *gdkbuf);

#endif // HAVE_GDK_PIXBUF

#endif
