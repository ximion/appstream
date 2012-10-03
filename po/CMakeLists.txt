# CMakeLists for AppStream-Core localisation

find_program (LC_INTLTOOL_UPDATE intltool-update)
mark_as_advanced (LC_INTLTOOL_UPDATE)

find_package (Gettext REQUIRED)

if (LC_INTLTOOL_UPDATE STREQUAL "")
	message (FATAL_ERROR "Intltool was not found! Please install it to continue!")
endif (LC_INTLTOOL_UPDATE STREQUAL "")

set(LPKNAME "appstream")

add_custom_target (l10n-update
	${LC_INTLTOOL_UPDATE} -p -g "${LPKNAME}"
	DEPENDS POTFILES.in
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

file (GLOB _translations ${CMAKE_CURRENT_SOURCE_DIR}/*.po)
        gettext_create_translations (${LPKNAME}.pot
				     ALL ${_translations}
)
