##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

ifeq ($(USE_OPT),)
  USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=4
endif

ifeq ($(USE_COPT),)
  USE_COPT =
endif

ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

ifeq ($(USE_LDOPT),)
  USE_LDOPT =
endif

ifeq ($(USE_LTO),)
  USE_LTO = no
endif

ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, target, sources and paths
#

PROJECT = ch

MCU = rv32imac_zba_zbb_zbs_zbkb_zcb_zcmp

CHIBIOS  := ../../..
CONFDIR  := ./cfg
BUILDDIR := ./build/RISCV-HAZARD3
DEPDIR   := ./.dep/RISCV-HAZARD3

include $(CHIBIOS)/os/license/license.mk
include $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/mk/startup_rp2350_riscv.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/RISCV-HAZARD3/compilers/GCC/mk/port.mk
include $(CHIBIOS)/tools/mk/autobuild.mk

LDSCRIPT = $(STARTUPLD)/RP2350_RISCV_FLASH.ld

CSRC = $(ALLCSRC) \
       main.c

CPPSRC = $(ALLCPPSRC)
ASMSRC = $(ALLASMSRC)
ASMXSRC = $(ALLXASMSRC)

INCDIR = $(CONFDIR) $(ALLINC)

CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes
CPPWARN = -Wall -Wextra -Wundef

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

UDEFS = -DRP2350 \
        -DCH_DBG_SYSTEM_STATE_CHECK=TRUE \
        -DCH_DBG_ENABLE_CHECKS=TRUE \
        -DCH_DBG_ENABLE_ASSERTS=TRUE \
        -DCH_DBG_ENABLE_STACK_CHECK=TRUE \
        -DPORT_ENABLE_GUARD_PAGES=TRUE

UADEFS = -DRP2350 \
         -DCH_DBG_SYSTEM_STATE_CHECK=TRUE \
         -DCH_DBG_ENABLE_CHECKS=TRUE \
         -DCH_DBG_ENABLE_ASSERTS=TRUE \
         -DCH_DBG_ENABLE_STACK_CHECK=TRUE \
         -DPORT_ENABLE_GUARD_PAGES=TRUE
UINCDIR =
ULIBDIR =
ULIBS =

#
# End of user section
##############################################################################

##############################################################################
# Common rules
#

RULESPATH = $(CHIBIOS)/os/common/startup/RISCV-HAZARD3/compilers/GCC/mk
include $(RULESPATH)/riscv-none-elf.mk
include $(RULESPATH)/rules.mk

#
# Common rules
##############################################################################
