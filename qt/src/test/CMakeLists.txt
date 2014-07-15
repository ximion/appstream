find_package(Qt4 4.8.0 COMPONENTS QtTest REQUIRED)
include(${QT_USE_FILE})

include_directories(../ )

add_executable(screenshotxmlparsertest screenshotxmlparsertest.cpp)
target_link_libraries(screenshotxmlparsertest ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} AppstreamQt)
add_test(screenshotxmlparsertest screenshotxmlparsertest)
