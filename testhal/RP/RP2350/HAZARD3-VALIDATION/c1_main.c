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

void c1_main(void) {
  extern semaphore_t smp_sem;
  extern volatile bool smp_run;
  extern volatile bool smp_done;
  extern volatile uint32_t smp_count;
  unsigned i;

  chSysWaitSystemState(ch_sys_running);
  chInstanceObjectInit(&ch1, &ch_core1_cfg);
  chSysUnlock();

  while (!__atomic_load_n(&smp_run, __ATOMIC_ACQUIRE)) {
    chThdSleepMicroseconds(100U);
  }

  for (i = 0U; i < 64U; i++) {
    chThdSleepMicroseconds(100U);
    __atomic_fetch_add(&smp_count, 1U, __ATOMIC_RELAXED);
    if (i == 63U) {
      __atomic_store_n(&smp_done, true, __ATOMIC_RELEASE);
    }
    chSemSignal(&smp_sem);
  }

  while (true) {
    chThdSleepMilliseconds(1000U);
  }
}
