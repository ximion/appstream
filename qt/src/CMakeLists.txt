
set(APPSTREAMQT_SRC
          component.cpp
          database.cpp
          image.cpp
          provides.cpp
          screenshot.cpp
)

set(APPSTREAMQT_PUBLIC_HEADERS
    component.h
    database.h
    image.h
    provides.h
    screenshot.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
		    ${CMAKE_SOURCE_DIR}/src/xapian
)

if(NOT QT)
    # hack for standalone compilation of the Qt lib (the QT variable is not defined if we build standalone)
    include_directories(${CMAKE_SOURCE_DIR}/../src/xapian)
endif()

add_library(AppstreamQt SHARED ${APPSTREAMQT_SRC})
target_link_libraries(AppstreamQt PUBLIC ${QT_QTCORE_LIBRARY} PRIVATE ${XAPIAN_LIBRARIES})
generate_export_header(AppstreamQt)

set_property(TARGET AppstreamQt PROPERTY VERSION "${APPSTREAMQT_SO_VERSION}.0.0")
set_property(TARGET AppstreamQt PROPERTY SOVERSION "${APPSTREAMQT_SO_VERSION}" )

install(TARGETS AppstreamQt EXPORT AppstreamQtTargets
    RUNTIME DESTINATION ${INSTALL_DIR_BIN}
    LIBRARY DESTINATION ${INSTALL_DIR_LIB}
    ARCHIVE DESTINATION ${INSTALL_DIR_LIB}
    INCLUDES DESTINATION ${INSTALL_DIR_INC}
)

install(FILES ${APPSTREAMQT_PUBLIC_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/appstreamqt_export.h DESTINATION "${INSTALL_DIR_INC}/AppstreamQt")

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtConfigVersion.cmake VERSION ${APPSTREAMQT_VERSION} COMPATIBILITY SameMajorVersion )

export(TARGETS AppstreamQt
       FILE "${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtTargets.cmake")

set(ConfigPackageLocation ${INSTALL_DIR_LIB}/cmake/AppstreamQt)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/AppstreamQtConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtConfig.cmake"
  @ONLY
)

install(EXPORT AppstreamQtTargets
    FILE AppstreamQtTargets.cmake
    DESTINATION ${ConfigPackageLocation})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtConfigVersion.cmake ${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtConfig.cmake DESTINATION ${ConfigPackageLocation})

add_subdirectory(test)

