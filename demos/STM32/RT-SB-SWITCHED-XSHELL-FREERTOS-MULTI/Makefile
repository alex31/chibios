##############################################################################
# Multi-project makefile rules
#

.PHONY: all clean

SUB_MAKES := $(sort $(wildcard make/*.make))

# Each target also builds the sandbox images for its own slots.
all:
	@for m in $(SUB_MAKES); do \
		echo; \
		echo "=== Building $$m ==="; \
		$(MAKE) --no-print-directory -f $$m all || exit $$?; \
		echo; \
	done
	@echo

clean:
	@for m in $(SUB_MAKES); do \
		echo "=== Cleaning $$m ==="; \
		$(MAKE) --no-print-directory -f $$m clean || exit $$?; \
		echo; \
	done
#
##############################################################################
