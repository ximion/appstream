if(APPSTREAM_QT_VERSION STREQUAL "4")
    find_package(Qt4 4.8.0 COMPONENTS QtTest REQUIRED)

    include(${QT_USE_FILE})
else()
    find_package(Qt5 COMPONENTS Test REQUIRED)

#     when removing qt4 support, remove this too
    set(QT_QTTEST_LIBRARY Qt5::Test)
endif()

add_executable(screenshotxmlparsertest screenshotxmlparsertest.cpp)
target_link_libraries(screenshotxmlparsertest ${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} AppstreamQt)
add_test(screenshotxmlparsertest screenshotxmlparsertest)
