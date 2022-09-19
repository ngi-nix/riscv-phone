# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/soc/include $(IDF_PATH)/components/soc/esp32 $(IDF_PATH)/components/soc/esp32/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/soc -lsoc -T $(IDF_PATH)/components/soc/esp32/ld/esp32.peripherals.ld
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/soc $(IDF_PATH)/components/soc/esp32/ld/esp32.peripherals.ld
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += soc
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/soc/linker.lf
component-soc-build: 
