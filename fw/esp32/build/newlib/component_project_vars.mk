# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/newlib/platform_include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/newlib -lnewlib -lc -lm -u newlib_include_heap_impl -u newlib_include_syscalls_impl -u newlib_include_pthread_impl -u newlib_include_assert_impl
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += newlib
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/newlib/newlib.lf $(IDF_PATH)/components/newlib/system_libs.lf
component-newlib-build: 
