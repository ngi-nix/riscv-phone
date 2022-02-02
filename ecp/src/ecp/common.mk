platform ?= posix

pwd := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
src_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
platform_dir = $(abspath $(src_dir)/platform/$(platform))

include $(platform_dir)/platform.mk
CFLAGS += -I$(src_dir)/ecp -I$(platform_dir)
