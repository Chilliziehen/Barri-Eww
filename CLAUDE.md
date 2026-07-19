# Barri-Eww — Project Instructions

> Auto-loaded into every session. These instructions apply to **all agents** working
> in this repository (the main agent and any subagents it spawns).

## Spec compliance is mandatory (always on)

This project is governed by the engineering spec in **`vibe-docs/spec.md`** (总纲:
T0 key metrics) and **`vibe-docs/spec/*.md`** (§1–§8, §1T). All work — code, comments,
build system, CI, tests, git workflow — must comply.

**The rule:** if a developer request, or a developer's specific need, would break a
spec rule when implemented as stated, do **not** silently comply and do **not**
silently refuse. Stop, name the specific rule and why it exists, propose a
spec-compliant technical approach that meets the developer's underlying goal, and only
proceed once you've agreed on it. See the **`spec-guard`** skill
(`.claude/skills/spec-guard/SKILL.md`) for the full procedure and examples — consult it
before writing/modifying code, choosing an approach, or touching build/CI/tests/git.

**Changing vs. breaking the spec:** the developer may always *change* the spec (owner
has final say; path = `vibe-docs/spec/ProposedExtensions.md` → approval → promote →
`CHANGELOG.md`, on a `docs/spec-*` branch). What is forbidden is *silently breaking* a
currently-fixed rule. Steer "the rule is wrong" toward the amendment path.

**Don't rely on memory of the spec.** Read the relevant section before enforcing or
citing it — sections evolve, and the files are the source of truth.

## Quick spec pointers (read the file before acting on it)

- **T0**: performance > compatibility > maintainability. Compile-time-doable work MUST
  be done at compile time.
- **§1 / §1T**: PascalCase files/classes; `m_`/`s_`/`g_` prefixes; full camelCase
  params (identical in decl & impl); **no abbreviations ever**; one class per file.
- **§2**: `/** */` doc comments only; **comments in English**; `@param` with explicit
  type; `@note ThreadSafety` (first line) and `@warning MemoryOwnership` mandatory
  across the Java↔Native boundary.
- **§3**: CMake 4.4 (C++23) / Gradle / npm+Vite+electron-builder (TypeScript); no
  build system at repo root (only `build.sh`/`build.bat`); compile-time switches via
  CMake→macro, prefix `BARRIEWW_`, registered in BuildSystem §3.3.
- **§4**: unit tests mandatory (Catch2 / JUnit 5 / Vitest); coverage gates C++/Java
  ≥90%, TS ≥70%.
- **§5**: branches `feature|fix|hotfix|chore|docs|test|refactor/<work>`; feature
  branches merge only to `dev`; `master` only from `dev`; both via MR + full CI;
  commit body needs 改动点/影响范围/相关文件/改动思路/功能性变化/非功能性变化.
- **§6/§7/§8**: FFM binding (compile-time downcall handles, trivial/critical marking,
  memory ownership), error handling + compile-time logging switches, generated
  artifacts & ClassLoader isolation.

## Note for spawned subagents

When you spawn a subagent for work in this repo, tell it to honor this file and the
`spec-guard` skill (include the mandate in the spawn prompt). If custom agents are
later defined under `.claude/agents/`, add the same spec-compliance mandate to each.
