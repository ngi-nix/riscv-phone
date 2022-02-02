fe310_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
bsp_dir := $(abspath $(fe310_dir)/bsp)
eos_dir := $(abspath $(fe310_dir)/eos)
ext_dir := $(abspath $(fe310_dir)/../../ext)

include $(fe310_dir)/platform.mk
CFLAGS += -I$(eos_dir)
