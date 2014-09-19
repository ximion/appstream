# CMakeLists for appstream-index
project(appstream-index)

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_library(M_LIB m)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${CMAKE_BINARY_DIR}
		    ${CMAKE_SOURCE_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
)

# Add the appstream-index client tool
add_executable(appstream-index appstream-index.c)
add_dependencies(appstream-index appstream)

# Add the appstream-validate client tool
add_executable(appstream-validate appstream-validate.c)
add_dependencies(appstream-validate appstream)

# the client tool uses API which is currently private...
# needs to be sorted out in future
add_definitions ("-DAS_COMPILATION")

target_link_libraries(appstream-index
		${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		appstream
)

target_link_libraries(appstream-validate
		${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		appstream
)

install(TARGETS appstream-index DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS appstream-validate DESTINATION ${CMAKE_INSTALL_BINDIR})
