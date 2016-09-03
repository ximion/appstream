
find_package(Qt5 COMPONENTS Test REQUIRED)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/testpaths.h.in ${CMAKE_CURRENT_BINARY_DIR}/testpaths.h @ONLY)

add_executable(qt-cache-read-test qtcachereadtest.cpp)
target_link_libraries(qt-cache-read-test
    ${SANITIZER_LIBS}
    Qt5::Core
    Qt5::Test
    AppStreamQt
)
add_test(qt-cache-read-test qt-cache-read-test)

if(SANITIZERS)
	set_tests_properties(qt-cache-read-test PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
endif()
