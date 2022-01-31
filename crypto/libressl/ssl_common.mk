include common.mk

CFLAGS += -I../../include/compat -I../../include
# exclude from include/compat/string.h
CFLAGS += -DHAVE_STRCASECMP -DHAVE_STRLCPY -DHAVE_STRLCAT -DHAVE_STRNDUP -DHAVE_STRNLEN -DHAVE_STRSEP -DHAVE_MEMMEM
# exclude from include/compat/unistd.h
CFLAGS += -DHAVE_GETPAGESIZE -DHAVE_PIPE2
