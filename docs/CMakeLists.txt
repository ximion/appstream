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
		message (FATAL_ERROR "Publican was not found. Please install it to continue!")
	endif (PUBLICAN STREQUAL "")

	# we need to assemble our documentation build-directory before we can built it
	add_custom_target (assemble-docs
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/prepare-docbuild.sh" "source_dir=${CMAKE_CURRENT_SOURCE_DIR}" "build_dir=${CMAKE_BINARY_DIR}/docbuild"
		DEPENDS "publican.cfg"
			"sources/"
			"${CMAKE_BINARY_DIR}/docbuild/"
			appstream
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	# the actual target to build all documentation
	add_custom_target (documentation
		COMMAND ${PUBLICAN} "build" "--langs=en-US" "--publish" "--formats=html"
		DEPENDS "publican.cfg"
			"${CMAKE_BINARY_DIR}/docbuild/publican.cfg"
			assemble-docs
			doc-appstream
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

	#########################
	# API documentation

	find_package(GtkDoc)
	if(NOT GTKDOC_FOUND)
		message(FATAL_ERROR "gtkdoc was not found, API documentation can not be built.")
	endif()
	include(UseGtkDoc)

	gtk_doc_add_module(appstream ${CMAKE_SOURCE_DIR}/src
		IGNOREHEADERS as-component-private.h as-utils.h as-database-write.h as-settings-private.h
		SUFFIXES h c hh cc
		LDFLAGS
			-L${CMAKE_CURRENT_BINARY_DIR}/../src
			-lappstream
		LDPATH
			"${CMAKE_CURRENT_BINARY_DIR}/../src"
		OUTPUT_DIR
			"${CMAKE_CURRENT_SOURCE_DIR}/api"
		DEPENDS appstream)

	set_directory_properties (PROPERTIES CLEAN_NO_CUSTOM ${CMAKE_CURRENT_SOURCE_DIR}/api)
 	set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/api/html;${CMAKE_CURRENT_SOURCE_DIR}/api/xml;${CMAKE_CURRENT_SOURCE_DIR}/api/tmpl")
endif()
