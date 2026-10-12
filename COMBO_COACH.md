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
the recorded sticks during hitlag, then resumes the original input timeline.
It defaults to zero and does not repair missed hits or positioning errors.

## Build

Follow the upstream toolchain instructions in `DEVELOPMENT.md`. The normal build
includes coaching and preserves normal boot/menu navigation:

```sh
./build.sh /path/to/your/melee.iso release
```

An optional build enters Training Lab on startup and imports a specially named
file from memory card A:

```sh
L7_AUTOLOAD=1 ./build.sh /path/to/your/melee.iso release
```

The auto-load hooks are compiled only with `L7_AUTOLOAD`. A host launcher selects
the desired `.gci`, copies it to Dolphin's GCI-folder card A, and sets the copy's
32-byte internal filename field at offsets 8–39 to ASCII `TMREC_L7AUTOLOAD`, padded
with zeroes. It leaves the source unchanged. The hooks use the native event menu
handler and Training Lab's existing import path to select characters/stage and
load the recording. They do not search macOS folders or automate desktop input.
Missing imports fall back to character select. Remove the staged copy on exit.

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
  src/coach/coach.c src/coach/test_coach.c -o /tmp/coach-test
/tmp/coach-test
cc -std=c99 -Wall -Wextra -fsanitize=address,undefined \
  src/coach/coach.c src/coach/test_di_buffer.c -o /tmp/coach-di-test
/tmp/coach-di-test
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
