/* Read-only native observations. Matching is independent of assistance, so
 * zero-buffer attempts receive the same explanations. */
static void L7_DiagnosticOffer(L7Diagnosis diagnosis){
 if(!l7_diagnostic_offer(&l7_diagnosis,diagnosis))return;
 OSReport("L7 DIAG attempt=%d code=%d frame=%d reference=%d move=%d delta=%d dx=%.4f dy=%.4f target_dy=%.4f priority=%d\n",l7.attempts_count+1,diagnosis.code,diagnosis.frame,diagnosis.reference,diagnosis.move,diagnosis.delta,diagnosis.dx,diagnosis.dy,diagnosis.target_dy,diagnosis.priority);
}
static void L7_DiagnosticReset(int frame){
 l7_diagnosis=(L7Diagnosis){0};
 l7_di_reset(&l7_diagnostic_match,l7_di_hits,l7_di_count,frame);
 l7_diagnostic_candidate_frame=-1;l7_diagnostic_candidate_distance=1e30f;
 for(int p=0;p<2;p++)l7_diagnostic_previous[p]=L7_Fighter(p);
}
static void L7_DiagnosticTick(int frame){
 if(l7.mode!=L7_ACTIVE||!l7_ready||l7_progress_reported)return;
 L7Fighter human=L7_Fighter(0),cpu=L7_Fighter(1);
 if(!L7_On(C_DIAGNOSTICS)){
  /* Re-enabling mid-attempt starts fresh; old observations cannot become advice. */
  L7_DiagnosticReset(frame);return;
 }
 if(l7_direct_hit(&l7_diagnostic_previous[1],&cpu)){
  int match=l7_di_match(&l7_diagnostic_match,l7_di_hits,l7_di_count,frame,human.attack_kind,30);
  if(match>=0){
   int ref=l7_di_hits[match].frame;
   L7_DiagnosticOffer(l7_diagnose_hit(frame,ref,human.attack_kind,&human,&cpu,&l7_diagnostic_previous[1],&l7_reference[ref].fighter[0],&l7_reference[ref].fighter[1]));
  }
  l7_diagnostic_candidate_frame=-1;l7_diagnostic_candidate_distance=1e30f;
 }
 int index=l7_diagnostic_match.cursor;
 if(index<l7_di_count&&l7_di_hits[index].frame<=l7_combo_end_frame){
  L7DIHit *hit=&l7_di_hits[index];int ref=hit->frame;
  L7Fighter *rh=&l7_reference[ref].fighter[0],*rc=&l7_reference[ref].fighter[1];
  FighterData *hf=Fighter_GetGObj(0)->userdata,*cf=Fighter_GetGObj(1)->userdata;
  int active=0;for(int h=0;h<4;h++)if(hf->hitbox[h].active)active=1;
  /* Melee clears shield.dmg_source before this post-frame hook. Use CE's
   * persistent attacker/defender link, as its frame-advantage OSD does. */
  int shield=cpu.state==ASID_GUARDSETOFF&&l7_diagnostic_previous[1].state!=ASID_GUARDSETOFF&&hf->TM.fighter_hurt_shield==cf;
  if(absolute(frame-ref)<=30&&human.attack_kind==hit->kind&&(active||shield)){
   if(shield)L7_DiagnosticOffer(l7_diagnose_miss(frame,ref,hit->kind,&human,&cpu,rh,rc,1));
   /* Choose the closest observed geometry during this attack, not the first
    * active frame while it is still moving toward the opponent. */
   float dx=(human.x-cpu.x)-(rh->x-rc->x),dy=(human.y-cpu.y)-(rh->y-rc->y);
   float distance=dx*dx+dy*dy;
   if(distance<l7_diagnostic_candidate_distance){
    l7_diagnostic_candidate_distance=distance;l7_diagnostic_candidate_frame=frame;
    l7_diagnostic_candidate[0]=human;l7_diagnostic_candidate[1]=cpu;
   }
  }
  if(frame>ref+30){
   if(l7_diagnostic_candidate_frame>=0)
    L7_DiagnosticOffer(l7_diagnose_miss(l7_diagnostic_candidate_frame,ref,hit->kind,&l7_diagnostic_candidate[0],&l7_diagnostic_candidate[1],rh,rc,0));
   else L7_DiagnosticOffer((L7Diagnosis){.code=L7_DIAG_MISSING,.priority=20,.frame=frame,.reference=ref,.move=hit->kind,.delta=frame-ref});
   l7_diagnostic_match.cursor++;
   l7_diagnostic_candidate_frame=-1;l7_diagnostic_candidate_distance=1e30f;
  }
 }
 if(human.state>=70&&human.state<=74&&!human.lcancel&&
    !(l7_diagnostic_previous[0].state>=70&&l7_diagnostic_previous[0].state<=74)){
  int best=-1,distance=31;
  for(int f=frame>30?frame-30:1;f<=l7_length&&f<=frame+30;f++){
   L7Fighter *r=&l7_reference[f].fighter[0],*old=&l7_reference[f-1].fighter[0];
   if(r->state==human.state&&old->state!=r->state&&r->lcancel&&absolute(f-frame)<distance){best=f;distance=absolute(f-frame);}
  }
  if(best>=0)L7_DiagnosticOffer((L7Diagnosis){.code=L7_DIAG_LCANCEL,.priority=80,.frame=frame,.reference=best,.move=human.attack_kind,.delta=frame-best});
 }
 l7_diagnostic_previous[0]=human;l7_diagnostic_previous[1]=cpu;
}
static void L7_DiagnosticText(char *title,char *advice){
 L7Diagnosis *d=&l7_diagnosis;
 const char *move="Attack";
 if(d->move>=13&&d->move<=17){static const char *names[]={"Nair","Fair","Bair","Up air","Down air"};move=names[d->move-13];}
 else if(d->move==21){FighterData *human=Fighter_GetGObj(0)->userdata;move=human->kind==FTKIND_FOX||human->kind==FTKIND_FALCO?"Shine":"Down B";}
 switch(d->code){
 case L7_DIAG_LATE_LOW:sprintf(title,"%s: target had fallen",move);sprintf(advice,"Try this attack sooner");break;
 case L7_DIAG_TARGET_HEIGHT:sprintf(title,"%s: target was %s",move,d->target_dy<0?"lower":"higher");sprintf(advice,"Match the shown hit height");break;
 case L7_DIAG_SIDE:sprintf(title,"%s hit from the other side",move);sprintf(advice,"Approach from the %s",d->dx>0?"left":"right");break;
 case L7_DIAG_SPACING:sprintf(title,"%s: too far %s",move,d->dx>0?"right":"left");sprintf(advice,"Shift %s before attacking",d->dx>0?"left":"right");break;
 case L7_DIAG_ATTACK_HEIGHT:sprintf(title,"%s was too %s",move,d->dy>0?"high":"low");sprintf(advice,"Use a %s attack",d->dy>0?"lower":"higher");break;
 case L7_DIAG_SHIELD:sprintf(title,"%s hit shield",move);sprintf(advice,d->delta>=2?"Try this attack sooner":"Check the gap before this hit");break;
 case L7_DIAG_MISSING:sprintf(title,"%s was not seen",move);sprintf(advice,"Preview the next attack");break;
 case L7_DIAG_LCANCEL:sprintf(title,"L-cancel missed");sprintf(advice,"Press L or R before landing");break;
 default:title[0]=advice[0]=0;break;
 }
}
