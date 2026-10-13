/* Observations relative to a matched impact, not a same-timeline screenshot.
 * These are coaching heuristics, not counterfactual physics proofs. Never write
 * fighter state or infer spacing from a button-timing error alone. */
static L7Diagnosis l7_diagnostic_measure(int frame,int reference,int move,
 const L7Fighter *human,const L7Fighter *cpu,const L7Fighter *ref_human,const L7Fighter *ref_cpu){
 return (L7Diagnosis){.frame=frame,.reference=reference,.move=move,.delta=frame-reference,
  .dx=(human->x-cpu->x)-(ref_human->x-ref_cpu->x),
  .dy=(human->y-cpu->y)-(ref_human->y-ref_cpu->y),.target_dy=cpu->y-ref_cpu->y};
}
L7Diagnosis l7_diagnose_hit(int frame,int reference,int move,
 const L7Fighter *human,const L7Fighter *cpu,const L7Fighter *before_cpu,
 const L7Fighter *ref_human,const L7Fighter *ref_cpu){
 L7Diagnosis d=l7_diagnostic_measure(frame,reference,move,human,cpu,ref_human,ref_cpu);
 if(human->dead||cpu->dead)return d;
 if(d.delta>=2&&d.target_dy< -2&&!before_cpu->grounded&&cpu->y<before_cpu->y-.05f){
  d.code=L7_DIAG_LATE_LOW;d.priority=90;
 }else if(d.target_dy>3||d.target_dy< -3){
  d.code=L7_DIAG_TARGET_HEIGHT;d.priority=60;
 }else{
  float side=human->x-cpu->x,ref_side=ref_human->x-ref_cpu->x;
  /* Near-overlapping centers are not reliable evidence of the wrong side. */
  if(side*ref_side<0&&side*side>2.25f&&ref_side*ref_side>2.25f){d.code=L7_DIAG_SIDE;d.priority=75;}
  else if(d.dx>4||d.dx< -4){d.code=L7_DIAG_SPACING;d.priority=50;}
 }
 return d;
}
L7Diagnosis l7_diagnose_miss(int frame,int reference,int move,
 const L7Fighter *human,const L7Fighter *cpu,const L7Fighter *ref_human,const L7Fighter *ref_cpu,int shield_contact){
 L7Diagnosis d=l7_diagnostic_measure(frame,reference,move,human,cpu,ref_human,ref_cpu);
 if(human->dead||cpu->dead)return d;
 if(shield_contact){d.code=L7_DIAG_SHIELD;d.priority=100;}
 else if(d.dx>4||d.dx< -4){d.code=L7_DIAG_SPACING;d.priority=50;}
 else if(d.dy>4||d.dy< -4){d.code=L7_DIAG_ATTACK_HEIGHT;d.priority=55;}
 /* A whiff with matching centers could be hitbox choice, animation timing or
  * another mechanic. Abstain rather than inventing a cause. */
 return d;
}
int l7_diagnostic_offer(L7Diagnosis *current,L7Diagnosis next){
 if(!next.code||next.priority<=current->priority)return 0;
 *current=next;return 1;
}
