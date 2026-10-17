# ThreadX kernel files.

THREADXDIR := $(CHIBIOS)/ext/threadx/common

THREADXSRC = $(sort $(wildcard $(THREADXDIR)/src/*.c))

THREADXINC = $(THREADXDIR)/inc

# The application configuration is in tx_user.h.
DDEFS += -DTX_INCLUDE_USER_DEFINE_FILE

# Shared variables
ALLCSRC += $(THREADXSRC)
ALLINC  += $(THREADXINC)
