# CMakeLists for documentation

find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (XMLTO STREQUAL "")
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (XMLTO STREQUAL "")

include (${CMAKE_SOURCE_DIR}/data/cmake/documentation.cmake)

add_xml_manpages(doc-man "appstream-index.1" "" "")
