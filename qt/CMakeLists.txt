# CMakeLists for appstream-qt

find_package(Qt5Core REQUIRED)
find_package(GLIB2 2.36 REQUIRED)
pkg_check_modules(GIO2 REQUIRED gio-2.0)

set(QAPPSTREAM_LIB_API_LEVEL "0")

set(QAPPSTREAM_SRC
	utils.cpp
	component.cpp
	database.cpp
)

set(QAPPSTREAM_PUBLIC_HEADERS
	appstream-qt.h
	appstream-qt_global.h
	component.h
	database.h
)

set(QAPPSTREAM_PRIVATE_HEADERS
	utils-private.h
)

add_library(appstream-qt SHARED ${QAPPSTREAM_SRC} ${QAPPSTREAM_PUBLIC_HEADERS} ${QAPPSTREAM_PRIVATE_HEADERS})
add_dependencies(appstream-qt appstream)
set_target_properties(appstream-qt PROPERTIES VERSION ${AS_VERSION} SOVERSION ${QAPPSTREAM_LIB_API_LEVEL})

add_definitions(${Qt5Core_DEFINITIONS} "-DAPPSTREAMQT_LIBRARY")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_BINARY_DIR}
		    ${CMAKE_SOURCE_DIR}/src
		    ${GLIB2_INCLUDE_DIR}
		    ${GIO2_INCLUDE_DIR}
		    ${Qt5Core_INCLUDES}
)

target_link_libraries(appstream-qt
		appstream
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
		${Qt5Core_LIBRARIES}
)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/appstream-qt.pc.in ${CMAKE_CURRENT_BINARY_DIR}/appstream-qt.pc @ONLY)

install(TARGETS appstream-qt DESTINATION ${INSTALL_DIR_LIB})
install(FILES ${QAPPSTREAM_PUBLIC_HEADERS} DESTINATION include/Appstream-Qt)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appstream-qt.pc DESTINATION ${INSTALL_DIR_LIB}/pkgconfig/)
