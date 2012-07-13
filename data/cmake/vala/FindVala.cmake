# - Find and use the Vala compiler.
#
# This module locates the Vala compiler and related tools and provides a set of
# functions which can be used to compile Vala (.vala) and Genie (.gs) sources.
# Typical usage could be
#  find_package(Vala REQUIRED)
#  include("${VALA_USE_FILE}")
#  include_directories(${VALA_INCLUDE_DIRS})
#  vala_add_executable(myexe source.vala)
# This first line finds the Vala compiler installation and the second line
# includes utility functions. The last line creates the executable myexe from
# source.vala. Similarly, the function vala_add_library is available to create
# libraries. For more sophisticated needs the function vala_precompile
# generates C sources and headers from a given list of Vala and Genie sources
# which then can be further processed using other CMake facilities. If you do
# so, please note that you should use vala_add_dependencies instead of the
# plain add_dependencies to create the dependencies in order for the
# specialized target properties and dependencies of the Vala targets to be
# properly propagated.
#
# The module provides the optional component "Development" which also finds the
# Vala development header and library. This component is only required if you
# want to use the Vala compiler infrastructure inside your program.
#
#  vala_precompile(<target> <outvar> <source1> ... <sourceN>
#                  [LIBRARY <libname>]
#                  [PACKAGES <pkg> ...]
#                  [VAPI_DIRS <vapidir> ...]
#                  [CUSTOM_VAPIS <vapifile> ...]
#                  [GENERATE_VAPI <vapifile>]
#                  [GENERATE_INTERNAL_VAPI <vapifile>]
#                  [GENERATE_HEADER <hfile>]
#                  [GENERATE_INTERNAL_HEADER <hfile>]
#                  [BASE_DIR <basedir>]
#                  [COMPILE_FLAGS <option> ...]
#                  [OUTPUT_DIR <outputdir>] [COMMENT <string>])
#
# Adds a custom target <target> to precompile the given Vala and Genie sources
# <source1> ... <sourceN> using the executable named by the VALA_COMPILER
# variable (i.e. valac). The resulting C source and header files are listed in
# the <outvar> variable and can be further processed using e.g. add_library or
# add_executable. The option LIBRARY can be used to signal that the listed
# sources will be compiled into a library and that the Vala compiler should
# generate an interface file <libname>.vapi in the output directory (i.e. this
# option passes the "--library=<libname>" flag to the Vala compiler). The
# strings following PACKAGES are a list of either package names to be passed to
# the Vala compiler using the "--pkg=<pkg>" option or CMake target names. In
# the latter case, the target must have the property VAPI_FILES set to the path
# to its .vapi file and all CUSTOM_VAPIS it depends on (the functions provided
# in this module do this automatically). A dependency of <target> on the target
# providing the .vapi file will be automatically added. The PACKAGES property
# is inherited by other Vala targets that use <target> in their own PACKAGES
# list. To search additional directories for .vapi files, list them after
# VAPI_DIRS. If not specified, the variable VALA_VAPI_DIRS is used if defined.
# To include a custom list of .vapi files in the compilation, list them after
# CUSTOM_VAPIS. This can be useful to include freshly created vala libraries
# without having to install them in the system. If LIBRARY is used and you want
# to change the name of the generated .vapi file or its output path (or you
# want to generate a .vapi file for a target without LIBRARY), you can use
# GENERATE_VAPI <vapifile>. If <vapifile> is not an absolute path, it is
# relative to the output directory (see below). An internal .vapi file can be
# created by passing GENERATE_INTERNAL_VAPI <vapifile>. To generate a public
# C-header <hfile>, use GENERATE_HEADER <hfile>. For an internal header file,
# use GENERATE_INTERNAL_HEADER <hfile> instead. If the names are not absolute
# paths, the output will be placed in the output directory. The
# BASE_DIR <basedir> option allows to set a different base source directory
# than ${CMAKE_CURRENT_SOURCE_DIR}. If the path is not absolute, it will be
# prefixed by ${CMAKE_CURRENT_SOURCE_DIR}. COMPILE_FLAGS can be used to pass
# additional options to the Vala compiler (e.g. --thread for multi-threading
# support). This option is initialized by the variable VALA_COMPILE_FLAGS and
# VALA_COMPILE_FLAGS_<CONFIG> and the new flags are appended to it. If you want
# to use a different output directory for the generated files than
# CMAKE_CURRENT_BINARY_DIR, specify OUTPUT_DIR <outdir>. If the path is not
# absolute, it will be prefixed by CMAKE_CURRENT_BINARY_DIR. By default the
# generated custom command will display "Precompiling Vala target <target>". To
# change this, pass the COMMENT <string> option.
#
#  vala_add_executable(<name> <source1> ... <sourceN>
#                      [PACKAGES <pkg> ...]
#                      [VAPI_DIRS <vapidir> ...]
#                      [CUSTOM_VAPIS <vapifile> ...]
#                      [BASE_DIR <basedir>]
#                      [COMPILE_FLAGS <flag> ...]
#                      [OUTPUT_DIR <outputdir>]
#                      [COMMENT <string>])
#
# Adds an executable <name>, just like add_executable does. All the options
# have the same meaning as for vala_precompile. The generated C sources are
# available through the VALA_C_SOURCES target property.
#
#  vala_add_library(<name> [STATIC | SHARED | MODULE]
#                   <source1> ... <sourceN>
#                   [LIBRARY <libname>]
#                   [PACKAGES <pkg> ...] [VAPI_DIRS <vapidir> ...]
#                   [CUSTOM_VAPIS <vapifile> ...]
#                   [GENERATE_VAPI <vapifile>]
#                   [GENERATE_INTERNAL_VAPI <vapifile>]
#                   [GENERATE_HEADER <hfile>]
#                   [GENERATE_INTERNAL_HEADER <hfile>]
#                   [BASE_DIR <basedir>]
#                   [COMPILE_FLAGS <option> ...]
#                   [OUTPUT_DIR <outputdir>] [COMMENT <string>])
#
# Adds a library <name>, just like add_library does. All the options have the
# same meaning as for vala_precompile. <name> will also be passed to the
# LIBRARY option of vala_precompile unless overriden by LIBRARY. As for
# vala_add_executable, the generated C sources can be retrieved from the
# VALA_C_SOURCES target property.
#
#  vala_add_dependencies(<target> <depend1> [<depend2> ...])
#
# Make <target> depend on the top-level targets <depend1> ... in the same way
# the standard add_dependencies does. If any of the dependencies is a Vala
# target, <target> will inherit the properties VAPI_FILES and
# VALA_PACKAGE_DEPENDENCIES which are used to implement transitive Vala
# dependencies. This is useful to e.g. make a target created by add_library to
# depend on the target created by vala_precompile and make the library target a
# viable target for the PACKAGES option of the Vala functions.
#
# The module sets the following variables:
#  VALA_FOUND           TRUE if the Vala compiler has been found
#  VALA_USE_FILE        The path of the file that must be included in the
#                       CMakeLists.txt in order to use the above functions.
#  VALA_INCLUDE_DIRS    Include-directories required to compile
#                       generated C code (GLib and GObject).
#  VALA_LIBRARIES       Libraries to link your Vala targets
#                       against (GLib and GObject).
#  VALA_COMPILER        The valac compiler executable.
#  VALA_VAPIGEN_COMPILER           The vapigen executable (if found)
#  VALA_VAPICHECK_EXECUTABLE       The vapicheck executable (if found)
#  VALA_GEN_INTROSPECT_EXECUTABLE  The vala-gen-introspect wrapper
#                       shell script (requires a CMake generator
#                       that uses a sensible shell interpreter to
#                       run it, i.e. Bourne-shell compatible).
#  VALA_VERSION         Version of the Vala compiler.
#  VALA_DEV_LIBRARIES   The libraries to link against if you want
#                       to use the Vala compiler infrastructure.
#                       Normal libraries and executables DO NOT
#                       need to link against these libraries! Only available if
#                       the component "Development" has been requested.
#  VALA_DEV_INCLUDE_DIR Include directory if you want to use the
#                       Vala compiler infrastructure. As for
#                       VALA_DEV_LIBRARIES, most projects won't
#                       need to use this variable. Only available if the
#                       component "Development" has been requested.
#
# Properties on Vala targets:
#  PUBLIC_VAPI_FILE     If a Vala target is created with GENERATE_VAPI or the
#                       LIBRARY option, the location of the generated public
#                       .vapi file is stored in this property.
#  INTERNAL_VAPI_FILE   If a Vala target is created with GENERATE_INTERNAL_VAPI
#                       option, the location of the generated internal .vapi
#                       file is stored in this property.
#  VAPI_FILES           Names the .vapi file describing the C GLib/GObject
#                       interface defined by a Vala library in the Vala
#                       language such that it can be used by other Vala code
#                       and all other .vapi files this library depends on.
#  VALA_PACKAGE_DEPENDENCIES Target names of Vala libraries that have been
#                       passed using the PACKAGES option of vala_precompile,
#                       vala_add_executable or vala_add_library on which this
#                       target depends. This property is inherited by other
#                       targets that list this target in their PACKAGES option.
#  VALA_C_SOURCES       The functions vala_add_executable and vala_add_library
#                       store in this property the paths of the generated
#                       C-sources as returned by vala_precompile.

#=============================================================================
# Copyright 2009-2010 Michael Wild, Kitware Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

SET(VALA_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})
SET(VALA_USE_FILE ${CMAKE_CURRENT_LIST_DIR}/UseVala.cmake)

# search for the valac executable in the usual system paths.
find_program(VALA_COMPILER
  NAMES valac)

set(_fv_vars VALA_COMPILER)

if(Vala_FIND_COMPONENTS)
  if(NOT Vala_FIND_COMPONENTS STREQUAL Development)
    message(SEND_ERROR "Unknown component")
  endif()
  if(VALA_COMPILER)
    get_filename_component(_fv_prefix "${VALA_COMPILER}" PATH)
    # search for vala.h
    find_path(VALA_DEV_INCLUDE_DIR vala.h
      HINTS "${_fv_prefix}/include"
      PATH_SUFFIXES vala-1.0
      )
    # search for libvala
    find_library(VALA_vala_LIBRARY vala
      HINTS "${_fv_prefix}/lib"
      )
  endif(VALA_COMPILER)
  list(APPEND _fv_vars VALA_DEV_INCLUDE_DIR VALA_vala_LIBRARY)
endif()

# determine the valac version
set(VALA_VERSION)
if(VALA_COMPILER)
  execute_process(COMMAND ${VALA_COMPILER} "--version"
    OUTPUT_VARIABLE VALA_VERSION
    )
  string(REPLACE "Vala" "" VALA_VERSION "${VALA_VERSION}")
  string(STRIP "${VALA_VERSION}" VALA_VERSION)
endif()

# check that the version is good
if(Vala_FIND_VERSION AND VALA_VERSION)
  set(VALA_VERSION_CHECK TRUE)
  if(${VALA_VERSION} VERSION_LESS ${Vala_FIND_VERSION})
    set(VALA_VERSION_CHECK FALSE)
  endif()
  if(Vala_FIND_VERSION_EXACT AND
      NOT ${VALA_VERSION} VERSION_EQUAL ${Vala_FIND_VERSION})
    set(VALA_VERSION_CHECK FALSE)
  endif()
  list(APPEND _fv_vars VALA_VERSION_CHECK)
endif()

# find tools
find_program(VALA_VAPIGEN_COMPILER vapigen)
find_program(VALA_VAPICHECK_EXECUTABLE vapicheck)
find_program(VALA_GEN_INTROSPECT_EXECUTABLE vala-gen-introspect)

# require Glib
if(VALA_VERSION AND VALA_VERSION VERSION_LESS 0.8.0)
  set(_fv_glib_version 2.12.0)
else()
  set(_fv_glib_version 2.14.0)
endif()
find_package(GLIB2 ${_fv_glib_version})
list(APPEND _fv_vars GLIB2_FOUND)

# handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vala DEFAULT_MSG ${_fv_vars})

mark_as_advanced(VALA_COMPILER VALA_DEV_INCLUDE_DIR VALA_vala_LIBRARY
  VALA_VAPIGEN_COMPILER VALA_VAPICHECK_EXECUTABLE
  VALA_GEN_INTROSPECT_EXECUTABLE)

# set uncached variables
set(VALA_INCLUDE_DIRS "${GLIB2_INCLUDE_DIRS}")
set(VALA_LIBRARIES "${GLIB2_LIBRARIES}")
set(VALA_DEV_LIBRARIES "${VALA_vala_LIBRARY}")

