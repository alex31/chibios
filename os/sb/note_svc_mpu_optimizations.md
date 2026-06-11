# Note: SVC / MPU Context Switch Optimizations (ALT ports)

Analysis of the shared SVC path used by both RT context switching and the
SB syscall mechanism in the ALT ports (`os/common/ports/ARMv7-M-ALT`,
applies conceptually to `ARMv8-M-ML-ALT` too). Findings ordered by
expected payoff. Date: 2026-06-11.

## Background

`SVC_Handler` in `ARMv7-M-ALT/compilers/GCC/chcoreasm.S` serves three roles:

1. Synchronous context switch: `svc #0` from privileged mode, issued by
   `__port_switch()` with ntp/otp in R0/R1.
2. Syscall return: `svc 1` from privileged mode at the end of
   `__sb_dispatch_syscall`.
3. Syscall/fastcall entry from unprivileged (sandbox) mode.

Discrimination is done first on CONTROL.nPRIV, then by **reading the SVC
immediate from code memory** (`ldr` of the stacked PC, then
`ldrb [pc, #-2]`).

## 1. Eliminate the SVC immediate fetch on the privileged path (best win)

The `ldrb` of the immediate is a data read from code memory (flash wait
states, on M7 typically missing the ART/cache) and is paid on **every**
`port_switch`, i.e. the hottest RT path, only to distinguish it from the
much rarer syscall return.

Both privileged callers are kernel-controlled, so a register convention
can replace the immediate: ntp (R0) is never NULL for a real switch, so
the dispatcher's return trap can issue `movs r0, #0; svc 0` and the
handler discriminates with a single `cbz r0` after the `ldmia r2, {r0, r1}`
it already executes. Removes the stacked-PC load and the flash byte read
from both privileged paths; also removes the overhead that
`PORT_USE_SYSCALL` currently imposes on non-SB context switches.

## 2. Pass the syscall number in a register instead of the SVC immediate

The unprivileged entry path performs the same stacked-PC + `ldrb` dance,
but reading from the *sandbox's* code memory, which may be external/QSPI
flash (tens of cycles for a random data read). Changing the user-side
stubs in `user/sbuser.h` to "n in R12, always `svc #0`" lets the handler
read the number from the hardware-stacked frame in SRAM
(`ldr r0, [r1, #16]`). Side benefit: no privileged load at an address
derived from guest state.

Caveat: this is a guest-visible ABI break, sandbox binaries must be
rebuilt. Needs a config option or an SB ABI version bump.

## 3. MPU switched regions: drop store-back, burst the load

`PORT_STORE_MPU_CONTEXT` / `PORT_LOAD_MPU_CONTEXT` treat the switched MPU
regions as mutable context (per switch, up to 4x RNR+RBAR+RASR accesses
in each direction).

- **Store-back is redundant**: regions enter the system only through the
  thread context copy (`utp->ctx.regions[]`, filled in `sbhost.c` at spawn)
  and the live registers are only written by the switch code itself, so
  switch-out reads back exactly what switch-in wrote. Eliminating the
  store halves the MPU MMIO traffic. New invariant required: nothing may
  write the switched MPU regions directly while a thread runs, all changes
  funnel through `ctx.regions` (guard page region is separate, unaffected).
- **Burst load via alias registers**: pre-OR `VALID | region#` into the
  stored RBAR values (one-time, in `get_mpu_settings()`), eliminating RNR
  writes; then use the ARMv7-M RBAR/RASR alias registers
  (0xE000ED9C-0xE000EDB8) to program all 4 regions with one
  `ldmia r0!, {r2-r9}` + `stmia`. Roughly 8 MMIO stores total versus ~24
  MMIO accesses today. Benefits PendSV preemption switches too. No
  explicit DSB/ISB needed, exception return serializes (already relied
  upon).

## 4. VRQ pending check on syscall return (minor)

`__sb_vrq_check_pending()` (`host/sbvrq.c`) loads `vrq.isr`, `vrq.wtmask`,
`vrq.enmask` and masks them on every syscall return. The mutation sites
are few and slow-path; a single precomputed "deliverable" word maintained
at post/enable/mask time reduces the hot path to one load + branch.

## Deliberately left alone

- Fastcall path: already minimal (handler mode, one table lookup).
- Two-exception syscall design (entry SVC -> privileged dispatcher ->
  exit SVC): ~4 hardware mode transitions of floor cost, but it is what
  makes syscalls preemptable/blockable. A thread-mode return (manual
  unstack + privilege drop) could shave ~15-20 cycles but interacts badly
  with FPCA/lazy stacking and nPRIV ordering. Poor risk/reward.
- `__sb_dispatch_syscall` table dispatch: already minimal.

## Shared-memory API interaction (planned)

Security note: a shared-memory region is the feature that activates the
TOCTOU / double-fetch risk described in
[note_sb_isolation_security.md](note_sb_isolation_security.md) point 1.
The API contract must require copy-in-once for any buffer backed by shared
memory (validate and use a private privileged copy, never re-dereference
guest memory after the check).

A future API adding a shared-memory MPU region must update both the
stored settings and, conditionally, the live registers:

- **Target sandbox thread is current** (e.g. the sandbox requests the
  mapping via syscall): the live MPU registers must be written. Under the
  current store-back scheme, writing only `ctx.regions` is silently
  discarded (store-back overwrites it at switch-out). Under the load-only
  scheme of point 3, write both.
- **Target not running**: write only `ctx.regions` / `sbp->regions`;
  touching the live registers would clobber the running thread's view.

Both the update and the is-current check must happen in one critical
section. The API fits the load-only discipline naturally (one helper
doing "always ctx.regions, plus live MPU iff current").

Constraints:

- `SB_CFG_NUM_REGIONS` and `PORT_SWITCHED_REGIONS_NUMBER` are both capped
  at 4, matching the 4 RBAR/RASR alias slots. A shared region taken out of
  the existing 1..4 budget composes cleanly; a dedicated 5th region needs
  the caps raised and the burst load becomes stm-of-4 plus one RNR-based
  write.
- The same RBAR/RASR encoding helper (`get_mpu_settings()`) must be used
  by the new API, especially once VALID|region# is pre-OR'ed into stored
  RBAR values.
- PMSAv7 alignment: power-of-2 size, size-aligned base; constrain the API
  or use subregion-disable bits. Design against v7-M as the lowest common
  denominator (v8-M-ML gets base/limit semantics for free).
- SMP: a target running on another core cannot have its MPU written
  remotely; would need a forced reschedule/IPI on that core or an API
  restriction to self/sleeping targets.
