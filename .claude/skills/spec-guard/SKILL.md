---
name: spec-guard
description: >
  Guards all work in this repository against violating the engineering spec in
  vibe-docs/spec (T0 key metrics and §1–§8 / §1T). Consult this skill whenever you
  are about to write or modify code, pick a technical approach, touch the build
  system / CI / tests / git workflow, or whenever a developer request or a
  developer's specific need would — if implemented as stated — break any spec rule.
  When a request conflicts with the spec, do NOT silently comply and do NOT silently
  refuse: stop, name the rule at stake, and work out a spec-compliant approach with
  the developer before doing the work. Use this even when the developer does not
  mention the spec, and even when the change looks small.
---

# Spec Guard

## Why this exists

Barri-Eww is a long-running, performance-sensitive project whose value depends on
the engineering spec holding over time (see `vibe-docs/spec.md`, T0[2] 长期可维护).
A single expedient exception — an abbreviation here, a runtime branch where a
compile-time switch belongs there, a direct push to `dev` — erodes the guarantees the
spec exists to protect. The spec is the contract; this skill keeps work inside it.

The goal is **not** to obstruct the developer. It is to make sure the developer's
real intent gets satisfied *through* a spec-compliant path, and that any genuine
tension between what they want and what the spec says is surfaced and resolved
deliberately — not papered over.

## What the spec is (read before deciding)

The authoritative rules live in:

- `vibe-docs/spec.md` — 总纲: **T0 key metrics** and the conflict-arbitration order
  `性能 (performance) > 兼容性 (compatibility) > 可维护性 (maintainability)`.
- `vibe-docs/spec/CodingConvention.md` (§1, C++/Java) and
  `vibe-docs/spec/CodingConventionTs.md` (§1T, TypeScript/Electron).
- `vibe-docs/spec/CommentConvention.md` (§2), `BuildSystem.md` (§3),
  `Testing.md` (§4), `VersionControl.md` (§5), `FfmBinding.md` (§6),
  `ErrorHandlingAndLogging.md` (§7), `GeneratedArtifacts.md` (§8).
- `vibe-docs/spec/ProposedExtensions.md` and `CHANGELOG.md` — the amendment ledger.

If you are unsure whether a rule applies, **read the relevant section** rather than
guessing. Do not rely on memory of the spec for anything you are about to enforce or
cite — sections change, and a stale citation is worse than none.

## The core discipline

Apply this before and during any implementation work:

1. **Know the rule.** Before writing or changing code, identify which spec sections
   govern it (naming, comments, compile-time switches, tests, git flow, FFM, etc.).

2. **Check the request against the rule.** If implementing the request *as stated*
   would satisfy every applicable rule, proceed normally — do not manufacture friction.

3. **If it would break a rule, stop.** Do not silently comply, and do not silently
   refuse. Both are failures: silent compliance breaks the contract; silent refusal
   ignores the developer's real need.

4. **Name the conflict precisely.** Tell the developer exactly which rule is at stake,
   quote or point to the spec section, and explain *why* the rule exists (tie it to the
   relevant T0 metric where possible). Concrete beats vague.

5. **Propose a spec-compliant path.** Offer one or more technical approaches that
   achieve the developer's underlying goal *without* breaking the rule. This is the
   heart of the skill — the developer usually has a legitimate need; find the compliant
   way to meet it. Lead with a recommendation, not just a menu.

6. **Discuss, then work.** Only start the implementation once you and the developer
   have agreed on a compliant approach. If they disagree with your reading, hear them
   out — you may have misread the spec or the situation.

7. **Never trade compliance for speed.** "It's just this once" / "it's a small file"
   are exactly the cases the spec is meant to catch. Small, quiet exceptions are how
   long-lived codebases rot.

## Violating the spec vs. amending the spec (important distinction)

A developer is always entitled to *change* the spec — they are not entitled to
*silently break* it. These are different actions with different paths:

- **Amending the spec** is legitimate and expected. Per `spec.md`, the project owner
  has final say on any change to fixed rules (T0 / §1–§8 / §1T). The path is:
  raise it in `vibe-docs/spec/ProposedExtensions.md` (marked `PROPOSED`) → owner
  approves → promote into the relevant section → record it in
  `vibe-docs/spec/CHANGELOG.md`, on a `docs/spec-*` branch via MR (§5).
  If the developer says "the rule itself is wrong," steer toward this path.

- **Violating the spec** — shipping code that breaks a currently-fixed rule without
  changing the rule — is what this skill blocks. If the developer wants the outcome,
  either find a compliant approach (step 5) or amend the rule first (above). Do not
  produce non-compliant work in the meantime.

When *you* (the agent) spot a useful improvement to the spec itself, you may propose
it directly into `ProposedExtensions.md`, but you may not unilaterally change a fixed
rule — surface it and let the owner decide.

## Examples

**Example 1 — naming shortcut**
Developer: "Just call the parameter `ctx`, it's obvious."
Response: Point to §1.1.2 (no abbreviations, ever) and its rationale (T0[2]
maintainability). Propose `renderContext` / `recordingContext` — pick the one that
fits the call site — and proceed once agreed. Do not write `ctx`.

**Example 2 — runtime flag where a compile-time switch belongs**
Developer: "Add a boolean so we can toggle threaded recording at runtime."
Response: Flag T0[1] (compile-time-doable work must be compile-time) and §3.2/§3.3
(feature differences via CMake option → macro, registered in the switch table).
Propose a `BARRIEWW_`-prefixed compile-time switch (register it in BuildSystem §3.3)
instead of a runtime branch. If they genuinely need runtime toggling, that's a spec
change → ProposedExtensions, not a quiet runtime `if`.

**Example 3 — merge flow**
Developer: "Just merge this feature branch straight into master."
Response: Point to §5.4 (feature branches merge only to `dev`; `master` only from
`dev`; both via MR + full CI). Propose the compliant route (→ `dev`, then `dev` →
`master`). Only `hotfix/*` may target `master`, and must back-merge to `dev`.

**Example 4 — genuinely compliant**
Developer: "Add a `recordDrawCommand` method with full doc comment and a Catch2 test."
Response: No conflict — proceed. Don't invent objections.
