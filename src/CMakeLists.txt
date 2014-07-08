SET(ASMARA_SRC
	component.cpp
	database.cpp
	image.cpp
	screenshot.cpp
)

SET(ASMARA_PUBLIC_HEADERS
    component.h
    database.h
    image.h
    screenshot.h
)

add_library(Asmara SHARED ${ASMARA_SRC})
target_link_libraries(Asmara PUBLIC ${QT_QTCORE_LIBRARY} PRIVATE ${XAPIAN_LIBRARIES})
generate_export_header(Asmara)

set_property(TARGET Asmara PROPERTY VERSION "${ASMARA_SO_VERSION}.0.0")
set_property(TARGET Asmara PROPERTY SOVERSION "${ASMARA_SO_VERSION}" )

install(TARGETS Asmara EXPORT AsmaraTargets
    RUNTIME DESTINATION "bin"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    INCLUDES DESTINATION "include"
)

install(FILES ${ASMARA_PUBLIC_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/asmara_export.h DESTINATION "include/Asmara")

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AsmaraConfigVersion.cmake VERSION ${ASMARA_VERSION} COMPATIBILITY SameMajorVersion )

export(TARGETS Asmara
       FILE "${CMAKE_CURRENT_BINARY_DIR}/AsmaraTargets.cmake")

set(ConfigPackageLocation lib/cmake/Asmara)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/AsmaraConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/AsmaraConfig.cmake"
  @ONLY
)

install(EXPORT AsmaraTargets
    FILE AsmaraTargets.cmake
    DESTINATION ${ConfigPackageLocation})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AsmaraConfigVersion.cmake ${CMAKE_CURRENT_BINARY_DIR}/AsmaraConfig.cmake DESTINATION ${ConfigPackageLocation})

add_subdirectory(test)

