# CMakeLists for Qt AppStream example

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Quick REQUIRED)

set(QASEXAMPLE_SRC
	main.cpp
	asintf.h
	asintf.cpp
)

qt5_add_resources (QASExampleResourceSources resources.qrc)

add_executable(qas-example ${QASEXAMPLE_SRC} ${QASExampleResourceSources})
add_dependencies(qas-example appstream-qt)

add_definitions(${Qt5Core_DEFINITIONS} ${Qt5Quick_DEFINITIONS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
			${CMAKE_BINARY_DIR}
		    ${CMAKE_SOURCE_DIR}/qt
		    ${Qt5Core_INCLUDES}
		    ${Qt5Quick_INCLUDES}
		    ${Qt5Widgets_INCLUDES}
)

link_directories (${CMAKE_BINARY_DIR}/src)

target_link_libraries(qas-example
		appstream-qt
		${Qt5Core_LIBRARIES}
		${Qt5Quick_LIBRARIES}
		${Qt5Widgets_LIBRARIES}
)
