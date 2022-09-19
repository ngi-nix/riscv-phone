# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += 
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/cxx -lcxx -Wl,--wrap=_Unwind_SetEnableExceptionFdeSorting -Wl,--wrap=__register_frame_info_bases -Wl,--wrap=__register_frame_info -Wl,--wrap=__register_frame -Wl,--wrap=__register_frame_info_table_bases -Wl,--wrap=__register_frame_info_table -Wl,--wrap=__register_frame_table -Wl,--wrap=__deregister_frame_info_bases -Wl,--wrap=__deregister_frame_info -Wl,--wrap=_Unwind_Find_FDE -Wl,--wrap=_Unwind_GetGR -Wl,--wrap=_Unwind_GetCFA -Wl,--wrap=_Unwind_GetIP -Wl,--wrap=_Unwind_GetIPInfo -Wl,--wrap=_Unwind_GetRegionStart -Wl,--wrap=_Unwind_GetDataRelBase -Wl,--wrap=_Unwind_GetTextRelBase -Wl,--wrap=_Unwind_SetIP -Wl,--wrap=_Unwind_SetGR -Wl,--wrap=_Unwind_GetLanguageSpecificData -Wl,--wrap=_Unwind_FindEnclosingFunction -Wl,--wrap=_Unwind_Resume -Wl,--wrap=_Unwind_RaiseException -Wl,--wrap=_Unwind_DeleteException -Wl,--wrap=_Unwind_ForcedUnwind -Wl,--wrap=_Unwind_Resume_or_Rethrow -Wl,--wrap=_Unwind_Backtrace -Wl,--wrap=__cxa_call_unexpected -Wl,--wrap=__gxx_personality_v0 -u __cxx_fatal_exception
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += cxx
COMPONENT_LDFRAGMENTS += 
component-cxx-build: 
