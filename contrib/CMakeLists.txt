
add_subdirectory(its)
if(VAPI)
	add_subdirectory(vapi)
endif()
if(APT_SUPPORT)
	add_subdirectory(apt-conf)
endif()
