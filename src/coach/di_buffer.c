/* Match direct hits once, in order, by move and absolute timing. A missed or
 * different move cannot borrow the following hit's DI merely by hit ordinal. */
void l7_di_reset(L7DIState *s,const L7DIHit *hits,int count,int start){
 *s=(L7DIState){.matched=-1};
 while(s->cursor<count&&hits[s->cursor].frame<start)s->cursor++;
}
int l7_di_match(L7DIState *s,const L7DIHit *hits,int count,int frame,int kind,int window){
 s->matched=-1;s->onset=frame;
 if(window<=0)return -1;
 while(s->cursor<count&&hits[s->cursor].frame<frame-window)s->cursor++;
 int best=-1,distance=window+1;
 for(int i=s->cursor;i<count&&hits[i].frame<=frame+window;i++){
  int delta=absolute(hits[i].frame-frame);
  if(hits[i].kind==kind&&delta<distance){best=i;distance=delta;}
 }
 if(best>=0){s->matched=best;s->cursor=best+1;}
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
