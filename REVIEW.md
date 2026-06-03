# ChibiOS Pull-Request Review Procedure

This is the review procedure for ChibiOS pull requests, carried out by the
`chibios-sheriff` assistant (first layer) and the human maintainers (final say).
It also serves as the review-guidance file for any automated reviewer.

## Role & boundaries
- The automated review is a **first layer and advisory**. It never merges; a human
  maintainer (the project lead or the relevant port owner) makes the merge decision.
- An automated approval is **advisory** and does not substitute for human review.
- **Fork PRs:** *compile* the change, but do **not run** the resulting binary on the
  host (the POSIX simulator would execute contributor code). Run only in a sandbox if
  execution is genuinely needed.

## 0. Intake
- Capture: repo, PR number, author, **base branch**, size (files, +/-), description, linked issue.
- **Targeting rule:** `stable-*` branches take **bugfixes/regressions only**; `main` takes
  features and fixes. Flag mis-targeted PRs.
- Scope sanity: reasonable size, on-topic, not bundling unrelated changes.

## 1. Fetch
- `gh pr checkout <n>` into a worktree/scratch; `git submodule update --init --recursive`
  if `ext` or `tools/ftl` are touched.

## 2. Mechanical checks (objective — run first)
- **Style:** `tools/style/stylecheck.py` on the changed `.c`/`.h` files.
- **Build:** the POSIX simulator (host gcc) plus the most relevant ARM target(s) for the
  touched port(s).
- Any mechanical failure → **request changes** before spending judgment time.

## 3. Judgment review, by dimension
- **Correctness & concurrency (RTOS):** logic, races, ISR-safety, no blocking in ISR,
  critical-section/locking discipline, thread priorities.
- **Hardware/registers:** bitfields, masks, channel counts vs. the datasheet.
- **HAL/port conventions:** driver model, LLD structure, OSAL usage, naming,
  `.chcfg`/`.mk` consistency.
- **Portability:** does not break other ports/configs; `#if` conditionals correct; no
  accidental coupling.
- **API/ABI stability:** on `stable-*`, no public API breaks; backward compatibility.
- **Resource/safety:** no prohibited dynamic allocation, stack/footprint, deterministic
  timing, MISRA-oriented hygiene.
- **Headers/docs/tests:** license headers present/correct; doxygen comments updated;
  test suite updated or added when behavior changes.

## 4. Findings & verdict
- Severity: **blocking** (correctness/safety/API break, or build/style failure),
  **should-fix** (conventions), **nit** (cosmetic).
- Verdict: **request changes** if any blocking item; **comment** if only should-fix/nit;
  **advisory-LGTM** if clean.
- For small, clear issues, offer to draft/push a fix to the PR branch.

## 5. Post & handoff
- Post inline comments on the relevant lines plus a summary block with severity-tagged
  findings.
- Summarize to the maintainer; **the human decides the merge.**

## Review output template
Post the review as one summary block (plus inline comments on specific lines where useful),
using this shape:

```
🤖 chibios-sheriff — first-layer review (advisory)   ← automated reviews MUST open with this AI-disclosure line

Scope: <what changed, target branch — one line>

Blocking:    <correctness/safety/API-break/build or style failures — or omit if none>
Should-fix:  <convention/quality items — or omit>
Notes:       <non-blocking nits/observations — or omit>

Mechanical gates: <style + build result, or what wasn't run this pass>

Verdict: request changes | comment | advisory-LGTM  (+ any condition)
```

Human reviewers use the same structure, minus the `🤖` disclosure line (that line is required
for automated reviewers, which can't carry GitHub's native `[bot]` badge).

## Cadence
- Pull-based: a maintainer brings a PR to a review session.
