PROJECT(asmara)
cmake_minimum_required(VERSION 2.8)
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

ENABLE_TESTING()

SET(ASMARA_SO_VERSION 0)
SET(ASMARA_VERSION "0.1")

set(QT_USE_IMPORTED_TARGETS true)
FIND_PACKAGE(Qt4 4.8.0 COMPONENTS QtCore REQUIRED)
include(${QT_USE_FILE})

find_package(Xapian 1.2 REQUIRED)

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_subdirectory(src)

