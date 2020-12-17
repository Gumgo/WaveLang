import SCons.Errors

import build_config

env = Environment()

platform = ARGUMENTS.get("platform", Platform())
configuration = ARGUMENTS.get("config", "development")

try:
	# Convert to Enum
	configuration = build_config.Configuration(configuration)
except ValueError:
	raise SCons.Errors.BuildError("Unknown configuration '{}'".format(configuration))

print("Building '{}' configuration on platform '{}'".format(configuration.value, platform))

build_options_filename = "build_options.json"
build_options = build_config.BuildOptions()
if not build_options.read(build_options_filename):
	build_options.write(build_options_filename)
	print("{} has been created, edit this file and rebuild".format(build_options_filename))
	Exit(1)

# Setup configuration- and platform-specific flags
compiler = env["CC"]
cc_flags = []
link_flags = []
cpp_defines = []

optimize = None
debug = None
if configuration is build_config.Configuration.DEBUG:
	optimize = False
	debug = True
elif configuration is build_config.Configuration.DEVELOPMENT:
	optimize = True
	debug = True
elif configuration is build_config.Configuration.RELEASE:
	optimize = True
	debug = False
else:
	assert False

if compiler == "cl":
	cc_flags += [
		"/permissive-", 		# Warn about non-standard code
		"/W3",					# Warning level
		"/sdl",					# Enable Security Development Lifecycle checks
		"/Zc:inline",			# Remove unreferenced functions
		"/fp:precise",			# Preserve floating-point expression ordering and rounding
		"/WX",					# Treat warnings as errors
		"/FC",					# Display full path of source files in compiler messages
		"/EHsc",				# Standard C++ stack unwinding, and assume extern "C" functions don't throw exceptions
		"/nologo",				# Suppress startup compiler banner
		"/diagnostics:column",	# Identify the column on which errors/warnings were found
		"/std:c++latest",		# Use the latest C++ standard available
		"/FS"					# Allows synchonous PDB writes for multiprocess builds
	]

	arch_flag = {
		build_config.SIMDSupport.NONE: None,
		build_config.SIMDSupport.AVX: "/arch:AVX",
		build_config.SIMDSupport.AVX2: "/arch:AVX2"
	}[build_options.simd_support]
	if arch_flag is not None:
		cc_flags.append(arch_flag)

	link_flags += [
		"/MACHINE:X64",
		"/SUBSYSTEM:CONSOLE",
		"/NOLOGO"
	]

	cpp_defines += [
		"_MBCS"	# Support multi-byte character sets - $TODO change this to _UNICODE
	]

	if optimize:
		cc_flags += [
			"/GL",	# Whole-program optimization
			"/Gy",	# Enable function-level linking
			"/Zi",	# Product separate PDB file for debugging
			"/O2",	# Optimize for speed
			"/Oi"	# Replace some function calls with intrinsics
		]

		link_flags += [
			"/LTCG:incremental",
			"/DEBUG:FULL",
			"/OPT:REF",
			"/OPT:ICF"
		]
	else:
		cc_flags += [
			"/ZI",	# Product separate PDB file for debugging which includes edit-and-continue support
			"/Od",	# Disable optimizations for debugging
			"/RTC1"	# Enable runtime checks for stack frame and uninitialized variables
		]

		link_flags += [
			"/DEBUG:FASTLINK",
			"/INCREMENTAL"
		]

	# $TODO perhaps switch to static CRT (/MT, /MTd) in the future
	if debug:
		cc_flags += [
			"/MDd"	# Use debug multithread DLL-specific version of the runtime library
		]
	else:
		cc_flags += [
			"/MD"	# Use multithread DLL-specific version of the runtime library
		]
else:
	raise SCons.Errors.BuildError("Unsupported compiler '{}'".format(platform))

if not debug:
	cpp_defines += ["NDEBUG"]

env.Append(CCFLAGS = cc_flags)
env.Append(LINKFLAGS = link_flags)
env.Append(CPPDEFINES = cpp_defines)

variant_dir = "build/{}/{}".format(platform, configuration.value)

Export("env", "platform", "configuration", "compiler", "build_options")

projects = [
	"common",
	"compiler",
	"console",
	"driver",
	"engine",
	"execution_graph",
	"native_module",
	"parser_generator",
	"runtime",
	"scraper",
	"task_function",
	"unit_tests"
]

for project in projects:
	SConscript(
		"source/{}/SConscript".format(project),
		variant_dir = "{}/{}".format(variant_dir, project),
		duplicate = 0)
