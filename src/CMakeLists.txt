# CMakeLists for AppStream database shared lib

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 2.28 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(Xapian 1.2 REQUIRED)

set (srcdir ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Source dir")

set(AS_VERSION_LIBS "${AS_VERSION_MAJOR}.${AS_VERSION_MINOR}")

set(APPSTREAM_LIB_SOURCES settings.vala
		utils.vala
		app-info.vala
		database.vala
)

set(ASLIB_XAP_SOURCES xapian/database.hpp
		xapian/database.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xapian
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${XAPIAN_INCLUDE_DIR}
)

vala_precompile(aslib-precomp ASLIB_SRC_C ${APPSTREAM_LIB_SOURCES}
			PACKAGES config gio-2.0 posix as-xapian
			VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi
				${CMAKE_CURRENT_SOURCE_DIR}/xapian
			GENERATE_INTERNAL_HEADER appstream_internal.h
			GENERATE_HEADER appstream.h
			COMPILE_FLAGS ${VALA_COMPILE_FLAGS}
				--gir=AppStream-${AS_VERSION_LIBS}.gir
				--library AppStream
				--internal-vapi=appstream_internal.vapi.orig
)

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

add_library(appstream SHARED ${ASLIB_SRC_C} ${UAI_XAP_SOURCES})
set_target_properties(appstream PROPERTIES VERSION ${AS_VERSION} SOVERSION ${APPSTREAM_LIB_API_LEVEL})

add_dependencies(appstream aslib-precomp)
add_dependencies (appstream header_internal_fix)

target_link_libraries(appstream
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${XAPIAN_LIBRARIES}
)

install(TARGETS appstream DESTINATION ${INSTALL_DIR_LIB})
