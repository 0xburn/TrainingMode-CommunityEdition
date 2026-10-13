#include "coach.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 L7Fighter rh={.x=-23.263296f,.y=37.640099f},rc={.x=-22.319f,.y=42.054245f};
 L7Fighter h=rh,c=rc,old=rc;
 L7Diagnosis d=l7_diagnose_hit(177,177,21,&h,&c,&old,&rh,&rc);
 assert(d.code==0); /* Matching execution stays quiet. */
 /* Actual Castle Guest observation: same five-frame-late impact on both
  * buffer settings, before any assisted SDI or launch correction occurs. */
 c.x=-20.519136f;c.y=37.012016f;old.y=38.120464f;h.x=-19;h.y=33;
 d=l7_diagnose_hit(182,177,21,&h,&c,&old,&rh,&rc);
 assert(d.code==L7_DIAG_LATE_LOW&&d.delta==5&&d.target_dy< -5);
 old.grounded=1;
 assert(l7_diagnose_hit(182,177,21,&h,&c,&old,&rh,&rc).code==L7_DIAG_TARGET_HEIGHT);
 old.grounded=0;old.y=c.y-1; /* Lower but rising does not prove pre-hit falling. */
 assert(l7_diagnose_hit(182,177,21,&h,&c,&old,&rh,&rc).code==L7_DIAG_TARGET_HEIGHT);
 c=rc;h=rh;old=rc;h.x+=6;
 d=l7_diagnose_hit(177,177,21,&h,&c,&old,&rh,&rc);
 assert(d.code==L7_DIAG_SPACING&&d.dx>0);
 h.x=rh.x-6;assert(l7_diagnose_hit(177,177,21,&h,&c,&old,&rh,&rc).dx<0);
 rh.x=rc.x-5;h.x=c.x+5;
 assert(l7_diagnose_hit(177,177,17,&h,&c,&old,&rh,&rc).code==L7_DIAG_SIDE);
 h=rh;h.x+=100;c.x+=100; /* Whole-scene translation is not a spacing error. */
 assert(l7_diagnose_hit(177,177,17,&h,&c,&old,&rh,&rc).code==0);
 h=rh;c=rc;h.y+=6;
 assert(l7_diagnose_miss(180,177,17,&h,&c,&rh,&rc,0).code==L7_DIAG_ATTACK_HEIGHT);
 h=rh;
 assert(l7_diagnose_miss(180,177,17,&h,&c,&rh,&rc,0).code==0); /* Ambiguous whiff. */
 assert(l7_diagnose_miss(180,177,17,&h,&c,&rh,&rc,1).code==L7_DIAG_SHIELD);
 h.dead=1;assert(l7_diagnose_miss(180,177,17,&h,&c,&rh,&rc,1).code==0);
 L7Diagnosis best={0};
 assert(l7_diagnostic_offer(&best,(L7Diagnosis){.code=L7_DIAG_SPACING,.priority=50,.frame=10}));
 assert(!l7_diagnostic_offer(&best,(L7Diagnosis){.code=L7_DIAG_SPACING,.priority=50,.frame=20}));
 assert(best.frame==10); /* Stable first observation, not a flickering queue. */
 assert(l7_diagnostic_offer(&best,(L7Diagnosis){.code=L7_DIAG_SHIELD,.priority=100}));
 assert(!l7_diagnostic_offer(&best,(L7Diagnosis){.code=L7_DIAG_MISSING,.priority=20}));
 puts("diagnostics: matched hits, falling height, spacing, side, whiffs, shield, abstention and priority passed");
}
