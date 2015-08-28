# CMakeLists for appstream-tool
project(appstream-tool)

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_library(M_LIB m)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_SOURCE_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
)

set(ASTOOL_SOURCES
	appstream-tool.c
	astool-utils.h
	astool-utils.c
	astool-cache-actions.h
	astool-cache-actions.c
	astool-validate-actions.h
	astool-validate-actions.c
)

# Add the appstream client tool
add_executable(appstream_tool ${ASTOOL_SOURCES})
set_target_properties(appstream_tool
	PROPERTIES OUTPUT_NAME appstream)
add_dependencies(appstream_tool appstream)

# the client tool uses API which is currently private...
# needs to be sorted out in future
add_definitions ("-DAS_COMPILATION")

target_link_libraries(appstream_tool
		${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		appstream
)

install(TARGETS appstream_tool DESTINATION ${CMAKE_INSTALL_BINDIR})
