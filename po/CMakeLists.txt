# CMakeLists for AppStream localisation

find_program (XGETTEXT xgettext)
mark_as_advanced (XGETTEXT)

find_program (ITSTOOL itstool)
mark_as_advanced (ITSTOOL)

find_package (Gettext REQUIRED)

if (NOT XGETTEXT)
	message (FATAL_ERROR "Xgettext was not found. Please install it to continue.")
endif()
if (NOT ITSTOOL)
	message (FATAL_ERROR "Itstool was not found. Please install it to continue.")
endif()

set(XGETTEXT_OPTIONS --from-code=UTF-8
		--keyword=_ --keyword=N_ --keyword=C_:1c,2 --keyword=NC_:1c,2
		-i -s --no-wrap -c
		--package-name=appstream
		"--copyright-holder=Matthias Klumpp"
		"--msgid-bugs-address=appstream@lists.freedesktop.org"
)

set(XML_FILES
	../data/org.freedesktop.appstream.cli.metainfo.xml
)

set(LPKNAME appstream)

set(XML_TMP_POT "${CMAKE_CURRENT_BINARY_DIR}/${LPKNAME}-xmlfiles.pot")
add_custom_command (OUTPUT ${XML_TMP_POT}
	COMMAND ${ITSTOOL} --output=${XML_TMP_POT}
			--its=${CMAKE_SOURCE_DIR}/contrib/its/metainfo.its
			${XML_FILES}
	DEPENDS ${XML_FILES}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target (l10n-update
	${XGETTEXT}
		${XGETTEXT_OPTIONS}
		--files-from=SRCFILES
		--directory=${CMAKE_SOURCE_DIR}
		${XML_TMP_POT}
		--output=${LPKNAME}.pot
	DEPENDS SRCFILES ${XML_TMP_POT}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

file (GLOB _translations ${CMAKE_CURRENT_SOURCE_DIR}/*.po)
        gettext_create_translations (${LPKNAME}.pot
				     ALL ${_translations}
)
