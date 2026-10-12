#include "coach.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 L7DIHit hits[]={{100,105,1},{120,126,2},{140,143,1},{144,147,1}};
 L7DIState s;
 for(int delta=-3;delta<=3;delta++){
  l7_di_reset(&s,hits,4,100);
  assert(l7_di_match(&s,hits,4,100+delta,1,3)==0);
  assert(l7_di_sample(&s,hits,4,101+delta,5)==101);
  assert(l7_di_sample(&s,hits,4,103+delta,1)==105);
  assert(l7_di_sample(&s,hits,4,104+delta,0)==0);
 }
 l7_di_reset(&s,hits,4,100);assert(l7_di_match(&s,hits,4,100,1,0)==-1);
 assert(l7_di_match(&s,hits,4,96,1,3)==-1);
 assert(l7_di_match(&s,hits,4,104,1,3)==-1);
 assert(l7_di_match(&s,hits,4,120,1,3)==-1); /* Wrong move. */
 assert(l7_di_match(&s,hits,4,120,2,3)==1);
 assert(l7_di_match(&s,hits,4,121,2,3)==-1); /* Cannot reuse. */
 assert(l7_di_match(&s,hits,4,143,1,4)==3); /* Closest eligible hit. */
 l7_di_reset(&s,hits,4,120);assert(s.cursor==1&&s.matched==-1);
 assert(l7_di_match(&s,hits,4,120,2,3)==1);
 assert(l7_di_sample(&s,hits,4,130,4)==126); /* Clamp extended hitlag. */
 l7_di_reset(&s,hits,4,100);assert(s.cursor==0&&s.matched==-1);
 assert(l7_di_match(&s,hits,4,140,1,3)==2); /* Skipped hits expire. */
 assert(l7_di_sample(&s,hits,4,140,4)==0); /* No pre-impact override. */
 puts("DI buffer: boundaries, early/late, wrong/skipped/repeated hits, hitlag exit and reset passed");
}
