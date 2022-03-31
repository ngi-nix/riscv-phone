platform ?= posix

pwd := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))
src_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
platform_dir = $(abspath $(src_dir)/platform/$(platform))

include $(platform_dir)/platform.mk
include $(platform_dir)/features.mk
CFLAGS += -I$(src_dir)/ecp -I$(platform_dir)

ifeq ($(with_dirsrv),yes)
with_dir = yes
endif

ifeq ($(with_pthread),yes)
CFLAGS += -DECP_WITH_PTHREAD=1
endif

ifeq ($(with_htable),yes)
CFLAGS += -DECP_WITH_HTABLE=1
subdirs	+= htable
endif

ifeq ($(with_vconn),yes)
CFLAGS += -DECP_WITH_VCONN=1
subdirs	+= vconn
endif

ifeq ($(with_rbuf),yes)
CFLAGS += -DECP_WITH_RBUF=1
subdirs	+= ext
endif

ifeq ($(with_msgq),yes)
CFLAGS += -DECP_WITH_MSGQ=1
endif

ifeq ($(with_dir),yes)
CFLAGS += -DECP_WITH_DIR=1
subdirs	+= dir
endif

ifeq ($(with_dirsrv),yes)
CFLAGS += -DECP_WITH_DIRSRV=1
endif

ifeq ($(with_debug),yes)
CFLAGS += -DECP_DEBUG=1
endif
