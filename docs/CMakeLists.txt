# CMakeLists for documentation

find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (XMLTO STREQUAL "")
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (XMLTO STREQUAL "")

include (${CMAKE_SOURCE_DIR}/data/cmake/documentation.cmake)

add_xml_manpages(doc-man "appstream-index.1" "" "")

if (DOCUMENTATION)
	# find documentation tools
	find_program (VALADOC valadoc)
	mark_as_advanced (VALADOC)
	if (VALADOC STREQUAL "")
		message (FATAL_ERROR "valadoc was not found! Please install Valadoc to continue!")
	endif (VALADOC STREQUAL "")
	find_program(GTKDOC_SCAN_EXE gtkdoc-scan PATH "${GLIB_PREFIX}/bin")
	if(NOT GTKDOC_SCAN_EXE)
		message(FATAL_ERROR "gtkdoc not found")
	endif(NOT GTKDOC_SCAN_EXE)

	find_path (SYSTEM_VAPI_DIR
		NAMES glib-2.0.vapi
		HINTS "/usr/share/vala-0.??/vapi" "/usr/share/vala")
	# ugly hack to help valadoc finding vapi files
	string (SUBSTRING ${VALA_VERSION} 0 4 VALA_VERSION_SHORT)
	set (SYSTEM_VAPI_DIR "${CMAKE_INSTALL_PREFIX}/share/vala-${VALA_VERSION_SHORT}/vapi")

	# find public library sources
	file (GLOB APPSTREAM_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/*.vala")

	add_custom_target (documentation
			COMMAND "valadoc" "--force" "-b" "${CMAKE_SOURCE_DIR}/src" "-o" "${CMAKE_SOURCE_DIR}/docs/api"
			"--vapidir=${CMAKE_SOURCE_DIR}/vapi" "--vapidir=${SYSTEM_VAPI_DIR}"
			"--vapidir=${CMAKE_SOURCE_DIR}/src/xapian"
			"-X" "${CMAKE_BINARY_DIR}/src/appstream.h" "-X" "-l"
			"-X" "${CMAKE_BINARY_DIR}/src/libappstream.so.${AS_VERSION_LIBS}"
			"--doclet" "gtkdoc"
			"--pkg=config" "--pkg=gio-2.0" "--pkg=as-xapian"
			"--package-name=appstream"
			"--package-version=${AS_VERSION_LIBS}"
			"--importdir=${CMAKE_SOURCE_DIR}/docs"
			${APPSTREAM_LIB_SOURCES}
			VERBATIM)
	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/html)
	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/xml)
endif()
