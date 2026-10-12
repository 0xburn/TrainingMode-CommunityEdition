/* Match direct hits once, in order, by move and absolute timing. A missed or
 * different move cannot borrow the following hit's response by hit ordinal. */
void l7_di_reset(L7DIState *s,const L7DIHit *hits,int count,int start){
 *s=(L7DIState){.matched=-1,.tech_matched=-1};
 while(s->cursor<count&&hits[s->cursor].frame<start)s->cursor++;
}
int l7_di_match(L7DIState *s,const L7DIHit *hits,int count,int frame,int kind,int window){
 s->matched=-1;s->tech_matched=-1;s->onset=frame;
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

/* NTSC common states: missed ground tech (183/191), then the six passive states. */
int l7_tech_outcome(int state){return state==183||state==191||(state>=199&&state<=204);}
static int l7_tech_flying(int state){return (state>=75&&state<=91)||state==38;}
void l7_tech_reference(L7DIHit *hit,const L7Fighter *first,int stride,int count){
 hit->tech_frame=hit->tech_state=hit->tech_x=0;
 for(int f=hit->frame+1;f<count;f++){
  const L7Fighter *now=(const L7Fighter*)((const char*)first+f*stride);
  const L7Fighter *old=(const L7Fighter*)((const char*)first+(f-1)*stride);
  /* Any intervening damage, including projectiles, belongs to another response. */
  if(now->dead||now->percent>old->percent+.01f||now->percent+.01f<old->percent)return;
  if(l7_tech_outcome(now->state)){
   hit->tech_frame=f;hit->tech_state=now->state;
   if(now->state==200||now->state==201)
    hit->tech_x=(now->facing<0?-80:80)*(now->state==200?1:-1);
   return;
  }
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
 if(frame<=s->onset||cpu->hitlag)return -1;
 return index;
}
