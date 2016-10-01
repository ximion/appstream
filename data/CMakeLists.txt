# CMakeLists for AppStream data directory

find_program (MSGFMT msgfmt)
mark_as_advanced (MSGFMT)

if (NOT MSGFMT)
	message (FATAL_ERROR "The msgfmt tool was not found. Is Gettext properly installed?")
endif ()

install(FILES appstream.conf
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
)

add_custom_target (merge-l10n ALL
	${MSGFMT} --xml
		--template ${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.appstream.cli.metainfo.xml
		-d ${CMAKE_SOURCE_DIR}/po
		-o ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.cli.metainfo.xml
	DEPENDS org.freedesktop.appstream.cli.metainfo.xml
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.cli.metainfo.xml
	DESTINATION share/metainfo
)
