project(AppstreamQt)
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

set(APPSTREAM_QT_VERSION "4" CACHE STRING "Used Qt version")

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

#     when removing qt4 support, remove this too
    set(QT_QTCORE_LIBRARY Qt5::Core)
endif()

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if (NOT DEFINED INSTALL_DIR_LIB)
    set(INSTALL_DIR_LIB lib${LIB_SUFFIX}/${CMAKE_LIBRARY_ARCHITECTURE} CACHE PATH "Installation directory for libraries")
endif()

if (NOT DEFINED INSTALL_DIR_BIN)
    set(INSTALL_DIR_BIN bin CACHE PATH "Installation directory for executables")
endif()

if (NOT DEFINED INSTALL_DIR_INC)
    set(INSTALL_DIR_INC include CACHE PATH "Installation directory for headers")
endif()

add_subdirectory(src)
