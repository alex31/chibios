/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 * ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker script for the ChibiOS sandbox architecture.
 *
 * Derived from the Zephyr Cortex-M linker script. The code region is the
 * sandbox flash slot, it starts with the sandbox header. The data region
 * is the sandbox RAM slot, the host places the startup stack and the
 * environment at its top, they are only used before the kernel starts.
 */

#include <zephyr/linker/sections.h>
#include <zephyr/devicetree.h>

#include <zephyr/linker/devicetree_regions.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

#define ROMABLE_REGION FLASH
#define RAMABLE_REGION RAM

/* The code region is the whole zephyr,flash chosen node, no load offset. */
#define ROM_ADDR CONFIG_FLASH_BASE_ADDRESS
#define ROM_SIZE (CONFIG_FLASH_SIZE * 1024)

#define RAM_ADDR DT_CHOSEN_SRAM_ADDR
#define RAM_SIZE DT_CHOSEN_SRAM_SIZE

_region_min_align = 4;

#define MPU_ALIGN(region_size) \
    . = ALIGN(_region_min_align)

#include <zephyr/linker/linker-devnull.h>

MEMORY
    {
    FLASH (rx) : ORIGIN = ROM_ADDR, LENGTH = ROM_SIZE
    RAM   (wx) : ORIGIN = RAM_ADDR, LENGTH = RAM_SIZE
#if defined(CONFIG_LINKER_DEVNULL_MEMORY)
    DEVNULL_ROM (rx) : ORIGIN = DEVNULL_ADDR, LENGTH = DEVNULL_SIZE
#endif
    LINKER_DT_REGIONS()
    /* Used by and documented in include/linker/intlist.ld */
    IDT_LIST (wx) : ORIGIN = 0xFFFF7FFF, LENGTH = 32K
    }

ENTRY(CONFIG_KERNEL_ENTRY)

SECTIONS
    {

#include <zephyr/linker/rel-sections.ld>

#ifdef CONFIG_LLEXT
#include <zephyr/linker/llext-sections.ld>
#endif

    /DISCARD/ :
	{
	*(.plt)
	}

    /DISCARD/ :
	{
	*(.iplt)
	}

    GROUP_START(ROMABLE_REGION)

	__rom_region_start = ROM_ADDR;

    SECTION_PROLOGUE(rom_start,,)
	{
	/* The sandbox header must be at the start of the code region. */
	KEEP(*(.sandbox))

/* Located in generated directory. This file is populated by calling
 * zephyr_linker_sources(ROM_START ...).
 */
#include <snippets-rom-start.ld>
	} GROUP_LINK_IN(ROMABLE_REGION)

    SECTION_PROLOGUE(_TEXT_SECTION_NAME,,)
	{
	__text_region_start = .;

#include <zephyr/linker/kobject-text.ld>

	*(.text)
	*(".text.*")
	*(.gnu.linkonce.t.*)

	*(.glue_7t) *(.glue_7) *(.vfp11_veneer) *(.v4_bx)

	. = ALIGN(4);
	} GROUP_LINK_IN(ROMABLE_REGION)

	__text_region_end = .;

#if defined (CONFIG_CPP) || defined(CONFIG_RUST) || defined(CONFIG_ARCH_STACKWALK)
	SECTION_PROLOGUE(.ARM.extab,,)
	{
	*(.ARM.extab* .gnu.linkonce.armextab.*)
	} GROUP_LINK_IN(ROMABLE_REGION)
#endif

	SECTION_PROLOGUE(.ARM.exidx,,)
	{
	__exidx_start = .;
	*(.ARM.exidx* gnu.linkonce.armexidx.*)
	__exidx_end = .;
	} GROUP_LINK_IN(ROMABLE_REGION)

	__rodata_region_start = .;

#include <zephyr/linker/common-rom.ld>
/* Located in generated directory. This file is populated by calling
 * zephyr_linker_sources(ROM_SECTIONS ...). Useful for grouping iterable RO structs.
 */
#include <snippets-rom-sections.ld>
#include <zephyr/linker/thread-local-storage.ld>

    SECTION_PROLOGUE(_RODATA_SECTION_NAME,,)
	{
	*(.rodata)
	*(".rodata.*")
	*(.gnu.linkonce.r.*)

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-rodata.ld>

#include <zephyr/linker/kobject-rom.ld>

	/* Padding so that the data load address is aligned. */
	. = ALIGN(4);
	} GROUP_LINK_IN(ROMABLE_REGION)

#include <zephyr/linker/cplusplus-rom.ld>

	__rodata_region_end = .;
	MPU_ALIGN(__rodata_region_end - ADDR(rom_start));
	__rom_region_end = __rom_region_start + . - ADDR(rom_start);
	__rom_region_size = __rom_region_end - __rom_region_start;

    GROUP_END(ROMABLE_REGION)

    /DISCARD/ : {
	*(.got.plt)
	*(.igot.plt)
	*(.got)
	*(.igot)
	}

    GROUP_START(RAMABLE_REGION)

	. = RAM_ADDR;
	. = ALIGN(_region_min_align);
	_image_ram_start = .;

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-ram-sections.ld>

    GROUP_START(DATA_REGION)

    SECTION_DATA_PROLOGUE(_DATA_SECTION_NAME,,)
	{
	__data_region_start = .;
	__data_start = .;
	*(.data)
	*(".data.*")
	*(".kernel.*")

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-rwdata.ld>

	__data_end = .;

	} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
    __data_size = __data_end - __data_start;
    __data_load_start = LOADADDR(_DATA_SECTION_NAME);

    __data_region_load_start = LOADADDR(_DATA_SECTION_NAME);

#include <zephyr/linker/common-ram.ld>
#include <zephyr/linker/kobject-data.ld>
#include <zephyr/linker/cplusplus-ram.ld>

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-data-sections.ld>

    __data_region_end = .;

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-sections.ld>

#include <zephyr/linker/debug-sections.ld>

    SECTION_PROLOGUE(.ARM.attributes, 0,)
	{
	KEEP(*(.ARM.attributes))
	KEEP(*(.gnu.attributes))
	}

    /* Sections generated from 'zephyr,memory-region' nodes */
    LINKER_DT_SECTIONS()

/* Must be last in romable region */
SECTION_PROLOGUE(.last_section,,)
{
  /* .last_section contains a fixed word to ensure location counter and actual
   * rom region data usage match when CONFIG_LINKER_LAST_SECTION_ID=y. */
  KEEP(*(.last_section))
} GROUP_LINK_IN(ROMABLE_REGION)

/* To provide the image size as a const expression,
 * calculate this value here. */
_flash_used = LOADADDR(.last_section) + SIZEOF(.last_section) - __rom_region_start;

    SECTION_DATA_PROLOGUE(_BSS_SECTION_NAME,(NOLOAD),)
	{
	/*
	 * For performance, BSS section is assumed to be 4 byte aligned and
	 * a multiple of 4 bytes
	 */
	. = ALIGN(4);
	__bss_start = .;
	__kernel_ram_start = .;

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-bss.ld>

	*(.bss)
	*(".bss.*")
	*(COMMON)
	*(".kernel_bss.*")

	/*
	 * As memory is cleared in words only, it is simpler to ensure the BSS
	 * section ends on a 4 byte boundary. This wastes a maximum of 3 bytes.
	 */
	__bss_end = ALIGN(4);
	} GROUP_DATA_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)

#include <zephyr/linker/common-noinit.ld>

    /* Define linker symbols */

    __kernel_ram_end = RAM_ADDR + RAM_SIZE;
    __kernel_ram_size = __kernel_ram_end - __kernel_ram_start;

#include <zephyr/linker/ram-end.ld>

    GROUP_END(RAMABLE_REGION)

    }
