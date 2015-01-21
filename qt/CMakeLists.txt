project(AppstreamQt)
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

set(APPSTREAM_QT_VERSION "5" CACHE STRING "Used Qt version")

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

find_package(Xapian 1.2 REQUIRED)

ENABLE_TESTING()

if(APPSTREAM_QT_VERSION STREQUAL "4")
    find_package(Qt4 4.8.0 COMPONENTS QtCore REQUIRED)

    set(APPSTREAMQT_SO_VERSION 0)
    set(APPSTREAMQT_VERSION "0.1")

    set(QT_USE_IMPORTED_TARGETS true)

    include(${QT_USE_FILE})
else()
    find_package(Qt5 REQUIRED COMPONENTS Core)

    set(APPSTREAMQT_SO_VERSION 1)
    set(APPSTREAMQT_VERSION "1.1")

    # when removing qt4 support, remove this too
    set(QT_QTCORE_LIBRARY Qt5::Core)
endif()

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

# Paths
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
    "/usr" CACHE PATH "Default install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Special case for /etc and /var when prefix is /usr
if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
  set(CMAKE_INSTALL_SYSCONFDIR "/etc" CACHE PATH "read-only single-machine data (etc)")
  set(CMAKE_INSTALL_LOCALSTATEDIR "/var" CACHE PATH "modifiable single-machine data (var)")
endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
include(GNUInstallDirs)

add_subdirectory(src)
