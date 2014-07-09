project(AppstreamQt)
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

find_package(Qt4 4.8.0 COMPONENTS QtCore REQUIRED)
find_package(Xapian 1.2 REQUIRED)

ENABLE_TESTING()

set(APPSTREAMQT_SO_VERSION 0)
set(APPSTREAMQT_VERSION "0.1")

set(QT_USE_IMPORTED_TARGETS true)

include(${QT_USE_FILE})

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_subdirectory(src)
