# Kirby Air Riders Roadmap

Updated 23 November 2026.

This roadmap orders the work as small, reviewable milestones, with clear evidence for when each one is done. The current
checkout still has Mario Kart World assumptions in its build target, version tables, patches, and documentation. Treat
those as inherited scaffolding; carry pieces forward only after verifying they apply to Kirby Air Riders. The tracked
project docs do not yet establish KAR's executable version, module layout, graphics interface, or startup requirements.

## How to work through it

- **P0** establishes the target facts and removes inherited defaults. **P1** reaches the first playable local slice.
  **P2** broadens support and prepares a release. Follow the order below; it reflects dependencies, not calendar dates.
- Keep each change to one observable outcome, with the affected KAR version and a reproducer or validation step.
- Prefer a complete vertical slice over broad placeholder support: identify a need from a trace or comparison, implement
  that need, then verify the same path.
- Store generated game-dependent output under `build/`. Do not commit game files, assets, generated code or shaders, or
  game screenshots. Work only with already-decrypted local files; do not add key handling or decryption. Do not implement
  Nintendo online services. Reused code must have a compatible license and a source citation.
- Before any game run, state the diagnostic purpose and expected outcomes. Get approval unless that run was already
  authorized.

## P0 — Establish the KAR baseline

These tasks remove uncertainty that would otherwise make every later change guesswork.

- [ ] **Record the first supported target version.** Inspect the available local game files and record the version,
  required input layout, module names, and any version-specific differences as text. Do not infer these from MKW's
  version tables. Fail clearly when the input is incomplete or unsupported.
- [ ] **Separate KAR identity from inherited MKW configuration.** Add a KAR-specific target profile for version selection,
  patch data, and generated-output paths. Keep shared runtime code behind game-neutral interfaces where practical. A KAR
  build must never silently select an MKW patch table or generated directory.
- [ ] **Make the host target identify itself as KAR.** Update executable naming, launcher identity, logs, settings paths,
  and package names together. Preserve an explicit migration path for user settings if the storage location changes.

**Done when:** a clean checkout identifies the selected KAR target version, rejects unsupported input with an actionable
message, and keeps all generated output under `build/` without using MKW defaults.

## P1 — Reproducible setup and translation

- [ ] **Prove one end-to-end generation path.** Run the existing translation approach against one verified KAR target,
  compile the generated output, and keep all game-dependent output out of version control.
- [ ] **Make setup failures diagnosable.** Report which input, module, translation step, or compiler failed, and retain
  enough local logs to reproduce the failure without exposing game content.
- [ ] **Add small regression checks for the target profile.** Cover version selection, missing files, unsupported
  versions, and output-directory boundaries using fixtures that contain no game data.

**Done when:** the same supported input and toolchain produce a successful host build on a clean setup, and the profile
checks pass without requiring game content in the test suite.

## P1 — Boot to a stable first screen

- [ ] **Trace startup from the host entry point to the first screen.** Record the first missing services and guest calls
  from logs or the executable database; implement only the behavior startup actually reaches.
- [ ] **Bring up the required kernel, memory, filesystem, and SDK paths.** Keep implementations narrow and add focused
  tests for boundaries, errors, and repeated initialization.
- [ ] **Make startup repeatable.** Verify cold launch, clean exit, and relaunch on the first supported target version.

**Done when:** repeated launches reach the same first in-game screen without a crash, hang, or unresolved startup call,
and a failure log points to the relevant module or service.

## P1 — Render the first screen accurately

- [ ] **Confirm KAR's graphics and shader formats from evidence.** Document the formats and runtime interface seen in the
  target before adapting the inherited graphics backend.
- [ ] **Render a captured reference path.** Start with the first screen and one menu transition; fix discrepancies at the
  responsible translation or runtime layer.
- [ ] **Build a reusable visual regression path.** Keep captures and generated graphics artifacts local, and make the
  comparison repeatable from a clean build.

**Done when:** the first screen and selected menu transition match the reference closely enough to identify no known
blocking visual errors, and the capture can be replayed to check future graphics changes.

## P1 — Complete the essential player I/O paths

- [ ] **Controller input:** detect a connected controller, navigate the verified menu path, and test disconnect/reconnect.
- [ ] **Audio:** reach a stable audible output path; cover startup, sustained playback, and clean shutdown.
- [ ] **Filesystem and settings:** load required game files and demonstrate a save/settings round trip without corrupting
  unrelated files.

**Done when:** a player can use the intended input device to navigate the first supported path, hear stable audio where
expected, and retain supported settings across a relaunch.

## P1 — Deliver one playable local slice

- [ ] **Choose the first mode from the verified game menus.** Record its entry path and the minimum guest services it uses.
- [ ] **Complete one full play loop.** Boot, navigate, enter the mode, finish a round, and return to a menu using supported
  controls.
- [ ] **Add a mode support matrix.** Track tested entry paths and known failures; add more modes one at a time.

**Done when:** the first documented local mode completes repeatedly on the supported target version, with input,
rendering, audio, and return-to-menu behavior all checked.

## P2 — Stability, compatibility, and performance

- [ ] **Fix hangs and crashes before tuning speed.** Reproduce each failure with the smallest useful trace or capture and
  add a regression check where possible.
- [ ] **Build a compatibility matrix.** Record tested operating systems, GPUs, drivers, controllers, and game versions;
  distinguish tested combinations from expected support.
- [ ] **Measure representative gameplay before optimizing.** Establish repeatable frame-time and load-time baselines,
  identify the critical path, and make one measured change at a time.
- [ ] **Check graphics correctness across the matrix.** Use validation output and final-frame comparisons on supported
  configurations.

**Done when:** the documented supported configurations complete the playable slice without known hangs or correctness
regressions, and each performance claim has a reproducible baseline.

## P2 — Package for players and contributors

- [ ] **Make setup understandable.** Explain the accepted local input format, required free space, supported version,
  and where generated files are stored.
- [ ] **Produce clean platform packages.** Verify install, launch, update, and uninstall behavior without bundling any
  game content.
- [ ] **Document support and reporting.** Keep known issues, tested configurations, and useful log locations current;
  ensure reports do not ask users to upload game files.

**Done when:** a new user can set up their own supported local files, launch the verified playable slice, and produce a
useful bug report from a packaged build.
