# CMakeLists for AppStream database shared lib

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 2.36 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(Xapian 1.2 REQUIRED)
find_package(GI REQUIRED)
find_package(LibXml2 REQUIRED)
find_library(M_LIB m)

set (srcdir ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Source dir")

set(AS_VERSION_LIBS "${AS_VERSION_MAJOR}.${AS_VERSION_MINOR}" CACHE INTERNAL "LibAppStream combined version number")

set(APPSTREAM_LIB_SOURCES settings.vala
)

set(ASLIB_XAP_SOURCES xapian/database-common.hpp
		xapian/database-read.hpp
		xapian/database-read.cpp
		xapian/database-write.hpp
		xapian/database-write.cpp
		xapian/database-vala.hpp
		xapian/database-vala.cpp
)

set(LIBAPPSTREAM_SRC_C
		as-utils.h
		as-utils.c
		# internal stuff to build the database
		as-data-provider.h
		as-data-provider.c
		data-providers/appstream-xml.h
		data-providers/appstream-xml.c
		data-providers/debian-dep11.h
		data-providers/debian-dep11.c
		data-providers/ubuntu-appinstall.h
		data-providers/ubuntu-appinstall.c
		# (mostly) public API
		as-component.h
		as-component-private.h
		as-component.c
		as-database-builder.h
		as-database-builder.c
		as-search-query.h
		as-search-query.c
		as-database.h
		as-database.c
		as-database-write.h
		as-database-write.c
		as-category.h
		as-category.c
		as-menu-parser.h
		as-menu-parser.c
		as-distro-details.h
		as-distro-details.c
)

set(LIBAPPSTREAM_PUBLIC_HEADERS
		appstream.h
		as-component.h
		as-search-query.h
		as-database.h
		as-category.h
		as-menu-parser.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xapian
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${LIBXML2_INCLUDE_DIR}
		    ${XAPIAN_INCLUDE_DIR}
)

add_definitions("-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE"
	"-DI_KNOW_THE_PACKAGEKIT_PLUGIN_API_IS_SUBJECT_TO_CHANGE"
)

vala_precompile(aslib-precomp ASLIB_SRC_C ${APPSTREAM_LIB_SOURCES}
			PACKAGES config gio-2.0 posix libxml-2.0
			VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi
				${CMAKE_CURRENT_SOURCE_DIR}/xapian
			GENERATE_INTERNAL_HEADER appstream_internal.h
			GENERATE_HEADER appstream.h
			COMPILE_FLAGS ${VALA_COMPILE_FLAGS}
				--gir=Appstream-${AS_VERSION_LIBS}.gir
				--library appstream
				--internal-vapi=appstream_internal.vapi.orig
)

# ensure that the GI compiler has been found (apparently, the automatic tests fail on some machines)
if (INTROSPECTION_COMPILER STREQUAL "")
  message(FATAL_ERROR "GObject-Introspection was not found! Please install it to continue.")
endif()

find_program (SED_TOOL NAMES sed)
if (NOT SED_TOOL)
	message (FATAL_ERROR "Unable to find sed")
endif (NOT SED_TOOL)

# Workaround to fix header filename in internal vapi
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/appstream_internal.vapi
	COMMAND ${SED_TOOL}  "'s/\"appstream.h\"/\"appstream_internal.h\"/'" ${CMAKE_CURRENT_BINARY_DIR}/appstream_internal.vapi.orig ">" ${CMAKE_CURRENT_BINARY_DIR}/appstream_internal.vapi
	DEPENDS aslib-precomp ${CMAKE_CURRENT_BINARY_DIR}/appstream_internal.vapi.orig
)
add_custom_target(header_internal_fix DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/appstream_internal.vapi)

# Compile the introspection data
add_custom_target (gir-compile
	${INTROSPECTION_COMPILER}  "--shared-library=appstream" "--includedir=."
				"${CMAKE_CURRENT_BINARY_DIR}/Appstream-${AS_VERSION_LIBS}.gir"
				"-o" "${CMAKE_CURRENT_BINARY_DIR}/Appstream-${AS_VERSION_LIBS}.typelib"
	DEPENDS aslib-precomp
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(appstream SHARED ${ASLIB_SRC_C} ${ASLIB_XAP_SOURCES} ${LIBAPPSTREAM_SRC_C})
set_target_properties(appstream PROPERTIES VERSION ${AS_VERSION} SOVERSION ${APPSTREAM_LIB_API_LEVEL})

add_dependencies(appstream aslib-precomp)
add_dependencies(appstream header_internal_fix)
add_dependencies(appstream gir-compile)

target_link_libraries(appstream
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${LIBXML2_LIBRARIES}
		${XAPIAN_LIBRARIES}
)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/appstream.pc.in ${CMAKE_CURRENT_BINARY_DIR}/appstream.pc @ONLY)

install(TARGETS appstream DESTINATION ${INSTALL_DIR_LIB})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appstream.vapi DESTINATION share/vala/vapi)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appstream.h DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appstream.pc DESTINATION ${INSTALL_DIR_LIB}/pkgconfig/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AppStream-${AS_VERSION_LIBS}.gir DESTINATION share/gir-1.0/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AppStream-${AS_VERSION_LIBS}.typelib DESTINATION lib/girepository-1.0/)

# include the PackageKit plugin
add_subdirectory(pk-plugin)
