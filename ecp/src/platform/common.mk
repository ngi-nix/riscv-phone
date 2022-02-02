platform ?= posix
src_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
include $(src_dir)/ecp/common.mk
