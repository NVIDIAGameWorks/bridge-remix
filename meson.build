#############################################################################
# Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#############################################################################

project(
  'bridge', ['c', 'cpp'],
  meson_version : '>= 0.58',
  default_options : ['werror=true', 'b_vscrt=static_from_buildtype', 'cpp_std=vc++17']
)


output_dir = meson.global_source_root() + '/_output/'

cpu_family = target_machine.cpu_family()
build_os = build_machine.system()

 
# Get script paths
if build_os == 'windows'
  script_extension = '.bat'
  recursive_arg = '/E'
elif build_os == 'linux'
  script_extension = '.sh'
  recursive_arg = '-r'
else
  script_extension = ''
endif
copy_script_path = meson.global_source_root() + '/scripts-common/copy' + script_extension
recursive_copy_path = meson.global_source_root() + '/scripts-common/recursive_copy' + script_extension

add_project_arguments('-DNOMINMAX', language : 'cpp')

if get_option('buildtype') != 'debug'
  add_project_arguments('-DNDEBUG', language: 'cpp')
endif

bridge_compiler = meson.get_compiler('cpp')
bridge_is_msvc = bridge_compiler.get_id() == 'msvc'

useDLSS = true

if bridge_is_msvc
  add_global_arguments('/MP', language : 'cpp')
endif

if bridge_compiler.get_id() == 'clang'
  if bridge_compiler.has_argument('-Wno-unused-private-field')
    add_project_arguments('-Wno-unused-private-field', language: 'cpp')
  endif
  if bridge_compiler.has_argument('-Wno-microsoft-exception-spec')
    add_project_arguments('-Wno-microsoft-exception-spec', language: 'cpp')
  endif
endif

if get_option('enable_multithreaded_device')
  add_project_arguments('-DWITH_MULTITHREADED_DEVICE', language : 'cpp')
endif

if get_option('enable_tracy')
  add_project_arguments('-DTRACY_ENABLE', language : 'cpp')
  # only enable profiling when the profiler-application is connected
  add_project_arguments('-DTRACY_ON_DEMAND', language : 'cpp')
endif

if not bridge_is_msvc
  if get_option('build_id') and bridge_compiler.has_link_argument('-Wl,--build-id')
    add_global_link_arguments('-Wl,--build-id', language: 'cpp')
  endif

  # We need to set the section alignment for debug symbols to
  # work properly as well as avoiding a memcpy from the Wine loader.
  if bridge_compiler.has_link_argument('-Wl,--file-alignment=4096')
    add_global_link_arguments('-Wl,--file-alignment=4096', language: 'cpp')
  endif
endif

bridge_include_path = '' # include_directories('./include')
util_include_path = include_directories('./src/util')

if (cpu_family == 'x86_64')
  bridge_library_path = meson.global_source_root() + '/lib'
else
  bridge_library_path = meson.global_source_root() + '/lib32'
endif

bridge_extradep = [  ]

if bridge_is_msvc
  wrc = find_program('rc')
else
  add_global_link_arguments('-static', '-static-libgcc', language: 'c')
  add_global_link_arguments('-static', '-static-libgcc', '-static-libstdc++', language: 'cpp')

  if cpu_family == 'x86_64'
    wrc = find_program('x86_64-w64-mingw32-windres')
  elif cpu_family == 'x86'
    wrc = find_program('i686-w64-mingw32-windres')
  else
    error('Unsupported target architecture')
  endif
endif

if cpu_family == 'x86_64'
  add_global_arguments('/DREMIX_BRIDGE_SERVER', language : 'cpp')

  if bridge_compiler.has_argument('-msse3')
    add_project_arguments('-msse3', language: ['c', 'cpp'])
  endif
elif cpu_family == 'x86'
  add_global_arguments('/DREMIX_BRIDGE_CLIENT', language : 'cpp')

  if bridge_compiler.has_link_argument('-Wl,--add-stdcall-alias')
    add_global_link_arguments('-Wl,--add-stdcall-alias', language: 'cpp')
  endif
  if bridge_compiler.has_link_argument('-Wl,--enable-stdcall-fixup')
    add_global_link_arguments('-Wl,--enable-stdcall-fixup', language: 'cpp')
  endif
  if bridge_compiler.has_argument('-msse') and bridge_compiler.has_argument('-msse2') and bridge_compiler.has_argument('-msse3')
    add_project_arguments('-msse', '-msse2', '-msse3', language: ['c', 'cpp'])
  endif
  if bridge_compiler.has_argument('-mfpmath=sse')
    add_project_arguments('-mfpmath=sse', language: ['c', 'cpp'])
  endif
endif

lib_version = bridge_compiler.find_library('Version')
lib_commCtrl = bridge_compiler.find_library('Comctl32')

exe_ext = ''
dll_ext = ''

if bridge_is_msvc
  res_ext = '.res'
else
  res_ext = '.o'
endif

def_spec_ext = '.def'

if bridge_is_msvc
  wrc_generator = generator(wrc,
  output    : [ '@BASENAME@' + res_ext ],
  arguments : [ '/fo', '@OUTPUT@', '@INPUT@' ])
else
  wrc_generator = generator(wrc,
  output    : [ '@BASENAME@' + res_ext ],
  arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ])
endif

bridge_version = vcs_tag(
  command: ['git', 'describe', '--dirty=+'],
  input:  'version.h.in',
  output: 'version.h')

subdir('src')
