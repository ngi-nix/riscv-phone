# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/esp_rom/include $(IDF_PATH)/components/esp_rom/esp32 $(IDF_PATH)/components/esp_rom/include/esp32
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/esp_rom -lesp_rom -L $(IDF_PATH)/components/esp_rom/esp32/ld -T esp32.rom.ld -T esp32.rom.libgcc.ld -T esp32.rom.syscalls.ld -T esp32.rom.newlib-data.ld -T esp32.rom.api.ld -T esp32.rom.newlib-funcs.ld -T esp32.rom.newlib-time.ld -lesp_rom -Wl,--wrap=longjmp
COMPONENT_LINKER_DEPS += $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.libgcc.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.syscalls.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.newlib-data.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.api.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.newlib-funcs.ld $(IDF_PATH)/components/esp_rom/esp32/ld/esp32.rom.newlib-time.ld
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += esp_rom
COMPONENT_LDFRAGMENTS += 
component-esp_rom-build: 
