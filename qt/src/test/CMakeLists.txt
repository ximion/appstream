
find_package(Qt5 COMPONENTS Test REQUIRED)

add_executable(screenshotxmlparsertest screenshotxmlparsertest.cpp)
target_link_libraries(screenshotxmlparsertest
    Qt5::Core
    Qt5::Test
    AppstreamQt
)
add_test(screenshotxmlparsertest screenshotxmlparsertest)
