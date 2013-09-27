# CMakeLists for AppStream PackageKit PlugIn

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
pkg_check_modules(PACKAGEKIT REQUIRED packagekit-glib2)
pkg_check_modules(PACKAGEKIT_PLUGIN REQUIRED packagekit-plugin)

set(PK_APPSTREAM_SOURCES pk-plugin-appstream.c
)

add_library(pk_plugin_appstream MODULE ${PK_APPSTREAM_SOURCES})

target_link_libraries(pk_plugin_appstream
		${M_LIB}
		${GIO2_LIBRARIES}
		${PACKAGEKIT_LIBRARIES}
		${PACKAGEKIT_PLUGIN_LIBRARIES}
		appstream
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
		${PACKAGEKIT_INCLUDE_DIRS}
		${PACKAGEKIT_PLUGIN_INCLUDE_DIRS}
)

install(TARGETS pk_plugin_appstream DESTINATION ${INSTALL_DIR_LIB}/packagekit-plugins)
