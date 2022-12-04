project(
	'swaybg',
	'c',
	version: '1.2.0',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

is_freebsd = host_machine.system().startswith('freebsd')
if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif


cc = meson.get_compiler('c')
rt = cc.find_library('rt')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91', native: true)
cairo = dependency('cairo')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf'))

git = find_program('git', required: false, native: true)
scdoc = find_program('scdoc', required: get_option('man-pages'), native: true)

version = '"@0@"'.format(meson.project_version())
if git.found()
	git_commit_hash = run_command([git, 'describe', '--always', '--tags'], check: false)
	git_branch = run_command([git, 'rev-parse', '--abbrev-ref', 'HEAD'], check: false)
	if git_commit_hash.returncode() == 0 and git_branch.returncode() == 0
		version = '"@0@ (" __DATE__ ", branch \'@1@\')"'.format(git_commit_hash.stdout().strip(), git_branch.stdout().strip())
	endif
endif

add_project_arguments([
	'-DSWAYBG_VERSION=@0@'.format(version),
	'-DHAVE_GDK_PIXBUF=@0@'.format(gdk_pixbuf.found().to_int()),
], language: 'c')

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'))

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

client_protos_src = []
client_protos_headers = []

client_protocols = [
	wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	wl_protocol_dir / 'stable/viewporter/viewporter.xml',
	wl_protocol_dir / 'staging/single-pixel-buffer/single-pixel-buffer-v1.xml',
	'wlr-layer-shell-unstable-v1.xml',
]

foreach filename : client_protocols
	client_protos_src += wayland_scanner_code.process(filename)
	client_protos_headers += wayland_scanner_client.process(filename)
endforeach

lib_client_protos = static_library(
	'client_protos',
	client_protos_src + client_protos_headers,
	dependencies: [wayland_client]
) # for the include directory

client_protos = declare_dependency(
	link_with: lib_client_protos,
	sources: client_protos_headers,
)

dependencies = [
	cairo,
	client_protos,
	rt,
	gdk_pixbuf,
	wayland_client,
]

sources = [
	'background-image.c',
	'cairo.c',
	'log.c',
	'main.c',
	'pool-buffer.c',
]

swaybg_inc = include_directories('include')

executable('swaybg',
	sources,
	include_directories: [swaybg_inc],
	dependencies: dependencies,
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
