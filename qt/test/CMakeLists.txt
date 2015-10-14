
find_package(Qt5 COMPONENTS Test REQUIRED)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/testpaths.h.in ${CMAKE_CURRENT_BINARY_DIR}/testpaths.h @ONLY)

add_executable(qtdbreadtest qtdbreadtest.cpp)
target_link_libraries(qtdbreadtest
    Qt5::Core
    Qt5::Test
    AppstreamQt
)
add_test(qtdbreadtest qtdbreadtest)
