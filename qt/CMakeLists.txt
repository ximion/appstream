project(AppstreamQt)
cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

find_package(Xapian 1.2 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core)
include(GNUInstallDirs)

set(APPSTREAMQT_SO_VERSION 1)
set(APPSTREAMQT_VERSION ${AS_VERSION})

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

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
                    ${CMAKE_BINARY_DIR}/src
)

add_library(AppstreamQt SHARED ${APPSTREAMQT_SRC})
add_dependencies(AppstreamQt appstream)
target_link_libraries(AppstreamQt PUBLIC Qt5::Core PRIVATE ${XAPIAN_LIBRARIES} ${PROTOBUF_LITE_LIBRARIES})
target_include_directories(AppstreamQt PUBLIC "$<INSTALL_INTERFACE:${${CMAKE_INSTALL_INCLUDEDIR}}/>")
generate_export_header(AppstreamQt)

set_property(TARGET AppstreamQt PROPERTY VERSION "${APPSTREAMQT_VERSION}")
set_property(TARGET AppstreamQt PROPERTY SOVERSION "${APPSTREAMQT_SO_VERSION}" )

install(TARGETS AppstreamQt EXPORT AppstreamQtTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${APPSTREAMQT_PUBLIC_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/appstreamqt_export.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/AppstreamQt")

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtConfigVersion.cmake VERSION ${APPSTREAMQT_VERSION} COMPATIBILITY SameMajorVersion )

export(TARGETS AppstreamQt
       FILE "${CMAKE_CURRENT_BINARY_DIR}/AppstreamQtTargets.cmake")

set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/AppstreamQt)

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
