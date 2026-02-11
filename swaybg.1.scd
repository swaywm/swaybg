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
	Set the background color.

*-h, --help*
	Show help message and quit.

*-i, --image* <path>
	Set the background image.

*-m, --mode* <mode>
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, _tile_, _span_ or _solid_color_.

	The scaling modes are as follows:
	- _stretch_ (default): Stretches the image to fit the entire screen.
	- _fill_: Scales the image while maintaining its aspect ratio to fill the entire screen.
	- _fit_: Scales the image while maintaining its aspect ratio to fit within the screen without cropping.
	- _center_: Centers the image on the screen without scaling.
	- _tile_: Repeats the image across the screen.
	- _span_: Same as stretch but preserves aspect ratio by adding padding around the image.
	- _solid_color_: Displays only the background color, ignoring any specified image.

*-o, --output* <name>
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value _\*_ selects all outputs.

*-v, --version*
	Show the version number and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other open
source contributors. For more information about swaybg development, see
<https://github.com/swaywm/swaybg>.
