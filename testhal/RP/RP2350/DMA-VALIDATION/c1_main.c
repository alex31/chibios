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

#include "ch.h"
#include "hal.h"

#include "dma_validation.h"

/**
 * Core 1 entry point.
 */
void c1_main(void) {

  /*
   * Starting a new OS instance running on this core, we need to wait for
   * system initialization on the other side.
   */
  chSysWaitSystemState(ch_sys_running);
  chInstanceObjectInit(&ch1, &ch_core1_cfg);

  /* It is alive now.*/
  chSysUnlock();

  c1_ready = 1U;

  /* Waiting for core 0 to hand over a DMA channel to be freed from this
     core, plain SRAM is coherent between the RP2350 cores.*/
  while (c1_free_go == 0U) {
    c1_heartbeat++;
  }

  /* Cross-core free, I-class under this core's own lock.*/
  chSysLock();
  dmaChannelFreeI(c1_free_chp);
  chSysUnlock();

  c1_free_done = 1U;

  while (true) {
    c1_heartbeat++;
  }
}
