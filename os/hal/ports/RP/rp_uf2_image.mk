# Make rules for building an .uf2 image, suitable for use with the bootloader
# on the RP MCUs.

# These make rules should be included _BEFORE_ $(RULESPATH)/rules.mk.

# Path to the picotool binary, can be overridden.
# picotool is distributed by Raspberry Pi at
# https://github.com/raspberrypi/picotool
PICOTOOL ?= picotool

# Rules defined below would otherwise become the default goal because this
# file is included before $(RULESPATH)/rules.mk, ensure the default goal
# stays "all" as defined there.
ifeq ($(.DEFAULT_GOAL),)
.DEFAULT_GOAL := all
endif

# The .uf2 image is only built if picotool is already installed, it is never
# downloaded or installed automatically. If picotool is not found the .uf2
# image is simply not built as part of the "all" target.
ifneq ($(shell command -v $(PICOTOOL) 2>/dev/null),)

# Build a .uf2 file out of the .elf to use with the bootloader of the RP MCUs.
$(BUILDDIR)/$(PROJECT).uf2: $(BUILDDIR)/$(PROJECT).elf
ifeq ($(USE_VERBOSE_COMPILE),yes)
	$(PICOTOOL) uf2 convert $(BUILDDIR)/$(PROJECT).elf $(BUILDDIR)/$(PROJECT).uf2
else
	@echo Creating $@
	@$(PICOTOOL) uf2 convert $(BUILDDIR)/$(PROJECT).elf $(BUILDDIR)/$(PROJECT).uf2
endif

# Build the .uf2 as part of the regular build target ("all").
ADDITIONAL_OUTFILES += $(BUILDDIR)/$(PROJECT).uf2

else

# picotool not available, targets explicitly requiring the .uf2 image fail
# with a clear message.
$(BUILDDIR)/$(PROJECT).uf2:
	$(error picotool not found. Download it from https://github.com/raspberrypi/picotool and either install it in $$PATH or set the PICOTOOL variable to its location)

endif
