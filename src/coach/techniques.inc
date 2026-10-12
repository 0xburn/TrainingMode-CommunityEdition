/* Technique names describe observed engine states, not merely button guesses. */
#define L7_MAX_GROUPS 128
typedef struct { int frame,end,kind,aerial,direction,jump; } L7Group;
enum { G_AERIAL=1,G_SH_AERIAL,G_SHFFL,G_WAVEDASH,G_WAVESHINE,G_SHINE };
static L7Group l7_groups[L7_MAX_GROUPS];
static int l7_group_count;
static void L7_BuildGroups(void){
 l7_group_count=0;
 RecInputData *track=rec_data.hmn_inputs[Record_GetSlot(0)];
 for(int f=1;f<l7_length&&l7_group_count<L7_MAX_GROUPS;f++){
  L7Fighter *before=&l7_reference[f-1].fighter[0],*now=&l7_reference[f].fighter[0];
  if(now->state==24 && before->state!=24){
   int air=0,land=0,aerial=0,fastfall=0,dodge=0,short_hop=0,dodge_direction=0;
   for(int n=f;n<=l7_length&&n<f+75;n++){
    L7Fighter *sample=&l7_reference[n].fighter[0];
    if(!air&&!sample->grounded){air=n;RecInputs *i=&track->inputs[n>1?n-2:0];short_hop=!(i->btn_x||i->btn_y)&&i->stickY<53;}
    if(sample->state>=65&&sample->state<=69&&!aerial)aerial=sample->state;
    if(sample->fastfall)fastfall=1;
    if(sample->state==236||sample->state==43){RecInputs *input=&track->inputs[n-1];if(input->stickX>24)dodge_direction=1;else if(input->stickX<-24)dodge_direction=-1;}
    if(sample->state==236)dodge=1;
    /* A wavedash may touch down on the same frame that the jump leaves ground. */
    if(sample->state==43&&n<=f+12){dodge=1;land=n;break;}
    if(air&&sample->grounded){land=n;break;}
   }
   if(!land)continue;
   int kind=aerial?(short_hop?G_SH_AERIAL:G_AERIAL):dodge?G_WAVEDASH:0;
   if(aerial&&short_hop&&fastfall&&l7_reference[land].fighter[0].lcancel)kind=G_SHFFL;
   int start=f;
   if(dodge){
    for(int n=f-1;n>=1&&n>=f-12;n--){int state=l7_reference[n].fighter[0].state;if(state>=360&&state<=369){kind=G_WAVESHINE;start=n;while(start>1&&l7_reference[start-1].fighter[0].state>=360&&l7_reference[start-1].fighter[0].state<=369)start--;break;}}
   }
   if(kind){
    float dx=l7_reference[land].fighter[0].x-l7_reference[start].fighter[0].x;
    l7_groups[l7_group_count++]=(L7Group){start,land,kind,aerial,dodge_direction?dodge_direction:dx<0?-1:dx>0?1:0,f};
    OSReport("L7 G %d %d %d %d %d\n",start,land,kind,aerial,dx<0?-1:1);
    f=land;
   }
  }
 }
}
static const char *L7_AerialName(int state){switch(state){case 65:return "nair";case 66:return "fair";case 67:return "bair";case 68:return "up air";case 69:return "down air";default:return "aerial";}}
static void L7_GroupName(L7Group *group,char *text){
 if(group->kind==G_WAVEDASH||group->kind==G_WAVESHINE)sprintf(text,"%s %s",group->kind==G_WAVESHINE?"Waveshine":"Wavedash",group->direction<0?"left":"right");
 else sprintf(text,"%s %s",group->kind==G_SHFFL?"SHFFL":group->kind==G_SH_AERIAL?"Short hop":"Full hop",L7_AerialName(group->aerial));
}

/* Queue entries are techniques, not individual button edges. Each technique
 * keeps the recorded inputs for its constituent phases. */
#define L7_MAX_SEQUENCE 128
#define L7_MAX_CUE_TOKENS 10
typedef struct { int icon,dx,dy; } L7CueToken;
typedef struct { int frame,end,count; char label[48]; L7CueToken tokens[L7_MAX_CUE_TOKENS]; } L7SequenceAction;
static L7SequenceAction l7_sequence[L7_MAX_SEQUENCE];
static int l7_sequence_count;
static void L7_CueToken(L7SequenceAction *out,int icon,int dx,int dy){
 if(out->count<L7_MAX_CUE_TOKENS)out->tokens[out->count++]=(L7CueToken){icon,dx,dy};
}
static void L7_CueStick(L7SequenceAction *out,int icon,int x,int y){
 int dx=x>=24?1:x<=-24?-1:0,dy=y>=24?1:y<=-24?-1:0;
 if(dx||dy)L7_CueToken(out,icon,dx,dy);
}
static RecInputs *L7_CueInput(int frame){return &rec_data.hmn_inputs[Record_GetSlot(0)]->inputs[frame-1];}
static void L7_CueJump(L7SequenceAction *out,int frame){
 RecInputs *r=L7_CueInput(frame);
 if(r->btn_x)L7_CueToken(out,2,0,0);
 else if(r->btn_y)L7_CueToken(out,3,0,0);
 else if(r->stickY>=53)L7_CueStick(out,7,0,r->stickY);
}
static void L7_CueShoulder(L7SequenceAction *out,RecInputs *r){
 if(r->btn_R)L7_CueToken(out,5,0,0);
 else if(r->btn_L||r->trigger>24)L7_CueToken(out,4,0,0);
 else if(r->btn_Z)L7_CueToken(out,6,0,0);
}
static void L7_CueAttack(L7SequenceAction *out,int frame){
 RecInputs *r=L7_CueInput(frame);
 if(r->btn_b){L7_CueStick(out,7,r->stickX,r->stickY);L7_CueToken(out,1,0,0);}
 else if(absolute(r->substickX)>=24||absolute(r->substickY)>=24)L7_CueStick(out,8,r->substickX,r->substickY);
 else if(r->btn_a){L7_CueStick(out,7,r->stickX,r->stickY);L7_CueToken(out,0,0,0);}
 else if(r->btn_Z)L7_CueToken(out,6,0,0);
}
static void L7_CueGroup(L7SequenceAction *out,L7Group *g){
 if(g->jump>=out->frame)L7_CueJump(out,g->jump);
 if(g->kind==G_WAVEDASH||g->kind==G_WAVESHINE){
  for(int f=out->frame;f<=g->end;f++){
   int state=l7_reference[f].fighter[0].state;
   if(state==236||state==43){RecInputs *r=L7_CueInput(f);L7_CueStick(out,7,r->stickX,r->stickY);L7_CueShoulder(out,r);break;}
  }
 }else{
  for(int f=out->frame;f<=g->end;f++){
   L7Fighter *now=&l7_reference[f].fighter[0],*before=&l7_reference[f-1].fighter[0];
   if(now->state>=65&&now->state<=69&&(now->state!=before->state||now->animation<before->animation)){L7_CueAttack(out,f);break;}
  }
  for(int f=out->frame;f<=g->end;f++)if(l7_reference[f].fighter[0].fastfall&&!l7_reference[f-1].fighter[0].fastfall){L7_CueStick(out,7,0,L7_CueInput(f)->stickY);break;}
  if(l7_reference[g->end].fighter[0].lcancel){
   for(int f=g->end;f>=out->frame&&f>g->end-7;f--){RecInputs *r=L7_CueInput(f);if(r->btn_L||r->btn_R||r->btn_Z||r->trigger>24){L7_CueShoulder(out,r);break;}}
  }
 }
}
static void L7_BuildSequence(int entry,int limit,int report){
 l7_sequence_count=0;int cursor=entry;
 while(l7_sequence_count<L7_MAX_SEQUENCE&&cursor<=limit){
  L7Group *group=0;
  for(int i=0;i<l7_group_count;i++)if(l7_groups[i].end>=cursor){group=&l7_groups[i];break;}
  int group_start=group?(group->kind==G_WAVESHINE&&cursor>group->frame?group->jump:group->frame):limit+1;
  L7Event *attack=0;
  for(int i=0;i<l7.event_count;i++){
   L7Event *e=&l7.events[i];if(e->frame<cursor)continue;if(e->frame>=group_start||e->frame>limit)break;
   L7Fighter *now=&l7_reference[e->frame].fighter[0],*before=&l7_reference[e->frame-1].fighter[0];
   if((e->kind==L7_ATTACK_EVENT||e->kind==L7_SPECIAL_EVENT)&&
      (now->state!=before->state||now->animation<before->animation)&&
      ((now->state>=44&&now->state<=69)||now->state>=341)){attack=e;break;}
  }
  if(!attack&&(!group||group_start>limit))break;
  L7SequenceAction *out=&l7_sequence[l7_sequence_count];memset(out,0,sizeof(*out));
  if(attack){
   out->frame=attack->frame;out->end=out->frame;
   int state=l7_reference[out->frame].fighter[0].state;
   while(out->end<limit&&out->end+1<group_start&&l7_reference[out->end+1].fighter[0].state==state)out->end++;
   sprintf(out->label,"%s",state>=65&&state<=69?L7_AerialName(state):l7_kind_name(attack->kind));
   L7_CueAttack(out,out->frame);
  }else if(group->kind==G_WAVESHINE&&cursor<=group->frame){
   out->frame=group->frame;out->end=group->jump-1;sprintf(out->label,"Down B");L7_CueAttack(out,out->frame);
  }else{
   out->frame=group_start<cursor?cursor:group_start;out->end=group->end;
   if(group->kind==G_WAVESHINE||group->kind==G_WAVEDASH){
    float facing=l7_reference[group->jump].fighter[0].facing;
    sprintf(out->label,"Wavedash %s",group->direction*facing<0?"back":"forward");
   }else L7_GroupName(group,out->label);
   L7_CueGroup(out,group);
  }
  if(out->end>limit)out->end=limit;
  cursor=out->end+1;
  if(report&&l7_sequence_count<3)OSReport("L7 SEQUENCE action=%d frame=%d label=%s\n",l7_sequence_count,out->frame,out->label);
  if(report)OSReport("L7 CUE action=%d frame=%d end=%d tokens=%d label=%s\n",l7_sequence_count,out->frame,out->end,out->count,out->label);
  l7_sequence_count++;
 }
}
static int L7_SequenceCursor(int frame){
 int i=0;while(i<l7_sequence_count&&l7_sequence[i].end<frame)i++;return i;
}
