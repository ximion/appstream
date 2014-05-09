# CMakeLists for the AppStream Project
project(appstream)

cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

set(CMAKE_BUILD_TYPE "Debug")

set(AS_VERSION_MAJOR  "0")
set(AS_VERSION_MINOR  "6")
set(AS_VERSION_PATCH  "2")
set(AS_VERSION "${AS_VERSION_MAJOR}.${AS_VERSION_MINOR}.${AS_VERSION_PATCH}")
set(APPSTREAM_LIB_API_LEVEL "0")

# Don't allow in-tree building
if(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})
      message(STATUS "Please do an out-of-tree build:")
      message(STATUS "rm -f CMakeCache.txt && mkdir build && cd build; cmake .. && make")
      message(FATAL_ERROR "In-tree-build detected!")
endif(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})

#
# Options
#
option(PROVIDER_DEBIAN_DEP11 "Enable DEP11 data provider" OFF)
option(PROVIDER_UBUNTU_APPINSTALL "Enable AppInstall data provider" OFF)
option(L18N "Enable localization" ON)
option(VAPI "Create and install a Vala API file" OFF)
option(TESTS "Enable Unit Tests" OFF)
option(MAINTAINER "Enable maintainer mode" OFF)

if(PROVIDER_DEBIAN_DEP11)
	add_definitions ("-DDEBIAN_DEP11")
endif()
if(PROVIDER_UBUNTU_APPINSTALL)
	add_definitions ("-DUBUNTU_APPINSTALL")
endif()

if(MAINTAINER)
	add_definitions ("-Wall" "-Werror")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
    "/usr" CACHE PATH "Default install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(XDG_APPS_INSTALL_DIR share/applications)

add_definitions(-DCMAKE_INSTALL_PREFIX="\"${CMAKE_INSTALL_PREFIX}\"")
add_definitions(-DCMAKE_INSTALL_LIBDIR="\"${CMAKE_INSTALL_LIBDIR}\"")

set(CMAKE_INSTALL_LIBDIR "lib/${CMAKE_LIBRARY_ARCHITECTURE}" CACHE PATH "Output directory for libraries")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/data/cmake/)
message(STATUS "Compiling AppStream version ${AS_VERSION}")

add_custom_target(distclean
  "make clean"
)

#
# l18n
#
find_package (Gettext REQUIRED)

#
# Default paths
#
set(INSTALL_DIR_BIN bin CACHE PATH "Installation directory for executables")
set(INSTALL_DIR_LIB lib${LIB_SUFFIX}/${CMAKE_LIBRARY_ARCHITECTURE} CACHE PATH "Installation directory for libraries")
set(INSTALL_DIR_LIBEXEC "${INSTALL_DIR_LIB}/appstream" CACHE PATH "Installation directory for executables use by a library")
set(INSTALL_DIR_INC include CACHE PATH "Installation directory for headers")
set(INSTALL_DIR_SYSCONF /etc CACHE PATH "Installation directory for system configuration files")

#
# Configure files
#
set (PREFIXDIR "${CMAKE_INSTALL_PREFIX}")
set (DATADIR "${CMAKE_INSTALL_PREFIX}/share")
set (PKGDATADIR "${DATA_INSTALL_DIR}")
set (LIBDIR "${INSTALL_DIR_LIB}")
set (PKGLIBDIR "${INSTALL_DIR_LIBEXEC}")
set (GETTEXT_PACKAGE "appstream")
set (LOCALE_DIR "${DATADIR}/locale")
set (VERSION "${AS_VERSION}")
set (BUILDDIR "${CMAKE_BINARY_DIR}")

configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)

#
# Enable/disable testing
#
if(TESTS)
	enable_testing()
endif(TESTS)

#
# Custom C flags
#
set (MAINTAINER_CFLAGS "-Werror -Wall -Wcast-align -Wuninitialized -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self")
option (DISABLE_MAINTAINER_CFLAGS "Disable maintainer CFlags" ON)
if (DISABLE_MAINTAINER_CFLAGS)
	set (MAINTAINER_CFLAGS "")
endif (DISABLE_MAINTAINER_CFLAGS)
add_definitions(${MAINTAINER_CFLAGS})

# we do always enable this to never run into the missing-include-trap again
add_definitions("-Werror=implicit-function-declaration")

# Required to include config.h before all other units
add_definitions ("--include config.h")

find_package(PkgConfig REQUIRED)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(data)
add_subdirectory(po)
add_subdirectory(docs)
add_subdirectory(tests)
