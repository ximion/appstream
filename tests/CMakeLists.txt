# CMakeLists for UAI Unit Tests

find_package(Vala REQUIRED)
include("${VALA_USE_FILE}")

find_package(GLIB2 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
find_package(LibXml2 REQUIRED)
find_library(M_LIB m)

link_libraries(${M_LIB}
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${LIBXML2_LIBRARIES}
		appstream
)

include_directories(${CMAKE_BINARY_DIR}
		${CMAKE_BINARY_DIR}/src
		${CMAKE_SOURCE_DIR}/src/xapian
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
		${LIBXML2_INCLUDE_DIR}
)

# Basic tests
vala_add_executable (uai-test-basics test-basics.vala
				BASE_DIR ${CMAKE_SOURCE_DIR}/src
				PACKAGES config gio-2.0 posix libxml-2.0 appstream_internal
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)
add_test(basic-test uai-test-basics ${CMAKE_CURRENT_SOURCE_DIR})

set(UAI_SOURCES_BASE ../uai/uai-utils.vala
		../uai/data-provider.vala
)

# AppStream-XML provider tests
vala_add_executable (uai-test-appstream ${UAI_SOURCES_BASE} ../uai/data-providers/appstream-xml.vala test-appstreamxml.vala
				BASE_DIR ${CMAKE_SOURCE_DIR}/src
				PACKAGES config gio-2.0 posix libxml-2.0 appstream_internal
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)
add_test(appstream-xml-test uai-test-appstream ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream database (read) tests
vala_add_executable (uai-test-db-read test-database-read.vala
				BASE_DIR ${CMAKE_SOURCE_DIR}/src
				PACKAGES config gio-2.0 posix libxml-2.0 appstream_internal
				VAPI_DIRS ${CMAKE_SOURCE_DIR}/vapi ${CMAKE_BINARY_DIR}/src
)
add_test(appstream-db-read-test uai-test-db-read ${CMAKE_CURRENT_SOURCE_DIR})
