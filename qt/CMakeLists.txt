project(AppstreamQt)
cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

find_package(Xapian 1.2 REQUIRED)
find_package(Protobuf REQUIRED)

ENABLE_TESTING()

if(${AS_VERSION} STREQUAL "")
    set(AS_VERSION "0.8.4")
endif()

find_package(Qt5 REQUIRED COMPONENTS Core)

set(APPSTREAMQT_SO_VERSION 1)
set(APPSTREAMQT_VERSION ${AS_VERSION})

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
