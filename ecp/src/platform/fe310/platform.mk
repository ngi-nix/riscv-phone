fe310_dir = $(abspath $(src_dir)/../../fw/fe310)
include $(fe310_dir)/platform.mk
CFLAGS += -I$(fe310_dir) -DECP_WITH_VCONN=1 -DECP_DEBUG=1 -D__FE310__
