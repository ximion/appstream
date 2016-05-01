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

set (TEST_COMMON_SRC
	as-test-utils.h
	as-test-utils.c
)

# Basic tests
add_executable(as-test-basics test-basics.c ${TEST_COMMON_SRC})
add_dependencies(as-test-basics appstream)
add_test(test-basic as-test-basics ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream XML parser tests
add_executable(as-test-xmldata test-xmldata.c ${TEST_COMMON_SRC})
add_dependencies(as-test-xmldata appstream)
add_test(test-xml as-test-xmldata ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream DEP-11 parser tests
add_executable(as-test-yamldata test-yamldata.c ${TEST_COMMON_SRC})
add_dependencies(as-test-yamldata appstream)
add_test(test-yaml as-test-yamldata ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream database tests
add_executable(as-test-database test-database.c ${TEST_COMMON_SRC})
add_dependencies(as-test-database appstream)
add_test(test-database as-test-database ${CMAKE_CURRENT_SOURCE_DIR})
