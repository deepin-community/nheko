project('coeurl', 'cpp',
  version : '0.3.0',
  license : 'MIT',
  meson_version : '>=0.54',
  default_options : ['warning_level=3', 'cpp_std=c++17']
  )

deps = [
    dependency('threads', required: true),
    dependency('spdlog', required: true)
  ]

libcurl_dep = dependency('libcurl', required: get_option('wrap_mode') == 'nofallback')

libevent_dep = dependency('libevent_core', required: get_option('wrap_mode') == 'nofallback',)
if target_machine.system() == 'windows'
  libevent_threads_dep = dependency('libevent_windows', required: get_option('wrap_mode') == 'nofallback',)
else
  libevent_threads_dep = dependency('libevent_pthreads', required: get_option('wrap_mode') == 'nofallback',)
endif

if (not libevent_dep.found()
  or not libevent_threads_dep.found()
  or get_option('wrap_mode') == 'forcefallback'
  or 'libevent' in get_option('force_fallback_for'))
cmake = import('cmake')
libevent_options = cmake.subproject_options()
    libevent_options.add_cmake_defines({
      'EVENT__LIBRARY_TYPE': 'STATIC',
      'BUILD_SHARED_LIBS': false,
      'BUILD_SHARED_AND_STATIC_LIBS': false,
      'EVENT__DISABLE_OPENSSL': true,
      'EVENT__DISABLE_BENCHMARK': true,
      'EVENT__DISABLE_TESTS': true,
      'EVENT__DISABLE_REGRESS': true,
      'EVENT__DISABLE_SAMPLES': true,
      'EVENT__MSVC_STATIC_RUNTIME': ['mt', 'mtd', 'static_from_buildtype'].contains(get_option('b_vscrt')),
      })
    if target_machine.system() != 'windows'
        libevent_options.add_cmake_defines({
          'CMAKE_C_FLAGS':  '-fPIC',
        })
    endif
    libevent_options.set_override_option('werror', 'false')
    libevent_options.set_override_option('warning_level', '0')
    libevent_proj = cmake.subproject('libevent', options: libevent_options)
    libevent_dep = libevent_proj.dependency('event_core_static')

    if target_machine.system() == 'windows'
      # the cmake project links the thread support into the library directly...
      libevent_threads_dep = dependency('', required : false)
    else
      libevent_threads_dep = libevent_proj.dependency('event_pthreads_static')
    endif
endif

deps += [libevent_dep, libevent_threads_dep]

if (not libcurl_dep.found()
  or get_option('wrap_mode') == 'forcefallback'
  or 'libcurl' in get_option('force_fallback_for'))
cmake = import('cmake')
libcurl_options = cmake.subproject_options()
    libcurl_options.add_cmake_defines({
      'BUILD_SHARED_LIBS': false,
      'BUILD_SHARED_AND_STATIC_LIBS': false,
      'HTTP_ONLY': true,
      'CMAKE_USE_LIBSSH2': false,
      })
    if target_machine.system() != 'windows'
        libcurl_options.add_cmake_defines({
          'CMAKE_C_FLAGS':  '-fPIC',
        })
    endif
    libcurl_options.set_override_option('werror', 'false')
    libcurl_options.set_override_option('warning_level', '0')
    libcurl_proj = cmake.subproject('curl', options: libcurl_options)
    libcurl_dep = libcurl_proj.dependency('libcurl')
endif

deps += [libcurl_dep]
include = include_directories('include')

defines = []
if target_machine.system() == 'windows'
  defines += ['-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN']
endif

if target_machine.system() == 'windows'
  cc = meson.get_compiler('c')
  deps += [
		cc.find_library('ws2_32', required: true),  # socket functions
		cc.find_library('iphlpapi', required: true) # if_nametoindex needed for libevent
  ]
endif

version_parts = meson.project_version().split('.')
soversion = version_parts.get(0) + '.' + version_parts.get(1)

lib = library('coeurl', ['lib/client.cpp', 'lib/request.cpp', 'lib/errors.cpp'],
  include_directories: include,
  cpp_args : defines,
  dependencies: deps,
  #version: meson.project_version(),
  soversion: soversion,
  install : true)

install_headers('include/coeurl/headers.hpp', 'include/coeurl/client.hpp', 'include/coeurl/request.hpp', 'include/coeurl/errors.hpp', subdir : 'coeurl')

coeurl_dep = declare_dependency(include_directories: include, link_with: lib, dependencies: deps)
meson.override_dependency('coeurl', coeurl_dep)

if get_option('examples')
  subdir('examples')
endif

if get_option('tests')
  subdir('tests')
endif

pkg = import('pkgconfig')
pkg.generate(lib,
  libraries : [lib],
  version : meson.project_version(),
  filebase : meson.project_name(),
  description : 'Simple library to do http requests asynchronously via CURL in C++.',
  url : 'https://nheko.im/nheko-reborn/coeurl')

# No idea how to export working cmake config files, so don't do that for now
#cmake = import('cmake')
#cmake.write_basic_package_version_file(name: meson.project_name(), version: meson.project_version())
#cmake.configure_package_config_file(
#    name: 'coeurl',
#    input: 'cmake/coeurl.cmake.in',
#    configuration: configuration_data()
#)
