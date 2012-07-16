# CMakeLists for UAI Unit Tests

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(Xapian 1.2 REQUIRED)
find_package(LibXml2 REQUIRED)
find_library(M_LIB m)

link_libraries(${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${LIBXML2_LIBRARIES}
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
		${LIBXML2_INCLUDE_DIR}
)

# Basic tests
vala_add_executable (uai-test-basics ${UAI_SOURCES_BASE} test-basics.vala
				BASE_DIR ${CMAKE_SOURCE_DIR}/src
				PACKAGES config gio-2.0 posix libxml-2.0
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)
add_test(basic-test uai-test-basics ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream-XML provider tests
vala_add_executable (uai-test-appstream ${UAI_SOURCES_BASE} test-appstreamxml.vala
				BASE_DIR ${CMAKE_SOURCE_DIR}/src
				PACKAGES config gio-2.0 posix libxml-2.0
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)
add_test(appstream-xml-test uai-test-appstream ${CMAKE_CURRENT_SOURCE_DIR})
