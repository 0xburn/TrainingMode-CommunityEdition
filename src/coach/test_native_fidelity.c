#include <assert.h>
#include "native_fidelity.h"
int main(void) {
 L7NudgeOverride o={0}; int fighter=0;
 /* Disabled Lab option preserves both native flag values. */
 assert(l7_nudge_override(&o,&fighter,259,28,0,1)==1);
 assert(l7_nudge_override(&o,&fighter,14,1,0,0)==0);
 /* Enable, hold, disable: restore the original value, not the override. */
 assert(l7_nudge_override(&o,&fighter,14,2,1,0)==1);
 assert(l7_nudge_override(&o,&fighter,14,3,1,1)==1);
 assert(l7_nudge_override(&o,&fighter,14,4,0,1)==0);
 /* An action entered while enabled supplies its own native flag. */
 assert(l7_nudge_override(&o,&fighter,14,5,1,0)==1);
 assert(l7_nudge_override(&o,&fighter,259,1,1,1)==1);
 assert(l7_nudge_override(&o,&fighter,259,2,0,1)==1);
 /* A new action on the disable frame must keep its native value. */
 assert(l7_nudge_override(&o,&fighter,259,3,1,1)==1);
 assert(l7_nudge_override(&o,&fighter,14,1,0,0)==0);
 /* Consecutive instances of the same action are independent. */
 assert(l7_nudge_override(&o,&fighter,259,4,1,0)==1);
 assert(l7_nudge_override(&o,&fighter,259,1,1,1)==1);
 assert(l7_nudge_override(&o,&fighter,259,2,0,1)==1);
 return 0;
}
