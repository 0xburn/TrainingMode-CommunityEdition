#include "coach.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void){
 /* Castle Guest: opposite-side shine retains prior horizontal knockback.
  * The same stick would launch near 84 degrees instead of the reference 75. */
 float x=.437098f,y=3.999934f,energy=x*x+y*y;
 assert(l7_di_launch(-.042823f,4.023518f,1.039273f,3.909915f,18,&x,&y));
 assert(fabsf(atan2f(y,x)-atan2f(3.909915f,1.039273f))<.00001f);
 assert(fabsf(x*x+y*y-energy)<.00001f);
 float same_x=x,same_y=y;
 assert(!l7_di_launch(-.042823f,4.023518f,1.039273f,3.909915f,18,&x,&y));
 assert(x==same_x&&y==same_y);
 /* Opposite-side launches cannot be reversed by assistance: clamp to DI's
  * actual range, including both signs and angles crossing +/- pi. */
 for(int sign=-1;sign<=1;sign+=2){
  x=4;y=0;
  assert(l7_di_launch(4,0,-4,sign*.1f,18,&x,&y));
  assert(fabsf(atan2f(y,x)-sign*18*.01745329252f)<.00001f);
  assert(fabsf(x*x+y*y-16)<.00001f);
  x=-4;y=sign*.1f;
  assert(l7_di_launch(-4,sign*.1f,-4,-sign*.1f,18,&x,&y));
  assert(fabsf(x+4)<.00001f&&fabsf(y+sign*.1f)<.00001f);
 }
 x=0;y=0;assert(!l7_di_launch(0,0,1,1,18,&x,&y));
 x=1;y=2;assert(!l7_di_launch(1,2,0,0,18,&x,&y));
 assert(!l7_di_launch(1,2,2,1,0,&x,&y));assert(x==1&&y==2);
 L7DIHit hits[]={{.frame=100,.last_input=105,.kind=1},{.frame=120,.last_input=126,.kind=2},{.frame=140,.last_input=143,.kind=1},{.frame=144,.last_input=147,.kind=1}};
 L7DIState s;
 for(int delta=-3;delta<=3;delta++){
  l7_di_reset(&s,hits,4,100);
  assert(l7_di_match(&s,hits,4,100+delta,1,3)==0);
  assert(l7_sdi_sample(&s,hits,4,99+delta)==-1);
  assert(l7_sdi_sample(&s,hits,4,100+delta)==100);
  assert(l7_sdi_sample(&s,hits,4,101+delta)==101);
  assert(l7_sdi_sample(&s,hits,4,105+delta)==105);
  assert(l7_sdi_sample(&s,hits,4,106+delta)==0); /* No repeated SDI at longer hitlag. */
  assert(l7_di_sample(&s,hits,4,101+delta,5)==101);
  assert(l7_di_sample(&s,hits,4,103+delta,1)==105);
  assert(l7_di_sample(&s,hits,4,104+delta,0)==0);
  assert(l7_sdi_sample(&s,hits,4,104+delta)==-1);
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
 /* Each tech choice is attached to the hit that actually caused it. */
 L7Fighter reference[32]={0};
 L7DIHit tech_hit={.frame=2,.last_input=6,.kind=1};
 for(int f=0;f<32;f++)reference[f]=(L7Fighter){.state=87,.percent=10,.facing=1};
 int outcomes[]={183,191,199,200,201,202,203,204};
 for(int o=0;o<8;o++){
  reference[14].state=outcomes[o];
  l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);
  assert(tech_hit.tech_frame==14&&tech_hit.tech_state==outcomes[o]);
  assert(tech_hit.tech_x==(outcomes[o]==200?80:outcomes[o]==201?-80:0));
  for(int delta=-2;delta<=3;delta++){
   l7_di_reset(&s,&tech_hit,1,0);
   assert(l7_di_match(&s,&tech_hit,1,2+delta,1,3)==0);
   L7Fighter live={.state=87,.hitlag=1};
   assert(l7_tech_response(&s,&tech_hit,1,4+delta,&live)==-1);
   l7_di_sample(&s,&tech_hit,1,7+delta,0); /* Ending DI must retain tech ownership. */
   live.hitlag=0;
   assert(l7_tech_response(&s,&tech_hit,1,7+delta,&live)==0);
   assert(l7_tech_response(&s,&tech_hit,1,17+delta,&live)==0);
   assert(l7_tech_response(&s,&tech_hit,1,18+delta,&live)==-1);
  }
  l7_di_reset(&s,&tech_hit,1,0);l7_di_match(&s,&tech_hit,1,3,1,3);
  L7Fighter live={.state=outcomes[o]};
  assert(l7_tech_response(&s,&tech_hit,1,15,&live)==-1&&s.tech_matched==-1);
 }
 reference[14].state=200;reference[14].facing=-1;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(tech_hit.tech_x==-80);
 reference[10].percent=11;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(!tech_hit.tech_frame);
 reference[10].percent=10;reference[10].state=14;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(!tech_hit.tech_frame);
 reference[10].state=87;reference[10].dead=1;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(!tech_hit.tech_frame);
 reference[10].dead=0;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);
 l7_di_reset(&s,&tech_hit,1,0);l7_di_match(&s,&tech_hit,1,3,1,3);
 assert(s.tech_matched==0);
 assert(l7_di_match(&s,&tech_hit,1,4,2,3)==-1&&s.tech_matched==-1); /* Interrupting wrong move. */
 l7_di_reset(&s,&tech_hit,1,0);assert(s.tech_matched==-1);
 L7DIHit wide={.frame=100,.last_input=106,.kind=17,.tech_frame=126,.tech_state=199};
 for(int delta=-14;delta<=14;delta++){
  l7_di_reset(&s,&wide,1,0);
  assert(l7_di_match(&s,&wide,1,100+delta,17,14)==0);
  L7Fighter live={.state=87};
  assert(l7_tech_response(&s,&wide,1,107+delta,&live)==0);
 }
 l7_di_reset(&s,&wide,1,0);assert(l7_di_match(&s,&wide,1,114,17,13)==-1&&s.tech_matched==-1);
 /* Preserve the launch before arming a recorded ground tech. This is a
  * geometric phase, not a fixed delay or the size of the timing window. */
 for(int f=0;f<32;f++)reference[f]=(L7Fighter){.state=89,.percent=10,.y=27,.facing=1};
 reference[1]=(L7Fighter){.state=183,.grounded=1,.percent=0,.y=27};
 reference[8].y=30;reference[14].state=199;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);
 assert(tech_hit.tech_bounce&&tech_hit.tech_state==199);

 L7Fighter airborne_down={.state=183,.grounded=0,.y=27};
 /* Only a matched downward launch of a downed fighter at a live floor is
  * normalized. Grounded / rising / distant / ordinary aerial hits are intact. */
 for(int state=0;state<4;state++)for(int ground=0;ground<2;ground++)for(int floor=0;floor<2;floor++)for(int rising=0;rising<2;rising++){
  int states[]={183,191,184,89};
  L7Fighter before={.state=states[state],.grounded=ground};
  l7_di_reset(&s,&tech_hit,1,0);l7_di_match(&s,&tech_hit,1,2,1,14);
  float vy=rising?2.5f:-2.5f;
  int wanted=state<2&&!ground&&floor&&!rising;
  assert(l7_tech_prepare(&s,&tech_hit,1,&before,floor,&vy,.8f)==wanted);
  assert(vy==(wanted?2.f:rising?2.5f:-2.5f));
  assert(s.tech_matched==0); /* Never drop the recorded defensive choice. */
  assert(!!s.tech_wait_bounce==!!(ground||wanted));
 }
 l7_di_reset(&s,&tech_hit,1,0);assert(l7_di_match(&s,&tech_hit,1,2,1,0)==-1);
 float disabled_vy=-2.5f;
 assert(!l7_tech_prepare(&s,&tech_hit,1,&airborne_down,1,&disabled_vy,.8f)&&disabled_vy==-2.5f);
 for(int window=0;window<=14;window+=14){
  l7_di_reset(&s,&tech_hit,1,0);l7_di_match(&s,&tech_hit,1,100,1,window);
  float vy=-2.5f;
  assert(!l7_tech_prepare(&s,&tech_hit,1,&airborne_down,1,&vy,.8f)&&vy==-2.5f);
 }
 for(int delta=-14;delta<=14;delta++){
  l7_di_reset(&s,&tech_hit,1,0);assert(l7_di_match(&s,&tech_hit,1,2+delta,1,14)==0);
  L7Fighter live={.state=89,.hitlag=1,.y=40};
  assert(l7_tech_response(&s,&tech_hit,1,3+delta,&live)==-1&&s.tech_wait_bounce);
  live.hitlag=0;
  assert(l7_tech_response(&s,&tech_hit,1,7+delta,&live)==0&&s.tech_wait_bounce);
  live.y=39; /* Still at/below the impact surface: never enable instant tech. */
  assert(l7_tech_response(&s,&tech_hit,1,8+delta,&live)==0&&s.tech_wait_bounce);
  live.y=42;
  assert(l7_tech_response(&s,&tech_hit,1,9+delta,&live)==0&&!s.tech_wait_bounce);
  live.y=39; /* Falling back to the surface retains the final tech option. */
  assert(l7_tech_response(&s,&tech_hit,1,13+delta,&live)==0&&!s.tech_wait_bounce);
 }
 reference[1].grounded=0;reference[1].state=89;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(!tech_hit.tech_bounce);

 reference[1].grounded=1;reference[8].y=27;
 l7_tech_reference(&tech_hit,reference,sizeof(*reference),32);assert(!tech_hit.tech_bounce);
 puts("DI buffer: boundaries, early/late, wrong/skipped/repeated hits, hitlag exit and reset passed");
 puts("Tech buffer: all choices, facing, early/late, exact timing, interruption, timeout and reset passed");
}
