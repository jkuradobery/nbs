
# This file was gererated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.


if (MSVC)
  include(CMakeLists.windows-x86_64.txt)
elseif (APPLE)
  include(CMakeLists.darwin-x86_64.txt)
endif()
