swaybg(1)

# NAME

swaybg - Background for Wayland

# SYNOPSIS

*swaybg* [options...]

Displays a background image on all outputs of your Wayland session.

Without an output specified, appearance options apply to all outputs.
Per-output appearance options can be set by passing _-o, --output_ followed by
these options.

# OPTIONS

*-c, --color* <[#]rrggbb>
	Set the background color. (The specific color model is typically sRGB or
	however the compositor interprets color values by default, but if a background
	image is set for which swaybg can use color management under the given Wayland
	compositor, then the background color interpretation is unspecified and may
	be changed in the future.)

*-h, --help*
	Show help message and quit.

*-i, --image* <path>
	Set the background image.

*-m, --mode* <mode>
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_. Use
	the additional mode _solid\_color_ to display only the background color,
	even if a background image is specified.

*-o, --output* <name>
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value _\*_ selects all outputs.

*-r, --render* <intent>
	Rendering intents for images: _perceptual_, _relative_, _saturation_,
	_absolute_, _relative_bpc_. The default is _perceptual_. These are only
	applied if the compositor supports color management, a PNG image is loaded
	whose color properties are specified by a CICP chunk, and the compositor
	supports the given rendering intent, the image color primaries, and the
	image transfer function.

*-v, --version*
	Show the version number and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other open
source contributors. For more information about swaybg development, see
<https://github.com/swaywm/swaybg>.
