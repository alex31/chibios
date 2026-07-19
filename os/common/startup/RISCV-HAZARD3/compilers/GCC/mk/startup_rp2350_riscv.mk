# List of the ChibiOS generic RP2350 RISC-V Hazard3 startup files.

STARTUPSRC =

STARTUPASM = $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/crt0_hazard3.S \
             $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/crt0_c1_hazard3.S \
             $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/vectors_hazard3.S \
             $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/devices/RP2350/rp2350_riscv_imagedef.S

STARTUPINC = $(CHIBIOS)/os/common/portability/GCC \
             $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC \
             $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/devices/RP2350 \
             $(CHIBIOS)/os/common/ext/RISCV \
             $(CHIBIOS)/os/common/ports/RISCV-common/include \
             $(CHIBIOS)/os/common/ext/RP \
             $(CHIBIOS)/os/common/ext/RP/RP2350

STARTUPLD  = $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/ld

# Note: the per-core stacks live in the 4KiB scratch banks (ram4/ram5)
# together with CH_MEM_LOCAL_BSS()/CH_MEM_LOCAL_COHERENT_BSS() annotated
# data, 0x400+0x400 leaves half of each bank for annotated data.
USE_EXCEPTIONS_STACKSIZE ?= 0x400
USE_PROCESS_STACKSIZE    ?= 0x400

# Shared variables
ALLXASMSRC += $(STARTUPASM)
ALLCSRC    += $(STARTUPSRC)
ALLINC     += $(STARTUPINC)
