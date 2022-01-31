platform ?= posix
src_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../src)
include $(src_dir)/common.mk
