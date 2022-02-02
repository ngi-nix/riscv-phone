platform ?= posix

pwd := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
ecp_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
src_dir := $(abspath $(ecp_dir)/src)
platform_dir = $(abspath $(ecp_dir)/platform/$(platform))

include $(platform_dir)/platform.mk
CFLAGS += -I$(src_dir) -I$(platform_dir)
