#ifndef GEKKO
#include <math.h>
#endif
/* Match direct hits once, in order, by move and absolute timing. A missed or
 * different move cannot borrow the following hit's response by hit ordinal. */
void l7_di_reset(L7DIState *s,const L7DIHit *hits,int count,int start){
 *s=(L7DIState){.matched=-1,.tech_matched=-1};
 while(s->cursor<count&&hits[s->cursor].frame<start)s->cursor++;
}
int l7_di_match(L7DIState *s,const L7DIHit *hits,int count,int frame,int kind,int window){
 s->matched=-1;s->tech_matched=-1;s->tech_wait_bounce=0;s->onset=frame;
 if(window<=0)return -1;
 while(s->cursor<count&&hits[s->cursor].frame<frame-window)s->cursor++;
 int best=-1,distance=window+1;
 for(int i=s->cursor;i<count&&hits[i].frame<=frame+window;i++){
  int delta=absolute(hits[i].frame-frame);
  if(hits[i].kind==kind&&delta<distance){best=i;distance=delta;}
 }
 if(best>=0){
  s->matched=best;s->cursor=best+1;
  /* A zero timing delta does not imply identical spacing or landing timing. */
  if(hits[best].tech_frame>0){
   s->tech_matched=best;s->tech_until=frame+hits[best].tech_frame-hits[best].frame+window;
   s->tech_wait_bounce=hits[best].tech_bounce?1:0;
  }
 }
 return best;
}
int l7_di_sample(L7DIState *s,const L7DIHit *hits,int count,int frame,float remaining){
 if(remaining<=0){s->matched=-1;return 0;}
 if(s->matched<0||s->matched>=count||frame<=s->onset)return 0;
 const L7DIHit *hit=&hits[s->matched];
 int sample=hit->frame+frame-s->onset;
 /* DI is consumed on hitlag exit using the previous input sample. Pin the
  * last live hitlag input to the reference's last input even if durations differ. */
 if(remaining<=1||sample>hit->last_input)sample=hit->last_input;
 return sample;
}

/* SDI steps are consumed once per corresponding hitlag frame. An extended live
 * hitlag must not repeat the reference's last SDI step (unlike held DI input). */
int l7_sdi_sample(const L7DIState *s,const L7DIHit *hits,int count,int frame){
 if(s->matched<0||s->matched>=count||frame<s->onset)return -1;
 const L7DIHit *hit=&hits[s->matched];
 int sample=hit->frame+frame-s->onset;
 return sample<=hit->last_input?sample:0;
}

/* Reproduce the recorded world-space launch direction using only an angle
 * attainable by DI from this hit's live incoming knockback. Native hitlag exit
 * has already handled ASDI/collision and knockback magnitude. Preserve that
 * magnitude, and leave an already-correct result bit-for-bit unchanged. Copying
 * stick coordinates alone cannot do this when residual knockback or hit side
 * changes the incoming angle. This is explicitly an assisted outcome. */
int l7_di_launch(float incoming_x,float incoming_y,float reference_x,float reference_y,
                 float max_degrees,float *x,float *y){
 const float pi=3.14159265358979323846f;
 float incoming_sq=incoming_x*incoming_x+incoming_y*incoming_y;
 float reference_sq=reference_x*reference_x+reference_y*reference_y;
 float live_sq=*x**x+*y**y;
 if(incoming_sq<.00001f||reference_sq<.00001f||live_sq<.00001f||max_degrees<=0)return 0;
 float incoming=atan2(incoming_y,incoming_x);
 float delta=atan2(reference_y,reference_x)-incoming;
 if(delta>pi)delta-=2*pi;
 if(delta< -pi)delta+=2*pi;
 float limit=max_degrees*(pi/180.f);
 if(delta>limit)delta=limit;
 if(delta< -limit)delta=-limit;
 float angle=incoming+delta;
 float error=angle-atan2(*y,*x);
 if(error>pi)error-=2*pi;
 if(error< -pi)error+=2*pi;
 if(error>-.00001f&&error<.00001f)return 0;
 float magnitude=sqrtf(live_sq);
 *x=magnitude*cos(angle);*y=magnitude*sin(angle);
 return 1;
}

/* NTSC common states: missed ground tech (183/191), then the six passive states. */
int l7_tech_outcome(int state){return state==183||state==191||(state>=199&&state<=204);}
static int l7_tech_flying(int state){return (state>=75&&state<=91)||state==38;}
/* Called once at a matched impact, before hitlag DI. A downed fighter may
 * still be internally airborne at the same floor contact. Give that narrow
 * case the native grounded-spike vertical launch, retaining live horizontal
 * knockback and position. Other air hits retain the defensive choice without
 * waiting for a ground rebound that their trajectory cannot produce. */
int l7_tech_prepare(L7DIState *s,const L7DIHit *hits,int count,const L7Fighter *before,
                    int at_floor,float *velocity_y,float bounce_decay){
 int i=s->tech_matched;
 if(i<0||i>=count||!hits[i].tech_bounce||before->grounded)return 0;
 if((before->state==183||before->state==191)&&at_floor&&*velocity_y<0&&bounce_decay>0){
  *velocity_y=-*velocity_y*bounce_decay;
  return 1;
 }
 s->tech_wait_bounce=0;
 return 0;
}
void l7_tech_reference(L7DIHit *hit,const L7Fighter *first,int stride,int count){
 hit->tech_frame=hit->tech_state=hit->tech_x=hit->tech_bounce=0;
 const L7Fighter *impact=(const L7Fighter*)((const char*)first+hit->frame*stride);
 const L7Fighter *before=hit->frame>0?(const L7Fighter*)((const char*)impact-stride):impact;
 int from_floor=before->grounded,rose=0;
 for(int f=hit->frame+1;f<count;f++){
  const L7Fighter *now=(const L7Fighter*)((const char*)first+f*stride);
  const L7Fighter *old=(const L7Fighter*)((const char*)first+(f-1)*stride);
  /* Any intervening damage, including projectiles, belongs to another response. */
  if(now->dead||now->percent>old->percent+.01f||now->percent+.01f<old->percent)return;
  if(l7_tech_outcome(now->state)){
   hit->tech_frame=f;hit->tech_state=now->state;
   /* A grounded spike can rebound before its eventual ground tech. Enabling
    * that tech at the first surface contact would erase the rebound. */
   hit->tech_bounce=from_floor&&rose&&now->state>=199&&now->state<=201;
   if(now->state==200||now->state==201)
    hit->tech_x=(now->facing<0?-80:80)*(now->state==200?1:-1);
   return;
  }
  if(!now->grounded&&now->y>impact->y+.5f)rose=1;
  if(!l7_tech_flying(now->state))return;
 }
}
int l7_tech_response(L7DIState *s,const L7DIHit *hits,int count,int frame,const L7Fighter *cpu){
 int index=s->tech_matched;
 if(index<0||index>=count)return -1;
 if(!hits[index].tech_frame){s->tech_matched=-1;return -1;}
 if(cpu->dead||frame>s->tech_until||l7_tech_outcome(cpu->state)||!l7_tech_flying(cpu->state)){
  s->tech_matched=-1;return -1;
 }
 if(s->tech_wait_bounce==1){s->tech_start_y=cpu->y;s->tech_wait_bounce=2;}
 if(s->tech_wait_bounce&&!cpu->grounded&&cpu->y>s->tech_start_y+.5f)s->tech_wait_bounce=0;
 if(frame<=s->onset||cpu->hitlag)return -1;
 return index;
}
