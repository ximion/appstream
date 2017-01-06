# CMakeLists for AppStream data directory

find_program (ITSTOOL itstool)
mark_as_advanced (ITSTOOL)

if (NOT ITSTOOL)
	message (FATAL_ERROR "Itstool was not found. Please install it to continue.")
endif ()

install(FILES appstream.conf
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
)

add_custom_target (merge-l10n ALL
	${ITSTOOL}
		-i ${CMAKE_SOURCE_DIR}/contrib/its/metainfo.its
		-j ${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.appstream.cli.metainfo.xml
		-o ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.cli.metainfo.xml
		${CMAKE_BINARY_DIR}/po/*.gmo
	DEPENDS org.freedesktop.appstream.cli.metainfo.xml translations_1
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.cli.metainfo.xml
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)
