# CMake macros to use the GtkDoc documentation system

include(StandardOptionParsing)
find_package(GtkDoc)

# gtk_doc_add_module(doc_prefix sourcedir
# [XML xmlfile]
# [FIXXREFOPTS fixxrefoption1...]
# [IGNOREHEADERS header1...]
# [DEPENDS depend1...] )
#
# sourcedir must be the *full* path to the source directory.
#
# If omitted, sgmlfile defaults to the auto generated ${doc_prefix}/${doc_prefix}-docs.xml.
macro(gtk_doc_add_module _doc_prefix _doc_sourcedir)
    set(_list_names "DEPENDS" "XML" "FIXXREFOPTS" "IGNOREHEADERS"
        "CFLAGS" "LDFLAGS" "LDPATH" "SUFFIXES" "OUTPUT_DIR")
    set(_list_variables "_depends" "_xml_file" "_fixxref_opts" "_ignore_headers"
        "_extra_cflags" "_extra_ldflags" "_extra_ldpath" "_suffixes" "_output_dir")
    parse_options(_list_names _list_variables ${ARGN})

    list(LENGTH _xml_file _xml_file_length)


    if(_suffixes)
        set(_doc_source_suffixes "")
        foreach(_suffix ${_suffixes})
            if(_doc_source_suffixes)
                set(_doc_source_suffixes "${_doc_source_suffixes},${_suffix}")
            else(_doc_source_suffixes)
                set(_doc_source_suffixes "${_suffix}")
            endif(_doc_source_suffixes)
        endforeach(_suffix)
    else(_suffixes)
        set(_doc_source_suffixes "h")
    endif(_suffixes)

    # set(_do_all ALL)

    set(_opts_valid 1)
    if(NOT _xml_file_length LESS 2)
        message(SEND_ERROR "Must have at most one sgml file specified.")
        set(_opts_valid 0)
    endif(NOT _xml_file_length LESS 2)

    if(_opts_valid)
        # a directory to store output.
        if(NOT _output_dir)
            set(_output_dir "${CMAKE_CURRENT_BINARY_DIR}/${_doc_prefix}")
        endif()
        set(_output_dir_stamp "${_output_dir}/dir.stamp")

        # set default sgml file if not specified
        set(_default_xml_file "${_output_dir}/${_doc_prefix}-docs.xml")
        get_filename_component(_default_xml_file ${_default_xml_file} ABSOLUTE)

        # a directory to store html output.
        set(_output_html_dir "${_output_dir}/html")
        set(_output_html_dir_stamp "${_output_dir}/html_dir.stamp")

        # The output files
        set(_output_decl_list "${_output_dir}/${_doc_prefix}-decl-list.txt")
        set(_output_decl "${_output_dir}/${_doc_prefix}-decl.txt")
        set(_output_overrides "${_output_dir}/${_doc_prefix}-overrides.txt")
        set(_output_sections "${_output_dir}/${_doc_prefix}-sections.txt")
        set(_output_types "${_output_dir}/${_doc_prefix}.types")

        set(_output_signals "${_output_dir}/${_doc_prefix}.signals")

        set(_output_unused "${_output_dir}/${_doc_prefix}-unused.txt")
        set(_output_undeclared "${_output_dir}/${_doc_prefix}-undeclared.txt")
        set(_output_undocumented "${_output_dir}/${_doc_prefix}-undocumented.txt")
        set(_output_tmpl_dir "${_output_dir}/tmpl")
        set(_output_tmpl_stamp "${_output_dir}/tmpl.stamp")

        set(_output_xml_dir "${_output_dir}/xml")
        set(_output_sgml_stamp "${_output_dir}/sgml.stamp")

        set(_output_html_stamp "${_output_dir}/html.stamp")

        # add a command to create output directory
        add_custom_command(
            OUTPUT "${_output_dir_stamp}" "${_output_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_output_dir}"
            COMMAND ${CMAKE_COMMAND} -E touch ${_output_dir_stamp}
            VERBATIM)

        set(_ignore_headers_opt "")
        if(_ignore_headers)
            set(_ignore_headers_opt "--ignore-headers=")
            foreach(_header ${_ignore_headers})
                set(_ignore_headers_opt "${_ignore_headers_opt}${_header} ")
            endforeach(_header ${_ignore_headers})
        endif(_ignore_headers)

        # add a command to scan the input
        add_custom_command(
            OUTPUT
                "${_output_decl_list}"
                "${_output_decl}"
                "${_output_decl}.bak"
                "${_output_overrides}"
                "${_output_sections}"
                "${_output_types}"
                "${_output_types}.bak"
            DEPENDS
                "${_output_dir}"
                ${_depends}
            COMMAND ${GTKDOC_SCAN_EXE}
                "--module=${_doc_prefix}"
                "${_ignore_headers_opt}"
                "--rebuild-sections"
                "--rebuild-types"
                "--source-dir=${_doc_sourcedir}"
            WORKING_DIRECTORY "${_output_dir}"
            VERBATIM)

        # add a command to scan the input via gtkdoc-scangobj
        # This is such a disgusting hack!
        add_custom_command(
            OUTPUT
                "${_output_signals}"
            DEPENDS
                "${_output_types}"
            COMMAND ${CMAKE_COMMAND}
                -D "GTKDOC_SCANGOBJ_EXE:STRING=${GTKDOC_SCANGOBJ_EXE}"
                -D "doc_prefix:STRING=${_doc_prefix}"
                -D "output_types:STRING=${_output_types}"
                -D "output_dir:STRING=${_output_dir}"
                -D "EXTRA_CFLAGS:STRING=${_extra_cflags}"
                -D "EXTRA_LDFLAGS:STRING=${_extra_ldflags}"
                -D "EXTRA_LDPATH:STRING=${_extra_ldpath}"
                -P ${GTKDOC_SCANGOBJ_WRAPPER}
            WORKING_DIRECTORY "${_output_dir}"
            VERBATIM)

        # add a command to make the templates
        add_custom_command(
            OUTPUT
                "${_output_unused}"
                "${_output_undeclared}"
                "${_output_undocumented}"
                "${_output_tmpl_dir}"
                "${_output_tmpl_stamp}"
            DEPENDS
                "${_output_types}"
                "${_output_signals}"
                "${_output_sections}"
                "${_output_overrides}"
                ${_depends}
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${_output_tmpl_dir}
            COMMAND ${GTKDOC_MKTMPL_EXE}
                "--module=${_doc_prefix}"
            WORKING_DIRECTORY "${_output_dir}"
            VERBATIM)

        set(_copy_xml_if_needed "")
        if(_xml_file)
            get_filename_component(_xml_file ${_xml_file} ABSOLUTE)
            set(_copy_xml_if_needed
                COMMAND ${CMAKE_COMMAND} -E copy "${_xml_file}" "${_default_xml_file}")
        endif(_xml_file)

        set(_remove_xml_if_needed "")
        if(_xml_file)
            set(_remove_xml_if_needed
                COMMAND ${CMAKE_COMMAND} -E remove ${_default_xml_file})
        endif(_xml_file)

        # add a command to make the database
        add_custom_command(
            OUTPUT
                "${_output_sgml_stamp}"
                "${_default_xml_file}"
            DEPENDS
                "${_output_tmpl_stamp}"
                ${_depends}
            ${_remove_xml_if_needed}
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${_output_xml_dir}
            COMMAND ${GTKDOC_MKDB_EXE}
                "--module=${_doc_prefix}"
                "--source-dir=${_doc_sourcedir}"
                "--source-suffixes=${_doc_source_suffixes}"
                "--output-format=xml"
                "--main-sgml-file=${_default_xml_file}"
            ${_copy_xml_if_needed}
            WORKING_DIRECTORY "${_output_dir}"
            VERBATIM)

        # make sure sections info is found by gtkdoc later
        add_custom_command(
            OUTPUT
                "${_output_html_dir}/${_doc_prefix}-sections.txt"
            DEPENDS
                "${_output_html_dir_stamp}"
                "${_output_sgml_stamp}"
                "${_output_tmpl_stamp}"
                "${_xml_file}"
                ${_depends}
            COMMAND ${CMAKE_COMMAND} -E copy "${_output_dir}/${_doc_prefix}-sections.txt" "${_output_html_dir}/${_doc_prefix}-sections.txt"
            WORKING_DIRECTORY "${_output_dir}"
            VERBATIM)

        # add a command to create html directory
        add_custom_command(
            OUTPUT "${_output_html_dir_stamp}" "${_output_html_dir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_output_html_dir}
            COMMAND ${CMAKE_COMMAND} -E touch ${_output_html_dir_stamp}
            VERBATIM)

        # add a command to output HTML
        add_custom_command(
            OUTPUT
                "${_output_html_stamp}"
            DEPENDS
                "${_output_html_dir_stamp}"
                "${_output_sgml_stamp}"
                "${_output_tmpl_stamp}"
                "${_xml_file}"
                "${_output_html_dir}/${_doc_prefix}-sections.txt"
                ${_depends}
            ${_copy_xml_if_needed}
            COMMAND ${GTKDOC_MKHTML_EXE}
                "${_doc_prefix}"
                "${_default_xml_file}"
            COMMAND ${GTKDOC_FIXXREF_EXE}
                "--module=${_doc_prefix}"
                "--module-dir=."
                ${_fixxref_opts}
            ${_remove_xml_if_needed}
            WORKING_DIRECTORY "${_output_html_dir}"
            VERBATIM)

        add_custom_target(doc-${_doc_prefix} ${_do_all}
            DEPENDS "${_output_html_stamp}")
    endif(_opts_valid)
endmacro(gtk_doc_add_module)
