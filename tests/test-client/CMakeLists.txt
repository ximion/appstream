# CMakeLists for UAI Example Client

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_library(M_LIB m)

link_libraries(${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
)

# Basic tests
vala_add_executable (uai-test-client test-client.vala
				PACKAGES config gio-2.0 posix
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi
)
