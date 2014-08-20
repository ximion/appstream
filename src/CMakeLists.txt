# CMakeLists for AppStream database shared lib

find_package(GLIB2 2.36 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(Xapian 1.2 REQUIRED)
find_package(GI REQUIRED)
find_package(LibXml2 REQUIRED)
find_library(M_LIB m)

include(GObjectIntrospectionMacros)
set(INTROSPECTION_GIRS)
set(INTROSPECTION_SCANNER_ARGS "--identifier-prefix=As" "--symbol-prefix=as" "--warn-all" "--add-include-path=${CMAKE_CURRENT_SOURCE_DIR}")
set(INTROSPECTION_COMPILER_ARGS "--includedir=${CMAKE_CURRENT_SOURCE_DIR}")

set (srcdir ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Source dir")
set(AS_VERSION_BASE "${AS_VERSION_MAJOR}.${AS_VERSION_MINOR}" CACHE INTERNAL "LibAppStream combined version number")

set(ASLIB_XAP_SOURCES xapian/database-common.hpp
		xapian/database-read.hpp
		xapian/database-read.cpp
		xapian/database-write.hpp
		xapian/database-write.cpp
		xapian/database-cwrap.hpp
		xapian/database-cwrap.cpp
)

set(LIBAPPSTREAM_SRC
		as-utils.c
		# internal stuff to build the database
		as-data-provider.c
		data-providers/appstream-xml.c
		data-providers/ubuntu-appinstall.c
		as-cache-builder.c
		# (mostly) public API
		as-metadata.c
		as-component.c
		as-enums.c
		as-provides.c
		as-data-pool.c
		as-search-query.c
		as-database.c
		as-category.c
		as-menu-parser.c
		as-distro-details.c
		as-screenshot.c
		as-image.c
		as-release.c
		as-validator.c
		as-validator-issue.c
)

# find stuff necessary to build libappstream with DEP-11 support
if(PROVIDER_DEBIAN_DEP11)
	pkg_check_modules(YAML REQUIRED yaml-0.1)
	set(LIBAPPSTREAM_SRC ${LIBAPPSTREAM_SRC} data-providers/debian-dep11.c)
endif()

set(LIBAPPSTREAM_PUBLIC_HEADERS
		appstream.h
		as-utils.h
		as-metadata.h
		as-component.h
		as-data-pool.h
		as-enums.h
		as-provides.h
		as-search-query.h
		as-database.h
		as-category.h
		as-menu-parser.h
		as-distro-details.h
		as-screenshot.h
		as-image.h
		as-release.h
		as-validator.h
		as-validator-issue.h
)

set(LIBAPPSTREAM_PRIVATE_HEADERS
		as-utils-private.h
		as-data-provider.h
		data-providers/appstream-xml.h
		data-providers/debian-dep11.h
		data-providers/ubuntu-appinstall.h
		as-metadata-private.h
		as-component-private.h
		as-cache-builder.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xapian
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${LIBXML2_INCLUDE_DIR}
		    ${XAPIAN_INCLUDE_DIR}
		    ${YAML_INCLUDE_DIR}
)

add_definitions("-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE"
	"-DI_KNOW_THE_PACKAGEKIT_PLUGIN_API_IS_SUBJECT_TO_CHANGE"
	"-DAS_COMPILATION"
)

# ensure that the GI compiler has been found (apparently, the automatic tests fail on some machines)
if (INTROSPECTION_COMPILER STREQUAL "")
  message(FATAL_ERROR "GObject-Introspection was not found! Please install it to continue.")
endif()

add_library(appstream SHARED ${ASLIB_XAP_SOURCES} ${LIBAPPSTREAM_SRC} ${LIBAPPSTREAM_PUBLIC_HEADERS} ${LIBAPPSTREAM_PRIVATE_HEADERS})
set_target_properties(appstream PROPERTIES VERSION ${AS_VERSION} SOVERSION ${APPSTREAM_LIB_API_LEVEL})

target_link_libraries(appstream
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${LIBXML2_LIBRARIES}
		${XAPIAN_LIBRARIES}
		${YAML_LIBRARIES}
)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/appstream.pc.in ${CMAKE_CURRENT_BINARY_DIR}/appstream.pc @ONLY)

####
# GObject Introspection
macro(_list_prefix _outvar _listvar _prefix)
  set(${_outvar})
  foreach(_item IN LISTS ${_listvar})
    list(APPEND ${_outvar} ${_prefix}${_item})
  endforeach()
endmacro(_list_prefix)

set(introspection_files ${LIBAPPSTREAM_SRC} ${LIBAPPSTREAM_PUBLIC_HEADERS})
set(Appstream_0_7_gir "appstream")
set(Appstream_0_7_gir_INCLUDES GObject-2.0 Gio-2.0)
set(Appstream_0_7_gir_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/" "-DAS_COMPILATION")
set(Appstream_0_7_gir_LIBS appstream)

_list_prefix(Appstream_0_7_gir_FILES introspection_files "${CMAKE_CURRENT_SOURCE_DIR}/")
set(Appstream_0_7_gir_SCANNERFLAGS --c-include appstream.h)
set(Appstream_0_7_gir_EXPORT_PACKAGES appstream)

list(APPEND INTROSPECTION_GIRS Appstream-0.7.gir)
gir_add_introspections(INTROSPECTION_GIRS)
####

install(TARGETS appstream DESTINATION ${INSTALL_DIR_LIB})
install(FILES ${LIBAPPSTREAM_PUBLIC_HEADERS} DESTINATION include/Appstream)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appstream.pc DESTINATION ${INSTALL_DIR_LIB}/pkgconfig/)

# include the PackageKit plugin
add_subdirectory(pk-plugin)
