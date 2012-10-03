# CMakeLists for UAI data directory

set(servicedir "${CMAKE_INSTALL_PREFIX}/${INSTALL_DIR_LIBEXEC}")

find_program (LC_INTLTOOL_MERGE intltool-merge)
mark_as_advanced (LC_INTLTOOL_MERGE)

if (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")
	message (FATAL_ERROR "Intltool was not found! Please install it to continue!")
endif (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")

configure_file(org.freedesktop.AppStream.service.in
  "${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.AppStream.service"
  IMMEDIATE @ONLY
)

install(FILES org.freedesktop.AppStream.conf
	DESTINATION /etc/dbus-1/system.d/
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.AppStream.service
	DESTINATION share/dbus-1/system-services/
)

add_custom_target (merge-l10n ALL
	${LC_INTLTOOL_MERGE} -x ${CMAKE_SOURCE_DIR}/po
		${CMAKE_CURRENT_SOURCE_DIR}/org.freedesktop.appstream.policy.in
		${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.policy
	DEPENDS org.freedesktop.appstream.policy.in
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.appstream.policy
	DESTINATION share/polkit-1/actions/
)
