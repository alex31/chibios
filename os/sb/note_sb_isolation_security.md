# Note: SB Isolation / Escape Resistance

Assessment of whether a thread running inside a sandbox can escape the
isolation, i.e. trigger arbitrary privileged code execution or crash the
supervisor. Covers the ALT ports (`os/common/ports/ARMv7-M-ALT`,
`ARMv8-M-ML-ALT`) and `os/sb/host`. Date: 2026-06-11.

Related: [note_svc_mpu_optimizations.md](note_svc_mpu_optimizations.md)
(the shared-memory region API interacts directly with point 1 below).

## Defenses currently in place (sound)

- **Hardware-enforced entry stacking.** On `svc`, the exception frame is
  stacked using the current (unprivileged) PSP, so the MPU applies. A
  sandbox that points PSP at host memory faults (MemManage) during
  stacking, before any privileged instruction runs. The sandbox cannot
  redirect the entry frame into supervisor memory.
- **Privileged dispatcher never runs on a guest stack.**
  `__port_do_syscall_entry` (`host/sbsyscall.c`) builds the return context
  at `tp->waend - sizeof(extctx)` (kernel-owned privileged stack). The
  user PSP is only saved (`sbp->u_psp`), never trusted as a stack pointer.
- **Dispatch index cannot go OOB by construction.** Handler splits on
  `cmp r0, #0x80`; syscall path does `subs #128` into a 128-entry table.
  SVC immediates are one byte, so 0..127 and 128..255 map exactly onto the
  two 128-entry tables. Unimplemented slots default to `sb_undef_handler`
  returning `ENOSYS`.
- **Buffer arguments validated against the sandbox's own regions.**
  `sb_is_valid_read_range` / `sb_is_valid_write_range` / `sb_check_string`
  (`host/sbregions.c`) walk `sbp->regions[]`, require `sb_reg_is_memory`,
  and for writes require `sb_reg_is_writable`. Device regions are excluded
  from syscall buffers.
- **Fail-safe on error.** A VRQ frame that does not fit the sandbox stack
  is "doomed" via `vrq_set_doomed` (`host/sbvrq.c`), pointing the return PC
  at a kernel address outside all sandbox regions -> immediate fault ->
  termination. Faults taken in unprivileged thread mode are converted by
  `sbexc.S` (`.Lexit_thd`) into `chThdExit`/`__sb_abort`, so a sandbox
  crash is a clean termination, not a supervisor crash.

## Margins / hardening points

### 1. TOCTOU / double-fetch (highest priority, gated by shared-memory API)

All range checks are validate-then-use. The single-threaded-sandbox
assumption holds *within one SB*, but is broken by:

- a region writable by both the sandbox and a peripheral/DMA engine;
- VRQ preemption re-entering the sandbox between check and use;
- **the planned shared-memory region API** (another SB or the host mutating
  a validated buffer mid-syscall).

Sharpest case: `sb_check_string` scans for the terminator to bound length,
then the VFS layer re-reads the string -> classic double-fetch if a
concurrent writer changes it in between.

**Constraint for the shared-memory API and any shared/DMA-backed buffer:**
copy-in to a private privileged buffer once, validate and use the copy,
never re-dereference guest memory after the check. Bake this into the API
contract from the start.

### 2. Syscall number read from guest code memory

The ALT handler recovers the SVC number with a privileged
`ldrb [stacked_pc, #-2]` — a privileged read at a guest-influenced address.
Not exploitable for escape (PC was just executed, within the code region),
but it is a privileged access derived from guest state. Moving the number
to R12 (optimization #2 in the optimizations note) removes the surface and
is faster — security and performance agree.

### 3. FPCA / lazy-stacking invariant is convention, not enforced

`__port_do_syscall_entry` / `__port_do_syscall_return` require that syscall
code does not change FPCA state, or the return uses the wrong frame shape.
A handler that touches the FPU could corrupt the return frame (supervisor
crash, not escape). Add a compile-time / debug assertion instead of relying
on reviewers.

### 4. Privileged stack overflow is guard-page-dependent

Entry skips overflow checking on the privileged frame ("created on the
privileged PSP so no need to check"). True only while the guard page backs
`SB_CFG_PRIVILEGED_STACK_SIZE`. Deep host call chains under a syscall (VFS
path resolution, lwIP) are the realistic consumers. v8-M sets PSPLIM; on
**v7-M the guard MPU region is the only backstop** — confirm it covers the
privileged stack base for every sandbox config. A privileged overflow that
misses the guard is direct supervisor-memory corruption.

### 5. VRQ injection vs. syscall-return window

The `vrq.isr` / `SB_VRQ_ISR_DISABLED` masking discipline prevents a VRQ
being injected during the return transition. `sbVRQTriggerFromISR` fires
asynchronously, so the masking atomicity across the entry -> dispatch ->
return sequence needs explicit re-review (see open_points "Host").

## Bottom line

Architecture is fundamentally sound; the escape surface is small. The
concentrated risk is TOCTOU on memory the sandbox does not exclusively own,
which the shared-memory API activates — hence the copy-in-once discipline.
Items 2-5 are supervisor-integrity (crash-resistance) hardening rather than
escape vectors, with #4 the one to verify concretely per-config.
