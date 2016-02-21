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

# AppStream XML parser tests
add_executable(as-test-xmldata test-xmldata.c)
add_dependencies(as-test-xmldata appstream)
add_test(asxml-test as-test-xmldata ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream DEP-11 parser tests
add_executable(as-test-yamldata test-yamldata.c)
add_dependencies(as-test-yamldata appstream)
add_test(dep11-test as-test-yamldata ${CMAKE_CURRENT_SOURCE_DIR})

# AppStream database tests
add_executable(as-test-database test-database.c)
add_dependencies(as-test-database appstream)
add_test(database-test as-test-database ${CMAKE_CURRENT_SOURCE_DIR})
