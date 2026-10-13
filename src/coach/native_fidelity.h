#ifndef L7_NATIVE_FIDELITY_H
#define L7_NATIVE_FIDELITY_H
/* Lab's optional intangibility override must not clear action-owned nudge
 * flags while disabled. Melee sets these for ledge attacks, among other states.
 * Capture the underlying flag on enable / action change and restore it on
 * disable. Reset this bookkeeping when loading a savestate. */
typedef struct {
 int active, state, state_frame, native_nudge;
 const void *fighter;
} L7NudgeOverride;

static int l7_nudge_override(L7NudgeOverride *o, const void *fighter,
 int state, int state_frame, int enabled, int native_nudge) {
 int changed = o->fighter != fighter || o->state != state || state_frame < o->state_frame;
 if (enabled) {
  if (!o->active || changed) o->native_nudge = native_nudge;
  o->active = 1;
 } else if (o->active) {
  if (!changed) native_nudge = o->native_nudge;
  o->active = 0;
 }
 o->fighter = fighter; o->state = state; o->state_frame = state_frame;
 return enabled ? 1 : native_nudge;
}
#endif
