# CMakeLists for UAI Unit Tests

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
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/xapian
		${CMAKE_CURRENT_SOURCE_DIR}
		${GLIB2_INCLUDE_DIR}
		${GIO2_INCLUDE_DIR}
		${LIBXML2_INCLUDE_DIR}
)

# Basic tests
add_executable(as-test-basics test-basics.c)
add_dependencies(as-test-basics appstream)
add_test(basic-test as-test-basics ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream-XML provider tests
add_executable(as-test-asxml test-asxml.c)
add_dependencies(as-test-asxml appstream)
add_test(asxml-test as-test-asxml ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream database tests
add_executable(as-test-database test-database.c)
add_dependencies(as-test-database appstream)
add_test(database-test as-test-database ${CMAKE_CURRENT_SOURCE_DIR})
