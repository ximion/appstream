# CMake macros to parse options

macro(parse_options _list_names _list_variables)
    list(LENGTH ${_list_names} _list_names_length)
    list(LENGTH ${_list_variables} _list_variables_length)
    #message(STATUS "${_list_names} => ${_list_names_length}")
    #message(STATUS "${_list_variables} => ${_list_variables_length}")

    if(NOT _list_names_length EQUAL _list_variables_length)
        message(SEND_ERROR "names list must have same length as var list")
    endif(NOT _list_names_length EQUAL _list_variables_length)

    set(_list_length ${_list_names_length})

    # Set each list to be empty initially
    foreach(_idx RANGE -${_list_length} -1)
        list(GET ${_list_variables} ${_idx} _variable)
        set(${_variable} "")
    endforeach(_idx RANGE ${_list_length})

    set(_current_list "")
    foreach(_arg ${ARGN})
        set(_skip_arg 0)

        foreach(_idx RANGE -${_list_length} -1)
            list(GET ${_list_names} ${_idx} _name)
            if(_arg STREQUAL _name)
                set(_current_list "${_arg}")
                set(_skip_arg 1)
            endif(_arg STREQUAL _name)
        endforeach(_idx RANGE -${_list_length} -1)

        if(NOT _current_list)
            message(SEND_ERROR "Error parsing args (unknown argument '${_arg}').")
            set(_skip_arg 1)
        endif(NOT _current_list)

        if(NOT _skip_arg)
            #message(STATUS "list: '${_current_list}', arg: '${_arg}'")
            foreach(_idx RANGE -${_list_length} -1)
                list(GET ${_list_names} ${_idx} _name)
                if(_current_list STREQUAL _name)
                    list(GET ${_list_variables} ${_idx} _variable)
                    list(APPEND ${_variable} "${_arg}")
                endif(_current_list STREQUAL _name)
            endforeach(_idx RANGE -${_list_length} -1)
        endif(NOT _skip_arg)
    endforeach(_arg ${ARGN})
endmacro(parse_options)
