# List of the FreeRTOS port files for ChibiOS ARMv7/8-M sandboxes.

FREERTOSPORTSRC = $(CHIBIOS)/os/various/freertos_bindings/portable/GCC/ARMvx-M-SB/port.c \
                  $(CHIBIOS)/os/various/freertos_bindings/portable/GCC/ARMvx-M-SB/porthooks.c

FREERTOSPORTASM = $(CHIBIOS)/os/various/freertos_bindings/portable/GCC/ARMvx-M-SB/portasm.S \
                  $(CHIBIOS)/os/common/ports/ARMvx-M-SB/compilers/GCC/vectors.S

FREERTOSPORTINC = $(CHIBIOS)/os/various/freertos_bindings/portable/GCC/ARMvx-M-SB

# The common VRQ entry code invokes the FreeRTOS preemption hook.
DADEFS += -DSB_VECTORS_NORTOS -DSB_VECTORS_EPILOGUE_HOOK=vPortVRQEpilogue

# Shared variables
ALLXASMSRC += $(FREERTOSPORTASM)
ALLCSRC    += $(FREERTOSPORTSRC)
ALLINC     += $(FREERTOSPORTINC)
