# CMakeLists for update-appstream-index
project(update-appstream-index)

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(LibXml2 REQUIRED)

set(UAI_SOURCES main.vala
		uai-engine.vala
		data-provider.vala
		data-providers/appstream-xml.vala
		data-providers/debian-dep11.vala
		data-providers/ubuntu-appinstall.vala
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_BINARY_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${LIBXML2_INCLUDE_DIR}
)

vala_add_executable(update-appstream-index ${UAI_SOURCES}
			PACKAGES config glib-2.0 gio-2.0 libxml-2.0 appstream_internal
			VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)

add_dependencies(update-appstream-index_precompile appstream)

target_link_libraries(update-appstream-index
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${LIBXML2_LIBRARIES}
		appstream
)

install(TARGETS update-appstream-index DESTINATION ${INSTALL_DIR_LIBEXEC})
