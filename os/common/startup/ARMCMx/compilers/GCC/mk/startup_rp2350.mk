# List of the ChibiOS generic RP2350 startup and CMSIS files.
STARTUPSRC = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/crt1.c

STARTUPASM = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/crt0_v8m-ml.S \
             $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/vectors.S \
             $(CHIBIOS)/os/common/startup/ARMCMx/devices/RP2350/rp2350_imagedef.S

STARTUPINC = $(CHIBIOS)/os/common/portability/GCC \
             $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC \
             $(CHIBIOS)/os/common/startup/ARMCMx/devices/RP2350 \
             $(CHIBIOS)/os/common/ext/ARM/CMSIS/Core/Include \
             $(CHIBIOS)/os/common/ext/RP \
             $(CHIBIOS)/os/common/ext/RP/RP2350

STARTUPLD  = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/ld

# Note: the per-core stacks live in the 4KiB scratch banks (ram4/ram5)
# together with CH_MEM_LOCAL_BSS()/CH_MEM_LOCAL_COHERENT_BSS() annotated
# data, 0x400+0x400 leaves half of each bank for annotated data.
USE_EXCEPTIONS_STACKSIZE ?= 0x400
USE_PROCESS_STACKSIZE    ?= 0x400

# Shared variables
ALLXASMSRC += $(STARTUPASM)
ALLCSRC    += $(STARTUPSRC)
ALLINC     += $(STARTUPINC)
