project(AppStreamQt)
cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
add_compiler_export_flags()

find_package(Qt5 REQUIRED COMPONENTS Core)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.46)
include(GNUInstallDirs)

set(APPSTREAMQT_SO_VERSION 2)
set(APPSTREAMQT_VERSION ${AS_VERSION})

set(CMAKE_AUTOMOC on)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(APPSTREAMQT_SRC
    category.cpp
    component.cpp
    pool.cpp
    image.cpp
    screenshot.cpp
    icon.cpp
    provided.cpp
    release.cpp
    bundle.cpp
    suggested.cpp
    contentrating.cpp
    launchable.cpp
    translation.cpp
)

set(APPSTREAMQT_PUBLIC_HEADERS
    category.h
    component.h
    pool.h
    image.h
    screenshot.h
    icon.h
    provided.h
    release.h
    bundle.h
    suggested.h
    contentrating.h
    launchable.h
    translation.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_SOURCE_DIR}/src
                    ${CMAKE_BINARY_DIR}/src
                    ${GLIB2_INCLUDE_DIRS}
)

add_library(AppStreamQt SHARED ${APPSTREAMQT_SRC})
target_link_libraries(AppStreamQt PUBLIC Qt5::Core
                                  PRIVATE appstream
                                          ${GLIB2_LIBRARIES}
                                          ${SANITIZER_LIBS})
target_include_directories(AppStreamQt PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/>")
generate_export_header(AppStreamQt)

set_property(TARGET AppStreamQt PROPERTY VERSION "${APPSTREAMQT_VERSION}")
set_property(TARGET AppStreamQt PROPERTY SOVERSION "${APPSTREAMQT_SO_VERSION}" )

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AppStreamQtConfigVersion.cmake VERSION ${APPSTREAMQT_VERSION} COMPATIBILITY SameMajorVersion )

export(TARGETS AppStreamQt appstream
       FILE "${CMAKE_CURRENT_BINARY_DIR}/AppStreamQtTargets.cmake")

install(TARGETS AppStreamQt EXPORT AppStreamLibsTarget
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${APPSTREAMQT_PUBLIC_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/appstreamqt_export.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/AppStreamQt")

set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/AppStreamQt)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/AppStreamQtConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/AppStreamQtConfig.cmake"
  INSTALL_DESTINATION ${ConfigPackageLocation}
)

install(EXPORT AppStreamLibsTarget
    FILE AppStreamQtTargets.cmake
    DESTINATION ${ConfigPackageLocation})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AppStreamQtConfigVersion.cmake ${CMAKE_CURRENT_BINARY_DIR}/AppStreamQtConfig.cmake DESTINATION ${ConfigPackageLocation})

add_subdirectory(tests)
