project(
	'swaybg',
	'c',
	version: '1.2.1',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

add_project_arguments(cc.get_supported_arguments([
	'-Wno-unused-parameter',
	'-Wno-unused-result',
	'-Wundef',
	'-Wvla',
]), language: 'c')

is_freebsd = host_machine.system().startswith('freebsd')
if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

rt = cc.find_library('rt')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.31')
wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)
cairo = dependency('cairo')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf'))
rsvg = dependency('librsvg-2.0', required: get_option('rsvg'))

git = find_program('git', required: false, native: true)
scdoc = find_program('scdoc', required: get_option('man-pages'), native: true)

version = '"@0@"'.format(meson.project_version())
if git.found()
	git_commit = run_command([git, '--git-dir=.git', 'rev-parse', '--short', 'HEAD'], check: false)
	git_branch = run_command([git, '--git-dir=.git', 'rev-parse', '--abbrev-ref', 'HEAD'], check: false)
	if git_commit.returncode() == 0 and git_branch.returncode() == 0
		version = '"@0@-@1@ (" __DATE__ ", branch \'@2@\')"'.format(
			meson.project_version(),
			git_commit.stdout().strip(),
			git_branch.stdout().strip(),
		)
	endif
endif

add_project_arguments([
	'-DSWAYBG_VERSION=@0@'.format(version),
	'-DHAVE_GDK_PIXBUF=@0@'.format(gdk_pixbuf.found().to_int()),
	'-DHAVE_RSVG=@0@'.format(rsvg.found().to_int()),
], language: 'c')

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'), native: true)

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protos_src = []

client_protocols = [
	wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	wl_protocol_dir / 'stable/viewporter/viewporter.xml',
	wl_protocol_dir / 'staging/single-pixel-buffer/single-pixel-buffer-v1.xml',
	wl_protocol_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
	'wlr-layer-shell-unstable-v1.xml',
]

foreach filename : client_protocols
	protos_src += wayland_scanner_code.process(filename)
	protos_src += wayland_scanner_client.process(filename)
endforeach

executable(
	'swaybg',
	[
		'background-image.c',
		'cairo.c',
		'log.c',
		'main.c',
		'pool-buffer.c',
		protos_src,
	],
	include_directories: 'include',
	dependencies: [
		cairo,
		rt,
		gdk_pixbuf,
		wayland_client,
		rsvg,
	],
	install: true
)

if scdoc.found()
	mandir = get_option('mandir')
	man_files = [
		'swaybg.1.scd',
	]
	foreach filename : man_files
		topic = filename.split('.')[-3].split('/')[-1]
		section = filename.split('.')[-2]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: filename,
			output: output,
			command: scdoc,
			feed: true,
			capture: true,
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif
