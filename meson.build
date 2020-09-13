project(
	'swaybg',
	'c',
	version: '1.0',
	license: 'MIT',
	meson_version: '>=0.48.0',
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

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
cairo          = dependency('cairo')
gdk_pixbuf     = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf'))

git = find_program('git', required: false)
scdoc = find_program('scdoc', required: get_option('man-pages'))
wayland_scanner = find_program('wayland-scanner')

version = '"@0@"'.format(meson.project_version())
if git.found()
	git_commit_hash = run_command([git.path(), 'describe', '--always', '--tags'])
	git_branch = run_command([git.path(), 'rev-parse', '--abbrev-ref', 'HEAD'])
	if git_commit_hash.returncode() == 0 and git_branch.returncode() == 0
		version = '"@0@ (" __DATE__ ", branch \'@1@\')"'.format(git_commit_hash.stdout().strip(), git_branch.stdout().strip())
	endif
endif
add_project_arguments('-DSWAYBG_VERSION=@0@'.format(version), language: 'c')

wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

if wayland_client.version().version_compare('>=1.14.91')
	code_type = 'private-code'
else
	code_type = 'code'
endif

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: [code_type, '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protos_src = []
client_protos_headers = []

client_protocols = [
	[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	[wl_protocol_dir, 'unstable/xdg-output/xdg-output-unstable-v1.xml'],
	['wlr-layer-shell-unstable-v1.xml'],
]

foreach p : client_protocols
	xml = join_paths(p)
	client_protos_src += wayland_scanner_code.process(xml)
	client_protos_headers += wayland_scanner_client.process(xml)
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

conf_data = configuration_data()
conf_data.set10('HAVE_GDK_PIXBUF', gdk_pixbuf.found())

subdir('include')

dependencies = [
	cairo,
	client_protos,
	gdk_pixbuf,
	wayland_client,
]

sources = [
	'background-image.c',
	'cairo.c',
	'log.c',
	'main.c',
	'dir.c',
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
	sh = find_program('sh')
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
			command: [
				sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc.path(), output)
			],
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif
