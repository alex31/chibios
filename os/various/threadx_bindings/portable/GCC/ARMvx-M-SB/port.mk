# List of the ThreadX port files for ChibiOS ARMv7/8-M sandboxes.

THREADXPORTSRC = $(CHIBIOS)/os/various/threadx_bindings/portable/GCC/ARMvx-M-SB/tx_port.c

THREADXPORTASM = $(CHIBIOS)/os/various/threadx_bindings/portable/GCC/ARMvx-M-SB/tx_port_asm.S \
                 $(CHIBIOS)/os/common/ports/ARMvx-M-SB/compilers/GCC/vectors.S

THREADXPORTINC = $(CHIBIOS)/os/various/threadx_bindings/portable/GCC/ARMvx-M-SB

# The common VRQ entry code invokes the ThreadX preemption hook.
DADEFS += -DSB_VECTORS_NORTOS -DSB_VECTORS_EPILOGUE_HOOK=_tx_sb_vrq_epilogue

# Shared variables
ALLXASMSRC += $(THREADXPORTASM)
ALLCSRC    += $(THREADXPORTSRC)
ALLINC     += $(THREADXPORTINC)
