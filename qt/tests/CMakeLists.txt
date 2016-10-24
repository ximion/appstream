# CMakeLists for AppStreamQt Tests

find_package(Qt5 COMPONENTS Test REQUIRED)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/testpaths.h.in ${CMAKE_CURRENT_BINARY_DIR}/testpaths.h @ONLY)

add_executable(asqt-pool-test asqt-pool-test.cpp)
target_link_libraries(asqt-pool-test
    ${SANITIZER_LIBS}
    Qt5::Core
    Qt5::Test
    AppStreamQt
)
add_test(asqt-pool-test asqt-pool-test)

if(SANITIZERS)
	set_tests_properties(asqt-pool-test PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
endif()
