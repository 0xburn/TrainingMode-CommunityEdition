/* Combo boundaries follow the viewer's damage/grab/tech/down states and its
 * 45-frame control gap. Read the captured engine state, never live mistakes. */
static int l7_combo_damage(int s){return (s>=75&&s<=91)||s==38||s==185||s==193||(s>=223&&s<=232)||(((s>=266&&s<=304)||(s>=327&&s<=338))&&s!=293);}
static int l7_combo_keep(int s){return l7_combo_damage(s)||(s>=183&&s<=204)||(s>=0&&s<=10);}
int l7_select_combo(const L7Fighter *first,int stride,int count,int requested,int radius,int *hit){
 int best=-1,best_distance=radius+1,start=-1,gap=0;
 float previous=first->percent;
 for(int f=1;f<count;f++){
  const L7Fighter *now=(const L7Fighter*)((const char*)first+f*stride);
  const L7Fighter *old=(const L7Fighter*)((const char*)first+(f-1)*stride);
  if(now->percent+0.01f<previous||(!old->dead&&now->dead)){start=-1;gap=0;}
  int damaged=l7_combo_damage(now->state);
  int impact=now->hit_instance!=old->hit_instance||now->hitlag||now->state!=old->state;
  int landed=damaged&&impact&&now->direct_hit&&now->percent>previous+0.01f;
  if(landed&&start<0)start=f;
  if(landed&&absolute(f-requested)<best_distance){best_distance=absolute(f-requested);best=start;*hit=f;}
  if(l7_combo_keep(now->state))gap=0;else if(++gap>45)start=-1;
  previous=now->percent;
 }
 return best;
}
int l7_countdown_beat(int remaining,int duration){
 if(remaining>duration)return -1;
 if(remaining<=0)return 0;
 return (remaining*3+duration-1)/duration;
}

/* Credit actual fighter-to-fighter damage; items, lasers and off-screen damage
 * never contribute. Hitlag alone is not a new hit. */
int l7_direct_hit(const L7Fighter *old,const L7Fighter *now){
 return now->direct_hit&&l7_combo_damage(now->state)&&now->percent>old->percent+0.01f&&
  (now->hit_instance!=old->hit_instance||now->hitlag||now->state!=old->state);
}
int l7_combo_end(const L7Fighter *first,int stride,int count,int hit){
 int gap=0;
 for(int f=hit+1;f<count;f++){
  const L7Fighter *now=(const L7Fighter*)((const char*)first+f*stride);
  const L7Fighter *old=(const L7Fighter*)((const char*)first+(f-1)*stride);
  if(now->dead||now->percent+0.01f<old->percent)return f;
  if(l7_combo_keep(now->state))gap=0;else if(++gap>45)return f;
 }
 return count-1;
}
int l7_combo_hits(const L7Fighter *first,int stride,int start,int end){
 int hits=0;
 for(int f=start>0?start:1;f<=end;f++){
  const L7Fighter *now=(const L7Fighter*)((const char*)first+f*stride);
  const L7Fighter *old=(const L7Fighter*)((const char*)first+(f-1)*stride);
  hits+=l7_direct_hit(old,now);
 }
 return hits;
}
void l7_progress_tick(L7Progress *p,const L7Fighter *old,const L7Fighter *now){
 if(p->closed)return;
 if(l7_direct_hit(old,now)){p->started=1;p->gap=0;if(p->hits<p->total)p->hits++;}
 if(now->dead||now->percent+0.01f<old->percent){p->closed=1;return;}
 if(p->started){if(l7_combo_keep(now->state))p->gap=0;else if(++p->gap>45)p->closed=1;}
}
int l7_progress_percent(const L7Progress *p){return p->total>0?100*p->hits/p->total:0;}
