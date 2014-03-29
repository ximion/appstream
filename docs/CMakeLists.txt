# CMakeLists for documentation

find_program (XMLTO xmlto)
mark_as_advanced (XMLTO)
if (XMLTO STREQUAL "")
	message (FATAL_ERROR "Xmlto was not found! Please install it to continue!")
endif (XMLTO STREQUAL "")

include (${CMAKE_SOURCE_DIR}/data/cmake/documentation.cmake)

add_xml_manpages(doc-man "sources/man/appstream-index.1" "" "")

if (DOCUMENTATION)
	#########################
	# General project documentation
	find_program (PUBLICAN publican)
	mark_as_advanced (PUBLICAN)
	if (PUBLICAN STREQUAL "")
		message (FATAL_ERROR "Publican was not found! Please install the it to continue!")
	endif (PUBLICAN STREQUAL "")

	# we need to assemble our documentation build-directory before we can built it
	add_custom_target (assemble-docs
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/prepare-docbuild.sh" "source_dir=${CMAKE_CURRENT_SOURCE_DIR}" "build_dir=${CMAKE_BINARY_DIR}/docbuild"
		DEPENDS "publican.cfg"
			"sources/"
			"${CMAKE_BINARY_DIR}/docbuild/"
			"${CMAKE_BINARY_DIR}/src/libappstream.so"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	# the actual target to build all documentation
	add_custom_target (documentation
		COMMAND ${PUBLICAN} "build" "--langs=en-US" "--publish" "--formats=html"
		DEPENDS "publican.cfg"
			"${CMAKE_BINARY_DIR}/docbuild/publican.cfg"
			assemble-docs
			api-docs
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/docbuild/
	)
	add_custom_command (OUTPUT "${CMAKE_BINARY_DIR}/docbuild/"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/docbuild"
			VERBATIM
	)

	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/html)
	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_BINARY_DIR}/docbuild)

	# a target to export documentation html to the source tree (useful when building the tarballs)
	add_custom_target (documentation-htmlexport
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/docbuild/AppStream-Docs/en-US/html/" "${CMAKE_CURRENT_SOURCE_DIR}/html"
		DEPENDS documentation
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	add_custom_command(
		TARGET documentation-htmlexport POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/api/html" "${CMAKE_CURRENT_SOURCE_DIR}/html/api/html"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)


	# FIXME: Adjust for the new C code
	#########################
	# API documentation

 	# find documentation tools
# 	find_program (VALADOC valadoc)
# 	mark_as_advanced (VALADOC)
# 	if (VALADOC STREQUAL "")
# 		message (FATAL_ERROR "valadoc was not found! Please install Valadoc to continue!")
# 	endif (VALADOC STREQUAL "")
# 	find_program(GTKDOC_SCAN_EXE gtkdoc-scan PATH "${GLIB_PREFIX}/bin")
# 	if(NOT GTKDOC_SCAN_EXE)
# 		message(FATAL_ERROR "gtkdoc not found")
# 	endif(NOT GTKDOC_SCAN_EXE)
#
# 	find_path (SYSTEM_VAPI_DIR
# 		NAMES glib-2.0.vapi
# 		HINTS "/usr/share/vala-0.??/vapi" "/usr/share/vala")
# 	# ugly hack to help valadoc finding vapi files
# 	string (SUBSTRING ${VALA_VERSION} 0 4 VALA_VERSION_SHORT)
# 	set (SYSTEM_VAPI_DIR "${CMAKE_INSTALL_PREFIX}/share/vala-${VALA_VERSION_SHORT}/vapi")
#
# 	# find public library sources
# 	file (GLOB APPSTREAM_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/*.vala")
# 	file (GLOB tmp "${CMAKE_SOURCE_DIR}/src/data-providers/*.vala")
# 	set(APPSTREAM_LIB_SOURCES ${APPSTREAM_LIB_SOURCES} ${tmp})
#
# 	add_custom_target (api-docs
# 			COMMAND "valadoc" "--force" "-b" "${CMAKE_SOURCE_DIR}/src" "-o" "${CMAKE_SOURCE_DIR}/docs/api"
# 			"--vapidir=${CMAKE_SOURCE_DIR}/vapi" "--vapidir=${SYSTEM_VAPI_DIR}"
# 			"--vapidir=${CMAKE_SOURCE_DIR}/src/xapian"
# 			"-X" "${CMAKE_BINARY_DIR}/src/appstream.h" "-X" "-l"
# 			"-X" "${CMAKE_BINARY_DIR}/src/libappstream.so.${AS_VERSION_LIBS}"
# 			"--doclet" "gtkdoc"
# 			"--pkg=config" "--pkg=gio-2.0" "--pkg=libxml-2.0" "--pkg=posix" "--pkg=as-xapian"
# 			"--package-name=appstream"
# 			"--package-version=${AS_VERSION_LIBS}"
# 			"--importdir=${CMAKE_SOURCE_DIR}/docs"
# 			${APPSTREAM_LIB_SOURCES}
# 			VERBATIM)
# 	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/html)
# 	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/xml)
endif()
