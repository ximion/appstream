# Vala API

set(APPSTREAM_VAPI_FILE "${CMAKE_CURRENT_BINARY_DIR}/appstream.vapi")

find_program (VAPIGEN vapigen)
mark_as_advanced (VAPIGEN)
if (VAPIGEN STREQUAL "VAPIGEN-NOTFOUND")
	message (FATAL_ERROR "Vapigen was not found. Please install it in order to generate Vala API files!")
endif (VAPIGEN STREQUAL "VAPIGEN-NOTFOUND")

add_custom_command (OUTPUT ${APPSTREAM_VAPI_FILE}
			COMMAND ${VAPIGEN} --metadatadir=${CMAKE_CURRENT_SOURCE_DIR} -d . --vapidir=${CMAKE_CURRENT_SOURCE_DIR}
					${CMAKE_BINARY_DIR}/src/AppStream-${AS_VERSION_BASE}.gir
					--library=appstream
					--pkg glib-2.0 --pkg gio-2.0 --pkg gobject-2.0
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS appstream
					${CMAKE_BINARY_DIR}/src/AppStream-${AS_VERSION_BASE}.typelib
			VERBATIM
)
add_custom_target(vapi ALL DEPENDS ${APPSTREAM_VAPI_FILE})

install(FILES ${APPSTREAM_VAPI_FILE} DESTINATION share/vala/vapi)
