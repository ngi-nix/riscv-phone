# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/esp_phy/include $(IDF_PATH)/components/esp_phy/esp32/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp_phy -lesp_phy -L$(IDF_PATH)/components/esp_phy/lib/esp32 -lphy -lrtc
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/esp_phy/lib/esp32/libphy.a $(IDF_PATH)/components/esp_phy/lib/esp32/librtc.a
COMPONENT_SUBMODULES += $(IDF_PATH)/components/esp_phy/lib
COMPONENT_LIBRARIES += esp_phy
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/esp_phy/linker.lf
component-esp_phy-build: 
