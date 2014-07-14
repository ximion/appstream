# CMakeLists for UAI data directory

find_program (LC_INTLTOOL_MERGE intltool-merge)
mark_as_advanced (LC_INTLTOOL_MERGE)

if (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")
	message (FATAL_ERROR "Intltool was not found! Please install it to continue!")
endif (LC_INTLTOOL_MERGE STREQUAL "LC_INTLTOOL_MERGE-NOTFOUND")

install(FILES appstream.conf
	DESTINATION ${INSTALL_DIR_SYSCONF}
)

install(FILES categories.xml
	DESTINATION share/app-info/
)
