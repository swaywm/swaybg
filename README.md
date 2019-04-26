# swaybg

swaybg is a wallpaper utility for Wayland compositors. It is compatible with any
Wayland compositor which implements the following Wayland protocols:

- wlr-layer-shell
- xdg-output
- xdg-shell

See the man page, `swaybg(1)`, for instructions on using swaybg.

## Release Signatures

Releases are signed with [B22DA89A](http://pgp.mit.edu/pks/lookup?op=vindex&search=0x52CB6609B22DA89A)
and published [on GitHub](https://github.com/swaywm/swaylock/releases). swaybg
releases are managed independently of sway releases.

## Installation

### From Packages

swaybg is available in many distributions. Try installing the "swaybg"
package for yours.

If you're interested in packaging swaybg for your distribution, stop by the
IRC channel or shoot an email to sir@cmpwn.com for advice.

### Compiling from Source

Install dependencies:

* meson \*
* wayland
* wayland-protocols \*
* cairo
* gdk-pixbuf2 \*\*
* [scdoc](https://git.sr.ht/~sircmpwn/scdoc) (optional: man pages) \*
* git \*

_\*Compile-time dep_

_\*\*optional: required for background images other than PNG_

Run these commands:

    meson build
    ninja -C build
    sudo ninja -C build install
