/*
    ChibiOS - Copyright (C) 2006-2026 Giovanni Di Sirio.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/* Size of the system block, SB_SYSBLOCK_ALIGN in sbsysc.h, the host checks
   that sb_sysblock_t has this size.*/
__sb_sysblock_size__ = 32;

SECTIONS
{
    /* The default heap uses the (statically) unused part of a RAM section,
       up to the system block.*/
    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_base__ = .;
        . = ORIGIN(DATA_SPACE) + LENGTH(DATA_SPACE) - __sb_sysblock_size__;
        __heap_end__ = .;
    } > DATA_SPACE

    /* System block, reserved at the top of the data area where the host
       places it.*/
    .sbsysblock (NOLOAD) :
    {
        __sb_sysblock_base__ = .;
        . += __sb_sysblock_size__;
        __sb_sysblock_end__ = .;
    } > DATA_SPACE
}

ASSERT((__sb_sysblock_base__ & 31) == 0, "misaligned system block")
