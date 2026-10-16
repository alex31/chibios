# FreeRTOS kernel files.

FREERTOSDIR := $(CHIBIOS)/ext/FreeRTOS-Kernel

# Heap implementation, from 1 to 5, or "none" if only static allocation
# is used.
ifeq ($(USE_FREERTOS_HEAP),)
  USE_FREERTOS_HEAP = 4
endif

FREERTOSSRC = $(FREERTOSDIR)/croutine.c \
              $(FREERTOSDIR)/event_groups.c \
              $(FREERTOSDIR)/list.c \
              $(FREERTOSDIR)/queue.c \
              $(FREERTOSDIR)/stream_buffer.c \
              $(FREERTOSDIR)/tasks.c \
              $(FREERTOSDIR)/timers.c

ifneq ($(USE_FREERTOS_HEAP),none)
FREERTOSSRC += $(FREERTOSDIR)/portable/MemMang/heap_$(USE_FREERTOS_HEAP).c
endif

FREERTOSINC = $(FREERTOSDIR)/include

# Shared variables
ALLCSRC += $(FREERTOSSRC)
ALLINC  += $(FREERTOSINC)
