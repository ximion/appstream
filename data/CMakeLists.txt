# CMakeLists for UAI data directory

set(servicedir "${CMAKE_INSTALL_PREFIX}/${INSTALL_DIR_LIBEXEC}")

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
