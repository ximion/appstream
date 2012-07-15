
find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 2.28 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(Xapian 1.2 REQUIRED)

set(UAI_SOURCES update-appstream-index.vala
		settings.vala
		uai-server.vala
)

set(UAI_XAP_SOURCES xapian/database.hpp
		xapian/database.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${XAPIAN_INCLUDE_DIR}
)

vala_precompile(uai-precomp UAI_SRC_C ${UAI_SOURCES}
			PACKAGES config gio-2.0 posix
			VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi
				${CMAKE_CURRENT_SOURCE_DIR}/xapian
			GENERATE_INTERNAL_HEADER uai_internal.h
)

add_executable(update-appstream-index ${UAI_SRC_C} ${UAI_XAP_SOURCES})
add_dependencies(update-appstream-index uai-precomp)

target_link_libraries(update-appstream-index
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${XAPIAN_LIBRARIES}
)

install(TARGETS update-appstream-index DESTINATION bin)
