# CMakeLists for the Update-AppStream-Index Project
project(update-appstream-index)

cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

set(CMAKE_BUILD_TYPE "Debug")

set(UAI_VERSION_MAJOR  "0")
set(UAI_VERSION_MINOR  "1")
set(UAI_VERSION_PATCH  "0")
set(UAI_VERSION "${UAI_VERSION_MAJOR}.${UAI_VERSION_MINOR}.${UAI_VERSION_PATCH}")

# Don't allow in-tree building
if(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})
      message(STATUS "Please do an out-of-tree build:")
      message(STATUS "rm -f CMakeCache.txt && mkdir build && cd build; cmake .. && make")
      message(FATAL_ERROR "In-tree-build detected!")
endif(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})

#
# Options
#
option(PROVIDER_APPSTREAM "Enable AppStream data provider" ON)
option(PROVIDER_DEBIAN_DEP11 "Enable DEP11 data provider" OFF)
option(PROVIDER_UBUNTU_APPINSTALL "Enable AppInstall data provider" OFF)
option(L18N "Enable localization" ON)
option(TESTS "Enable Unit Tests" ON)

set(VALA_COMPILE_FLAGS "")

if(PROVIDER_APPSTREAM)
	set(VALA_COMPILE_FLAGS ${VALA_COMPILE_FLAGS} --define=APPSTREAM)
endif()
if(PROVIDER_DEBIAN_DEP11)
	set(VALA_COMPILE_FLAGS ${VALA_COMPILE_FLAGS} --define=DEBIAN_DEP11)
endif()
if(PROVIDER_UBUNTU_APPINSTALL)
	set(VALA_COMPILE_FLAGS ${VALA_COMPILE_FLAGS} --define=UBUNTU_APPINSTALL)
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
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/data/cmake/vala)

message(STATUS "Compiling Update-AppStream-Index version ${UAI_VERSION}")

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
set(BIN_INSTALL_DIR bin CACHE PATH "Installation directory for executables")
set(LIB_INSTALL_DIR lib/${CMAKE_LIBRARY_ARCHITECTURE} CACHE PATH "Installation directory for libraries")
set(INC_INSTALL_DIR include CACHE PATH "Installation directory for headers")
set(SYSCONF_INSTALL_DIR etc CACHE PATH "Installation directory for system configuration files")

#
# Configure files
#
set (PREFIXDIR "${CMAKE_INSTALL_PREFIX}")
set (DATADIR "${CMAKE_INSTALL_PREFIX}/share")
set (PKGDATADIR "${DATA_INSTALL_DIR}")
set (LIBDIR "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
set (PKGLIBDIR "${LIBDIR}/update-appstream-index")
set (GETTEXT_PACKAGE "update-appstream-index")
set (LOCALE_DIR "${DATADIR}/locale")
set (VERSION "${UAI_VERSION}")
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
set (MAINTAINER_CFLAGS "-Werror -Wall -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self")
option (DISABLE_MAINTAINER_CFLAGS "Disable maintainer CFlags" ON)
if (DISABLE_MAINTAINER_CFLAGS)
	set (MAINTAINER_CFLAGS "")
endif (DISABLE_MAINTAINER_CFLAGS)
add_definitions(${MAINTAINER_CFLAGS})

# Required to include config.h before all other units
add_definitions ("--include config.h")

#
# Validate Vala version
#
find_package(PkgConfig REQUIRED)
find_package(Vala REQUIRED)
include(${VALA_USE_FILE})
ensure_vala_version("0.16" MINIMUM)

add_subdirectory(src)
add_subdirectory(data)
if(TESTS)
	add_subdirectory(tests)
endif(TESTS)
