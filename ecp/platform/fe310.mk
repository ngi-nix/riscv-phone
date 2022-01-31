FE310_HOME ?= $(PWD)/../../fw/fe310
export FE310_HOME

include $(FE310_HOME)/common.mk
CFLAGS	+= -I$(FE310_HOME) -DECP_WITH_VCONN=1 -DECP_DEBUG=1 -D__FE310__

platform = fe310
