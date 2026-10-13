# Combo Coach (experimental)

This branch adds native combo practice to Training Lab. It is based on the
tested CE-v1.4 source commit `6aecb9cd53a70dd59a06ae4a3b3175f62a03238c`.
The PR targets the fork's `coach/ce-v1.4-base` branch, not current upstream master.

Import a compatible `.gci` recording in Training Lab and let its demonstration
finish once. The coach captures reference inputs, direct hits, and movement.

- D-pad Down: choose the closest direct combo hit for takeover.
- D-pad Left: retry from the countdown.
- D-pad Up: return to watching.
- D-pad Right: loop a preview around the best attempt (two hits before/after).

`Combo Coach Settings`, above Help, controls assistance and presentation.
Short is the default countdown. Technique names describe the first three actions
before the countdown; preview mode advances that queue through its clip. Best and
latest direct-hit completion are shown in the HUD. Projectiles do not count.
The optional opponent DI buffer matches direct hits by attack and timing, uses
the recorded sticks during hitlag, reproduces the observed native SDI steps
(including UCF decisions) at hit-relative frames, and targets the recorded launch
angle within the live hit's legal DI range. The native hitlag-exit callback handles
ASDI and collision; assistance rotates the resulting velocity to the attainable
reference angle while preserving live speed. Identical timing/physics is unchanged.
This compensates for hit-side and residual-knockback differences that copying
stick coordinates alone cannot resolve. The buffer also preserves the recorded tech response for
matched hits, including exact-frame matches. This includes missed tech, tech in place and world-direction
rolls, plus wall/ceiling tech choices. A recorded ground bounce is preserved before
enabling the subsequent ground tech. For an early downed rehit at a live floor
surface, it corrects the downward vertical launch using Melee's grounded-spike
bounce coefficient before DI. That bounce adjustment retains horizontal knockback
before the launch-angle assist. Other airborne
hits retain the tech choice without waiting for a ground bounce. This physics
assist shares the DI buffer setting; zero disables both. CE's normal collision handling still decides
whether the opponent reaches a surface. Response assistance ends on the tech,
interruption, recovery, death, reset, or its bounded deadline. Matching the hit frame alone does not guarantee the same landing timing. The setting defaults to zero and does not repair
missed hits or positioning errors. Ground-tech responses are exercised in the
macOS fixture; wall/ceiling choices have portable policy coverage only.

## Advanced diagnostics

Enabled by default in Combo Coach Settings. The coach shows one measured finding
and one suggested adjustment when it has evidence beyond an early/late input:
late hits on a falling lower target, different target height or hit side,
horizontal spacing, active attacks passing too high/low, confirmed shield contact,
missed L-cancels, and missing expected attacks. Advice remains after a drop and
clears on retry/watch; completed combos hide it. Turning the setting off restores
the existing timing display. It does not change fighter state, inputs or assistance.

Matching uses unused direct hits of the same move within 30 frames, independently
of the DI buffer. Spatial comparisons are relative to the opponent at each hit,
not screenshots at different elapsed times. Near-overlapping sides and ambiguous
whiffs do not receive invented explanations. Shield detection uses CE's existing
frame-advantage contact record because Melee clears its temporary shield source
before the post-frame observer runs.

The rules offer practice suggestions, not counterfactual proof that one change
will reproduce the combo. No LLM or network is required. Developer logs retain the
live/reference frame, move, timing and spatial deltas; Lucky Stats' local session
report groups repeated findings across attempts. Native acceptance verifies quiet
perfect execution and unchanged trajectories with diagnostics on/off, late lower
targets with DI assistance on/off, missing attacks, missed L-cancels, shield contact
and reset clearing. Other spatial classifications have portable policy tests.

## Build

Follow the upstream toolchain instructions in `DEVELOPMENT.md`. The standard
build supports both automatic folder imports and traditional manual Import:

```sh
./build.sh /path/to/your/melee.iso release
```

Configure Dolphin Slot A as a GCI folder, choose a familiar folder ending in
`USA`, and save a new Lucky Stats practice export there. On startup, CE loads the
newest recognized export by its embedded millisecond export time. Holding B
through startup bypasses automatic loading. With no eligible file, normal menus
remain available; later visits to character select never auto-import again.

See [the user guide](COMBO_COACH_IMPORT.md) for Windows/macOS setup and both import
flows. Maintainers can compile out the boot hooks with
`L7_AUTOLOAD=0 ./build.sh /path/to/your/melee.iso release`.

The existing development launcher remains compatible: its explicitly staged
`TMREC_L7AUTOLOAD` copy takes priority over timestamped folder exports. The native
hooks use the event menu handler and Training Lab's normal import path. No desktop
input automation or runtime debugger is needed for folder imports.

This is source for a development build, not a packaged launcher or game download.
No ISO or tournament recording is included. Module builds have been checked with
the pinned Lucky Stats macOS/PowerPC toolchain; full upstream ISO packaging and
hardware compatibility need separate validation.

## Source and verification

`src/coach/` contains the portable timing/hit-selection core, native adapters,
HUD, and optional auto-load hooks. `config.generated.h` contains the tested
compile defaults; in-game settings can change them during a session.
`icons.generated.h` contains unchanged controller artwork; see `SOURCES.txt` for
attribution. The generated headers were exported from the Lucky Stats coaching
build. The native game uses no Lucky Stats API or debugger service.

The in-game text heap fix is backported from upstream commit
`67b77a6dc74a823e346e444a472d6908a58e5efd`. It is required to avoid HUD text-memory
exhaustion. Auto-load addresses are NTSC-specific and verified against the CE
assembly patches, including the event-control procedure's priority.

```sh
cc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
  src/coach/coach.c src/coach/test_coach.c -o /tmp/coach-test -lm
/tmp/coach-test
cc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
  src/coach/coach.c src/coach/test_di_buffer.c -o /tmp/coach-di-test -lm
/tmp/coach-di-test
cc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
  src/coach/coach.c src/coach/test_diagnostics.c -lm -o /tmp/coach-diagnostics-test
/tmp/coach-diagnostics-test
```

Prior local Dolphin acceptance on the selected Falco/Fox Battlefield fixture
compared all 905 frames under perfect input: 11/11 direct hits with DI assistance
off and on, with identical fighter state. Delaying the first aerial sequence by
three frames reached 3/11 hits without assistance and 10/11 with a three-frame DI
window. Other tests cover early, outside-window, missing and buffered inputs,
countdown/reset/preview controls, and native auto-import. These are fixture-level
results, not a claim of exact continuation for every game, stage or character.

Files exported by Lucky Stats can carry `L7COACH1` metadata in unused recording
space: physical player order, requested frame and per-frame RNG seeds. This fork
uses it to preserve those details; ordinary compatible TM-CE recordings can still
be imported. Older-format conversion may reconstruct stale-move history and has
additional fidelity limits. The browser converter and host launcher remain in
the separate Lucky Stats repository.

## Native collision fidelity

Training Lab preserves Melee's action-owned nudge flag when CPU intangibility is
off, including while Combo Coach is disabled. Melee uses this flag during ledge
attacks; clearing it unconditionally changed body collisions and could derail
recorded practice sequences. The optional override captures/restores the native
value and clears its bookkeeping on save loads. This does not force positions.

Unit check: `cc -Wall -Wextra -Werror src/coach/test_native_fidelity.c -o /tmp/ce-fidelity && /tmp/ce-fidelity`.

The same fix passed native continuation checks in the Lucky Stats test build.
Converter corrections are separate: existing exports need regeneration using
the updated Lucky Stats converter.
