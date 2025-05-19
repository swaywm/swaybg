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
	Scaling mode for images: _stretch_, _fill_, _fit_, _center_, or _tile_. Use
	the additional mode _solid\_color_ to display only the background color,
	even if a background image is specified.

*-n, --namespace* <name>
	Namespace to use for the layer shell surface. Defaults to "wallpaper".

*-o, --output* <name>
	Select an output to configure. Subsequent appearance options will only
	apply to this output. The special value _\*_ selects all outputs.

*-v, --version*
	Show the version number and quit.

# AUTHORS

Maintained by Simon Ser <contact@emersion.fr>, who is assisted by other open
source contributors. For more information about swaybg development, see
<https://github.com/swaywm/swaybg>.
