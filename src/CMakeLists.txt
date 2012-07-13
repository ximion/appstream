
find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

set(RUNAPP_SOURCES update-appstream-index.vala
		uai-server.vala
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
)

vala_add_executable(update-appstream-index ${RUNAPP_SOURCES}
			PACKAGES config gio-2.0 posix
			VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi
			GENERATE_INTERNAL_HEADER uai_internal.h
)

target_link_libraries(update-appstream-index
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
)

install(TARGETS update-appstream-index DESTINATION bin)
