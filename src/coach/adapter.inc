/* Included by pinned TM-CE lab.c: engine-facing hooks live here. */
#include "coach.c"
#include "config.generated.h"
/* The stock HUD allocates five SIS strings per row for its outline. Use a
 * bounded coach cache with one shadow and one foreground string instead. */
static Text *l7_text_cache[16];
static int l7_text_used;
static void L7_Text(const char *text,Rect *rect,float size){
 if(l7_text_used>=16)return;
 HUDCamData *hud=event_vars->hudcam_gobj->userdata;
 Text **slot=&l7_text_cache[l7_text_used++];
 if(!*slot){
  Text *t=Text_CreateText(2,hud->canvas);*slot=t;
  t->kerning=1;t->align=1;t->use_aspect=1;t->aspect.X=165;
  t->viewport_scale.X=t->viewport_scale.Y=0.1f;
  Text_AddSubtext(t,0,0,"");Text_AddSubtext(t,0,0,"");
  GXColor shadow={0,0,0,255};Text_SetColor(t,0,&shadow);
 }
 char safe[128];l7_hud_text(safe,text);Text *t=*slot;t->hidden=false;
 float x=rect->x*10.f+rect->w*5.f,y=rect->y*-10.f+rect->h*-5.f-25.f;
 for(int i=0;i<2;i++){Text_SetText(t,i,safe);Text_SetScale(t,i,size,size);Text_SetPosition(t,i,x+(i?0:1),y+(i?0:1));}
}
#define L7_FRAMES 3601
static L7Coach l7;
typedef struct { float x,y,size; int active; } L7Hit;
typedef struct { Vec2 stick; int valid, moved; } L7SDI;
typedef struct { Vec2 velocity; int valid; } L7Launch;
typedef struct { L7Fighter fighter[2]; L7Hit hit[4]; L7SDI sdi; L7Launch launch; int valid; } L7Sample;
static L7Sample *l7_reference;
static int l7_ready,l7_length,l7_loop,l7_last_post=-1,l7_saved_attempts,l7_reference_errors;
static int l7_test_target=-1,l7_last_mode=-1,l7_buffer_until=-1,l7_buffer_kind;
/* Native acceptance can target a later input sequence; ordinary play ignores it. */
static int l7_test_shift_from, l7_test_shift_amount;
static RecInputs l7_buffer_input,l7_effective;
static int l7_override;
static int l7_consumed_buttons;
static int l7_rng_count;
static int l7_was_enabled,l7_saved_loop,l7_saved_restore,l7_saved_counter;
static int l7_reset_after_post;
/* One checked, item-free checkpoint per second, captured only during warmup. */
static Savestate_v1 *l7_checkpoints;
static int l7_checkpoint_count,l7_entry=-1,l7_retry_checkpoint,l7_retry_after_post,l7_up_held,l7_last_beat=-1;
static int l7_scheduled_run,l7_left_held,l7_watch_after_post,l7_selection_failed,l7_combo_first_hit;
static int l7_buffer_early,l7_buffer_applied;
static L7Progress l7_progress;
static L7Fighter l7_progress_previous;
static L7DIHit l7_di_hits[L7_MAX_EVENTS];
static int l7_di_count,l7_di_applied;
static L7DIState l7_di;
static L7Fighter l7_di_previous;
static L7Diagnosis l7_diagnosis;
static L7DIState l7_diagnostic_match;
static L7Fighter l7_diagnostic_previous[2],l7_diagnostic_candidate[2];
static int l7_diagnostic_candidate_frame;
static float l7_diagnostic_candidate_distance;
static int l7_selected_hit=-1,l7_combo_end_frame,l7_combo_total;
static int l7_best_hits,l7_latest_hits=-1,l7_good_job_ticks,l7_ui_ticks;
static int l7_down_held,l7_right_held,l7_preview_active,l7_preview_after_post;
static int l7_preview_start,l7_preview_end,l7_progress_reported,l7_result_improved;
static const char *l7_countdowns[]={"Long","Normal","Short"};
static int l7_magnify_frame=-1;
static int L7_Frame(void){return event_vars->game_timer-rec_state->frame;}
static int l7_pending_command;
static const char *l7_yesno[]={"Off","On"};
static const char *l7_speeds[]={"100%","75%","50%","25%"};
static const char *l7_opponents[]={"Original inputs","Hit-relative DI","Neutral DI"};
static const char *l7_handoffs[]={"Countdown","Finish action"};
static const char *l7_scenarios[]={"Human","Perfect","3 frames late","3 frames early","No input","Buffered early","DI test late","DI test early","DI test outside"};
enum { C_ENABLED,C_LOOP,C_TAKEOVER,C_TIMELINE,C_AUDIO,C_GHOST,C_HITBOX,C_FEEDBACK,C_SPEED,C_BUFFER,C_OPPONENT,C_TRACK,C_TELEMETRY,C_TEST,C_HANDOFF,C_LEAD,C_SHORTCUT,C_PREROLL,C_CELEBRATION,C_DI_BUFFER,C_DIAGNOSTICS,C_RESET,C_ARM,C_COUNT };
static void L7_MenuReset(GOBJ*g){l7_pending_command=1;}
static void L7_MenuTakeover(GOBJ*g){l7_pending_command=2;}
#define L7_TOGGLE(label,value) {.kind=OPTKIND_STRING,.name=label,.value_num=2,.values=l7_yesno,.val=value}
static EventOption L7_Options[C_COUNT]={
 L7_TOGGLE("Coach",CFG_ENABLED),L7_TOGGLE("Loop",CFG_LOOP),L7_TOGGLE("Takeover",CFG_TAKEOVER),
 L7_TOGGLE("Action cues",CFG_TIMELINE),L7_TOGGLE("Rhythm sounds",CFG_AUDIO),L7_TOGGLE("Ghost trail",CFG_GHOST),
 L7_TOGGLE("Reference hitboxes",CFG_HITBOX),L7_TOGGLE("Timing feedback",CFG_FEEDBACK),
 {.kind=OPTKIND_STRING,.name="Speed",.value_num=4,.values=l7_speeds,.val=CFG_SPEED},
 {.kind=OPTKIND_INT,.name="Early buffer",.value_num=7,.format="%df",.val=CFG_BUFFER},
 {.kind=OPTKIND_STRING,.name="Opponent",.value_num=3,.values=l7_opponents,.val=CFG_OPPONENT},
 L7_TOGGLE("Session tracking",CFG_TRACK),L7_TOGGLE("Test telemetry",CFG_TELEMETRY),
 {.kind=OPTKIND_STRING,.name="Test inputs",.value_num=9,.values=l7_scenarios,.val=CFG_TEST},
 {.kind=OPTKIND_STRING,.name="Handoff",.value_num=2,.values=l7_handoffs,.val=CFG_HANDOFF},
 {.kind=OPTKIND_STRING,.name="Countdown Length",.value_num=3,.values=l7_countdowns,.val=CFG_COUNTDOWN},
 L7_TOGGLE("D-pad left reset",CFG_SHORTCUT),
 {.kind=OPTKIND_INT,.name="Lead-in frames",.value_num=181,.format="%df",.val=CFG_PREROLL},
 L7_TOGGLE("Celebration sound",CFG_CELEBRATION),
 {.kind=OPTKIND_INT,.name="Opponent DI buffer",.value_num=31,.format="%df",.val=CFG_DI_BUFFER},
 L7_TOGGLE("Advanced diagnostics",CFG_DIAGNOSTICS),
 {.kind=OPTKIND_FUNC,.name="Reset attempt",.OnSelect=L7_MenuReset},
 {.kind=OPTKIND_FUNC,.name="Take over",.OnSelect=L7_MenuTakeover},
};
static EventMenu L7_Menu={.name="Combo Coach Settings",.option_num=C_COUNT,.options=L7_Options};
static int L7_On(int option){return L7_Options[option].val;}
static int L7_Lead(void){static const int frames[]={109,55,30};return frames[clamp(L7_On(C_LEAD),0,2)];}
static int L7_Enabled(void){return L7_On(C_ENABLED)&&l7_length>0;}
static float L7_Speed(void){if(L7_Enabled()&&l7_preview_active&&L7_Frame()<l7_preview_start)return 4.0f;if(L7_Enabled()&&l7.mode==L7_COUNTDOWN&&l7_entry-L7_Frame()>L7_Lead())return 4.0f;static const float values[]={1,0.75f,0.5f,0.25f};return L7_On(C_ENABLED)?values[L7_On(C_SPEED)]:1;}
static void L7_Magnify(GOBJ*g,int pass){
 /* NTSC 1.02 magnifier updates gameplay-visible flags from a draw callback.
  * Extra renders between slow-motion simulation ticks must not advance them.
  * The texture remains available to the ordinary HUD drawing callback. */
 if(L7_Enabled()){
  int frame=event_vars->game_timer;
  if(frame==l7_magnify_frame)return;
  l7_magnify_frame=frame;
 }
 ((void(*)(GOBJ*,int))0x802fbbdc)(g,pass);
}
static L7Input L7_FromRecord(RecInputs*i){
 return (L7Input){.buttons=(i->btn_a?L7_A:0)|(i->btn_b?L7_B:0)|((i->btn_x||i->btn_y)?L7_JUMP:0)|((i->btn_L||i->btn_R||i->btn_Z)?L7_SHIELD:0),.x=i->stickX,.y=i->stickY,.cx=i->substickX,.cy=i->substickY,.trigger=i->trigger};
}
static L7Input L7_FromPad(HSD_Pad*p){
 return (L7Input){.buttons=((p->held&HSD_BUTTON_A)?L7_A:0)|((p->held&HSD_BUTTON_B)?L7_B:0)|((p->held&(HSD_BUTTON_X|HSD_BUTTON_Y))?L7_JUMP:0)|((p->held&(HSD_TRIGGER_L|HSD_TRIGGER_R|HSD_TRIGGER_Z))?L7_SHIELD:0),.x=p->stickX,.y=p->stickY,.cx=p->substickX,.cy=p->substickY,.trigger=p->triggerLeft>p->triggerRight?p->triggerLeft:p->triggerRight};
}
static RecInputs L7_RecordPad(HSD_Pad*p){
 RecInputs r={0};r.btn_a=!!(p->held&HSD_BUTTON_A);r.btn_b=!!(p->held&HSD_BUTTON_B);r.btn_x=!!(p->held&HSD_BUTTON_X);r.btn_y=!!(p->held&HSD_BUTTON_Y);r.btn_L=!!(p->held&HSD_TRIGGER_L);r.btn_R=!!(p->held&HSD_TRIGGER_R);r.btn_Z=!!(p->held&HSD_TRIGGER_Z);
 r.stickX=p->stickX;r.stickY=p->stickY;r.substickX=p->substickX;r.substickY=p->substickY;r.trigger=p->triggerLeft>p->triggerRight?p->triggerLeft:p->triggerRight;return r;
}
static void L7_BufferChannel(RecInputs*out,RecInputs*from,int kind){
 switch(kind){case L7_JUMP_EVENT:out->btn_x=from->btn_x;out->btn_y=from->btn_y;break;
 case L7_ATTACK_EVENT:out->btn_a=from->btn_a;out->substickX=from->substickX;out->substickY=from->substickY;break;
 case L7_SPECIAL_EVENT:out->btn_b=from->btn_b;break;
 case L7_SHIELD_EVENT:out->btn_L=from->btn_L;out->btn_R=from->btn_R;out->btn_Z=from->btn_Z;out->trigger=from->trigger;break;}
}
static RecInputs L7_FilterRequest(RecInputs input){
 if(l7_consumed_buttons&L7_A)input.btn_a=0;
 if(l7_consumed_buttons&L7_B)input.btn_b=0;
 if(l7_consumed_buttons&L7_JUMP){input.btn_x=0;input.btn_y=0;}
 if(l7_consumed_buttons&L7_SHIELD){input.btn_L=0;input.btn_R=0;input.btn_Z=0;input.trigger=0;}
 return input;
}
static L7Input L7_Assist(int frame,RecInputs effective){
 l7_override=0;l7_effective=effective;l7_buffer_applied=0;
 if(l7.mode!=L7_ACTIVE || !L7_On(C_BUFFER))return L7_FromRecord(&effective);
 L7Input input=L7_FromRecord(&effective);int edges=l7_input_kinds(input,l7.previous);
 if(l7_buffer_until<0&&l7.event_cursor<l7.event_count){L7Event*e=&l7.events[l7.event_cursor];int early=e->frame-frame;
  if(early>0&&early<=L7_On(C_BUFFER)&&e->kind<=L7_SHIELD_EVENT&&(edges&(1<<e->kind))){l7_buffer_until=e->frame;l7_buffer_input=effective;l7_buffer_kind=e->kind;l7_buffer_early=early;}
 }
 if(l7_buffer_until>=0){RecInputs neutral={0};l7_override=1;
  if(frame<l7_buffer_until)L7_BufferChannel(&l7_effective,&neutral,l7_buffer_kind);
  else {L7_BufferChannel(&l7_effective,&l7_buffer_input,l7_buffer_kind);l7_buffer_until=-1;l7_buffer_applied=1;}
 }
 return L7_FromRecord(&l7_effective);
}
static L7Fighter L7_Fighter(int p){FighterData*f=Fighter_GetGObj(p)->userdata;return(L7Fighter){.state=f->state_id,.actionable=CheckOverlay(Fighter_GetGObj(p),OVERLAY_ACTIONABLE),.hitlag=f->flags.hitlag,.dead=f->flags.dead,.hit_instance=f->dmg.atk_instance_hurtby,.direct_hit=f->dmg.hit_log.source==Fighter_GetGObj(1-p),.attack_instance=f->atk_instance,.attack_kind=f->atk_kind,.facing=f->facing_direction,.animation=f->state.frame,.x=f->phys.pos.X,.y=f->phys.pos.Y,.percent=f->dmg.percent,.grounded=f->phys.air_state==0,.fastfall=f->flags.is_fastfall,.lcancel=f->state_id>=70&&f->state_id<=74&&(u8)f->input.timer_trigger_any_ignore_hitlag<7};}
/* Native damage SDI callback (NTSC 1.02). Observe its actual decision during
 * reference playback, including UCF's history-dependent extensions. During a
 * matched live hit, replay that decision through the same callback. Do not edit
 * position or the global raw-pad history shared with the human controller. */
static void L7_SDIThink(GOBJ *gobj){
 FighterData *cpu=gobj->userdata;
 void (*native)(GOBJ*)=(void*)0x8008e4f0;
 int frame=L7_Frame();
 if(frame<0||frame>=L7_FRAMES||!L7_On(C_ENABLED)){native(gobj);return;}
 float x=cpu->phys.pos.X,y=cpu->phys.pos.Y;
 Vec2 stick=cpu->input.lstick;
 if(!l7_ready){
  native(gobj);
  l7_reference[frame].sdi=(L7SDI){.stick=stick,.valid=1,.moved=fabs(cpu->phys.pos.X-x)>.001f||fabs(cpu->phys.pos.Y-y)>.001f};
  if(l7_reference[frame].sdi.moved&&L7_On(C_TELEMETRY))OSReport("L7 SDI REF frame=%d dx=%.6f dy=%.6f\n",frame,cpu->phys.pos.X-x,cpu->phys.pos.Y-y);
  return;
 }
 int sample=-1;
 if(l7.mode==L7_ACTIVE&&L7_On(C_OPPONENT)==0&&L7_On(C_DI_BUFFER)>0&&cpu->flags.hitlag)
  sample=l7_sdi_sample(&l7_di,l7_di_hits,l7_di_count,frame);
 if(sample<0||(sample>0&&!l7_reference[sample].sdi.valid)){native(gobj);return;}
 L7SDI *ref=sample>0?&l7_reference[sample].sdi:0;
 u8 tx=cpu->input.timer_lstick_tilt_x,ty=cpu->input.timer_lstick_tilt_y;
 u8 sx=cpu->input.timer_lstick_smash_x,sy=cpu->input.timer_lstick_smash_y;
 /* 0x670/671 are consumed SDI timers; 0x673/674 are sticky timers used by
  * UCF. Disable its fallback while reproducing the already-observed decision. */
 if(ref)cpu->input.lstick=ref->stick;
 cpu->input.timer_lstick_tilt_x=ref&&ref->moved?0:254;
 cpu->input.timer_lstick_tilt_y=254;
 cpu->input.timer_lstick_smash_x=cpu->input.timer_lstick_smash_y=254;
 native(gobj);
 int moved=fabs(cpu->phys.pos.X-x)>.001f||fabs(cpu->phys.pos.Y-y)>.001f;
 cpu->input.lstick=stick;
 cpu->input.timer_lstick_smash_x=sx;cpu->input.timer_lstick_smash_y=sy;
 if(!moved){cpu->input.timer_lstick_tilt_x=tx;cpu->input.timer_lstick_tilt_y=ty;}
 if(L7_On(C_TELEMETRY))OSReport("L7 SDI APPLY frame=%d sample=%d expected=%d moved=%d dx=%.6f dy=%.6f\n",frame,sample,ref?ref->moved:0,moved,cpu->phys.pos.X-x,cpu->phys.pos.Y-y);
}
/* NTSC 1.02 damage hitlag exit. Keep native ASDI and collision, then preserve
 * the reference launch direction within the live hit's legal DI angle range.
 * This never snaps positions, copies reference speed, or changes hit timing. */
static void L7_DIExit(GOBJ *gobj){
 FighterData *cpu=gobj->userdata;
 int frame=L7_Frame();
 float incoming_x=cpu->phys.kb_vel.X,incoming_y=cpu->phys.kb_vel.Y;
 ((void(*)(GOBJ*))0x8008e714)(gobj);
 if(!l7_reference||frame<0||frame>=L7_FRAMES||!L7_On(C_ENABLED))return;
 if(!l7_ready){
  l7_reference[frame].launch=(L7Launch){.velocity={cpu->phys.kb_vel.X,cpu->phys.kb_vel.Y},.valid=1};
  if(L7_On(C_TELEMETRY))OSReport("L7 LAUNCH REF frame=%d in_x=%.6f in_y=%.6f x=%.6f y=%.6f\n",frame,incoming_x,incoming_y,cpu->phys.kb_vel.X,cpu->phys.kb_vel.Y);
  return;
 }
 if(l7.mode!=L7_ACTIVE||L7_On(C_OPPONENT)!=0||L7_On(C_DI_BUFFER)<=0||l7_di.matched<0||l7_di.matched>=l7_di_count)return;
 L7DIHit *hit=&l7_di_hits[l7_di.matched];
 int sample=hit->last_input+1;
 if(sample>=L7_FRAMES||!l7_reference[sample].launch.valid)return;
 Vec2 reference=l7_reference[sample].launch.velocity;
 float old_x=cpu->phys.kb_vel.X,old_y=cpu->phys.kb_vel.Y;
 int corrected=l7_di_launch(incoming_x,incoming_y,reference.X,reference.Y,(*stc_ftcommon)->tdi_maxAngle,&cpu->phys.kb_vel.X,&cpu->phys.kb_vel.Y);
 if(L7_On(C_TELEMETRY))OSReport("L7 LAUNCH APPLY frame=%d hit=%d corrected=%d in_x=%.6f in_y=%.6f native_x=%.6f native_y=%.6f x=%.6f y=%.6f ref_x=%.6f ref_y=%.6f\n",frame,hit->frame,corrected,incoming_x,incoming_y,old_x,old_y,cpu->phys.kb_vel.X,cpu->phys.kb_vel.Y,reference.X,reference.Y);
}
static int L7_EventMask(int state,L7Input input){
 int mask=127;
 if(state && !(state>=14&&state<=24))mask&=~(1<<L7_DASH_EVENT);
 if((input.buttons&L7_B)||(state && !(state>=25&&state<=38)&&!(state>=65&&state<=69)))mask&=~(1<<L7_FASTFALL_EVENT);
 return mask;
}
/* Reference hitlag end is the first post-frame with hitlag cleared. The engine
 * consumes the preceding input at priority 0, before new priority-3 inputs. */
static void L7_BuildDI(void){
 l7_di_count=0;
 for(int f=1;f<l7_length&&l7_di_count<L7_MAX_EVENTS;f++){
  if(!l7_direct_hit(&l7_reference[f-1].fighter[1],&l7_reference[f].fighter[1]))continue;
  int end=f;
  while(end<l7_length&&l7_reference[end].fighter[1].hitlag)end++;
  if(end<=f+1)continue;
  L7DIHit *hit=&l7_di_hits[l7_di_count++];
  *hit=(L7DIHit){.frame=f,.last_input=end-1,.kind=l7_reference[f].fighter[0].attack_kind};
  l7_tech_reference(hit,&l7_reference[0].fighter[1],sizeof(L7Sample),l7_length+1);
  OSReport("L7 DI REF frame=%d last=%d kind=%d before_ground=%d before_state=%d\n",f,end-1,l7_reference[f].fighter[0].attack_kind,l7_reference[f-1].fighter[1].grounded,l7_reference[f-1].fighter[1].state);
  if(hit->tech_frame)OSReport("L7 TECH REF hit=%d frame=%d state=%d x=%d bounce=%d\n",f,hit->tech_frame,hit->tech_state,hit->tech_x,hit->tech_bounce);
 }
 if(L7_On(C_TELEMETRY))OSReport("L7 TECH TABLE address=%x stride=%d count=%d\n",(u32)l7_di_hits,sizeof(L7DIHit),l7_di_count);
}
#include "techniques.inc"
static void L7_BuildEvents(void){
 RecInputData*r=rec_data.hmn_inputs[Record_GetSlot(0)];L7Input before={0};l7.event_count=0;
 for(int frame=1;frame<=r->num&&frame<L7_FRAMES;frame++){
  L7Input input=L7_FromRecord(&r->inputs[frame-1]);int kinds=l7_input_kinds(input,before);before=input;
  int prior=l7_reference&&l7_reference[frame-1].valid?l7_reference[frame-1].fighter[0].state:0;
  kinds&=L7_EventMask(prior,input);
  for(int k=1;k<=L7_FASTFALL_EVENT;k++)if(kinds&(1<<k)){
   int state=l7_reference&&l7_reference[frame].valid?l7_reference[frame].fighter[0].state:0;
   l7_add_event(&l7,(L7Event){.frame=frame,.kind=k,.action=state});
  }
 }
 l7_test_target=-1;
 for(int i=0;i<l7.event_count;i++)if(l7.events[i].frame>CFG_TEST_REQUEST+45 && l7.events[i].kind==L7_JUMP_EVENT){l7_test_target=l7.events[i].frame;break;}
}
#include "diagnostics.inc"
static void L7_SaveProgress(void){
 if(!l7_progress.total||l7_progress_reported)return;
 l7_progress_reported=1;
 l7_latest_hits=l7_progress.hits;
 int improved=l7_latest_hits>l7_best_hits;
 l7_result_improved=improved;
 if(improved){l7_best_hits=l7_latest_hits;l7_good_job_ticks=180;
  if(L7_On(C_CELEBRATION)){int voice=((int(*)(int,int,int))0x800237a8)(0x9c41,127,64);OSReport("L7 CONGRATULATIONS voice=%d\n",voice);}
 }
 OSReport("L7 RESULT attempt=%d hits=%d total=%d percent=%d best=%d improved=%d frame=%d\n",(l7.current.sequence?l7.current.sequence:l7.attempts_count+1),l7_latest_hits,l7_progress.total,l7_progress_percent(&l7_progress),l7_best_hits,improved,L7_Frame());
}
static void L7_ReportAttempt(void){
 if(l7.attempts_count==l7_saved_attempts)return;
 l7_saved_attempts=l7.attempts_count;
 L7Attempt*a=&l7.current;
 OSReport("L7 A %d %d %d %d %d %d %d %d %d %d %d\n",a->sequence,a->start,a->end,a->completed,a->assisted,a->first_kind,a->first_error,a->first_delta,a->matched,a->total,a->aborted);
 L7_SaveProgress();
 OSReport("L7 PROGRESS attempt=%d hits=%d total=%d percent=%d best=%d improved=%d\n",a->sequence,l7_progress.hits,l7_progress.total,l7_progress_percent(&l7_progress),l7_best_hits,l7_result_improved);
 if(a->buffer_saves)OSReport("L7 BUFFER_RESULT attempt=%d count=%d kind=%d delta=%d\n",a->sequence,a->buffer_saves,a->buffer_kind,a->buffer_delta);
}
static void L7_Reset(void){
 if(!l7_reference)return;
 if(l7.mode==L7_ACTIVE){l7_finish(&l7,l7.frame,1);L7_ReportAttempt();}
 OSReport("L7 RESET loop=%d\n",l7_loop);
 l7_reset(&l7,0);l7_last_post=-1;l7_magnify_frame=-1;l7_buffer_until=-1;l7_consumed_buttons=0;l7_scheduled_run=0;l7_last_beat=-1;l7_buffer_applied=0;l7_progress=(L7Progress){0};l7_progress_reported=0;l7_result_improved=0;l7_preview_active=0;l7_di_applied=0;l7_di_reset(&l7_di,l7_di_hits,l7_di_count,0);
 L7_DiagnosticReset(0);
}
static void L7_Init(void){
 l7_init(&l7,(L7Config){.enabled=CFG_ENABLED,.loop=CFG_LOOP,.takeover=CFG_TAKEOVER,.tolerance=CFG_TOLERANCE,.match_window=12,.buffer_frames=CFG_BUFFER,.reset_delay=CFG_RESET_DELAY,.request_deadzone=40,.pending_timeout=180,.attempt_tracking=CFG_TRACK});
 Audio_QueueFileLoad(2,1ULL<<FGM_NR_1P);Audio_UpdateCache();Audio_SyncLoadAll();
 l7_reference=calloc(sizeof(L7Sample)*L7_FRAMES);l7_ready=0;l7_loop=0;l7_saved_attempts=0;l7_reference_errors=0;l7_sequence_count=0;l7_di_count=0;
 l7_length=rec_data.hmn_inputs[Record_GetSlot(0)]->num;if(l7_length>=L7_FRAMES)l7_length=L7_FRAMES-1;
 l7_selected_hit=-1;l7_best_hits=0;l7_latest_hits=-1;l7_combo_total=0;l7_good_job_ticks=0;l7_ui_ticks=0;l7_down_held=0;l7_right_held=0;l7_preview_after_post=0;
 l7_entry=-1;l7_retry_after_post=0;l7_reset_after_post=0;l7_up_held=0;l7_left_held=0;l7_watch_after_post=0;l7_selection_failed=0;
 l7_checkpoint_count=l7_length/60+1;
 l7_checkpoints=calloc(sizeof(Savestate_v1)*l7_checkpoint_count);
 if(l7_checkpoints)memcpy(&l7_checkpoints[0],rec_state,sizeof(Savestate_v1));
 L7_BuildEvents();
 for(GOBJ*g=(*stc_gobj_lookup)[15];g;g=g->next)if((u32)g->gx_cb==0x802fbbdc){g->gx_cb=L7_Magnify;OSReport("L7 MAGNIFY simulation clock\n");}
 u8 *metadata=rec_state->event_data+480;
 l7_rng_count=(*(u32*)metadata==0x4c37434f && *(u32*)(metadata+4)==0x41434831 && metadata[10]==1)?*(u32*)(metadata+16):0;
 if(l7_rng_count>3600)l7_rng_count=0;
 int high_port=(*(u32*)metadata==0x4c37434f && *(u32*)(metadata+4)==0x41434831) ? metadata[8]>metadata[9] : CFG_HUMAN_HIGH_PORT;
 OSReport("L7 ORDER human_high=%d metadata=%d\n",high_port,(*(u32*)metadata==0x4c37434f && *(u32*)(metadata+4)==0x41434831));
 if(high_port){
  /* NTSC 1.02 engine routine: reorder the fighter and every simulation proc. */
  GOBJ *human=Fighter_GetGObj(0),*cpu=Fighter_GetGObj(1);
  ((void(*)(u32,GOBJ*,u8,u8,GOBJ*))0x8039032c)(2,human,human->p_link,human->p_priority,cpu);
 }
 OSReport("L7 CONTROL options=%x stride=%d value=%d command=%x\n",(u32)L7_Options,sizeof(EventOption),offsetof(EventOption,val),(u32)&l7_pending_command);
 OSReport("L7 DIAGNOSTICS version=1\n");
 OSReport("L7 PAUSE address=%x\n",(u32)&LabOptions_General[OPTGEN_FRAME].val);
 if(L7_On(C_TELEMETRY))OSReport("L7 TEST_SHIFT address=%x amount=%x\n",(u32)&l7_test_shift_from,(u32)&l7_test_shift_amount);
 if(L7_On(C_TELEMETRY))OSReport("L7 TEST_INPUTS human=%x cpu=%x\n",(u32)rec_data.hmn_inputs[Record_GetSlot(0)]->inputs,(u32)rec_data.cpu_inputs[Record_GetSlot(1)]->inputs);
 OSReport("L7 INIT frames=%d config=%x human=%x cpu=%x\n",l7_length,(u32)&L7_Options,(u32)Fighter_GetGObj(0)->userdata,(u32)Fighter_GetGObj(1)->userdata);
 l7_was_enabled=L7_On(C_ENABLED);l7_saved_loop=LabOptions_Record[OPTREC_LOOP].val;l7_saved_restore=LabOptions_Record[OPTREC_AUTORESTORE].val;l7_saved_counter=LabOptions_Record[OPTREC_PLAYBACK_COUNTER].val;
 if(l7_length>0&&L7_On(C_ENABLED)){
  LabOptions_Record[OPTREC_LOOP].val=0;LabOptions_Record[OPTREC_AUTORESTORE].val=0;LabOptions_Record[OPTREC_PLAYBACK_COUNTER].val=0;
  LabOptions_Record[OPTREC_HMNMODE].val=RECMODE_HMN_PLAYBACK;LabOptions_Record[OPTREC_CPUMODE].val=RECMODE_CPU_PLAYBACK;
 }
}
static int L7_ChooseEntry(int frame){
 int hit=-1;
 int first=l7_select_combo(&l7_reference[0].fighter[1],sizeof(L7Sample),l7_length+1,frame,CFG_HIT_WINDOW,&hit);
 if(first<0){l7_selection_failed=1;OSReport("L7 SELECT none requested=%d\n",frame);return -1;}
 /* Handoff belongs to the attack that causes the selected hit. Setup
  * movement stays on the original track, even within the same combo. */
 int entry=-1;
 for(int i=0;i<l7.event_count;i++){
  L7Event *event=&l7.events[i];if(event->frame>hit)break;
  if(event->kind==L7_ATTACK_EVENT||event->kind==L7_SPECIAL_EVENT||event->kind==L7_SHIELD_EVENT){
   int state=event->action;L7Fighter *before=&l7_reference[event->frame-1].fighter[0],*after=&l7_reference[event->frame].fighter[0];
   int onset=state!=before->state||after->animation<before->animation;
   if(after->attack_instance==l7_reference[hit].fighter[1].hit_instance&&onset&&((state>=44&&state<=69)||state>=341||(state>=212&&state<=222)))entry=event->frame;
  }
 }
 if(entry>=0&&hit-entry>180)entry=-1;
 if(entry<0){l7_selection_failed=1;return -1;}
 if(hit!=l7_selected_hit){l7_best_hits=0;l7_latest_hits=-1;l7_good_job_ticks=0;}
 l7_selected_hit=hit;
 l7_combo_end_frame=l7_combo_end(&l7_reference[0].fighter[1],sizeof(L7Sample),l7_length+1,hit);
 l7_combo_total=l7_combo_hits(&l7_reference[0].fighter[1],sizeof(L7Sample),hit,l7_combo_end_frame);
 L7_BuildSequence(entry,l7_combo_end_frame,1);
 OSReport("L7 COMBO hit=%d end=%d total=%d\n",hit,l7_combo_end_frame,l7_combo_total);
 l7_combo_first_hit=first;l7_selection_failed=0;
 /* Select a jump inside this attempt for the synthetic early/late scenarios. */
 l7_test_target=-1;for(int i=0;i<l7.event_count;i++)if(l7.events[i].frame>entry+6&&l7.events[i].kind==L7_JUMP_EVENT){l7_test_target=l7.events[i].frame;break;}
 OSReport("L7 SELECT requested=%d hit=%d first=%d entry=%d before=%d target=%d\n",frame,hit,first,entry,L7_On(C_PREROLL),l7_test_target);
 return entry;
}
static void L7_Retry(void){
 if(!l7_ready||!L7_On(C_TAKEOVER))return;
 if(l7_entry<0){l7_selection_failed=1;return;}
 int lead=L7_On(C_PREROLL)>L7_Lead()?L7_On(C_PREROLL):L7_Lead();
 int wanted=l7_entry-lead;
 l7_retry_checkpoint=0;
 if(l7_checkpoints)for(int i=1;i<l7_checkpoint_count;i++){
  Savestate_v1 *state=&l7_checkpoints[i];
  if(state->is_exist&&state->frame-rec_state->frame<=wanted)l7_retry_checkpoint=i;
 }
 l7_retry_after_post=1;
}
static void L7_RestoreCountdown(void){
 l7_retry_after_post=0;
 Savestate_v1 *state=l7_checkpoints?&l7_checkpoints[l7_retry_checkpoint]:rec_state;
 Record_Restart(state,Savestate_Silent);
 int start=L7_Frame();
 RecInputData *track=rec_data.hmn_inputs[Record_GetSlot(0)];
 L7Input before=l7_entry>1?L7_FromRecord(&track->inputs[l7_entry-2]):(L7Input){0};
 l7_schedule(&l7,start,l7_entry,before);l7_scheduled_run=1;
 l7_consumed_buttons=L7_FromPad(PadGetMaster(stc_hmn_controller)).buttons;
 OSReport("L7 COUNTDOWN start=%d entry=%d lead=%d\n",start,l7_entry,l7_entry-start);
}
static void L7_RestorePreview(void){
 l7_preview_after_post=0;
 if(l7_selected_hit<0||!l7_ready)return;
 if(l7.mode==L7_ACTIVE){l7_finish(&l7,l7.frame,1);L7_ReportAttempt();}
 int hits[L7_MAX_EVENTS],n=0;
 for(int f=l7_selected_hit;f<=l7_combo_end_frame&&n<L7_MAX_EVENTS;f++)if(l7_direct_hit(&l7_reference[f-1].fighter[1],&l7_reference[f].fighter[1]))hits[n++]=f;
 if(!n)return;
 int center=clamp(l7_best_hits-1,0,n-1),lo=clamp(center-2,0,n-1),hi=clamp(center+2,0,n-1);
 int start=hits[lo]-30;if(start<0)start=0;
 int end=hits[hi]+30;if(hi+1<n&&end>=hits[hi+1])end=hits[hi+1]-1;if(end>l7_combo_end_frame)end=l7_combo_end_frame;
 int checkpoint=0;
 if(l7_checkpoints)for(int i=1;i<l7_checkpoint_count;i++)if(l7_checkpoints[i].is_exist&&l7_checkpoints[i].frame-rec_state->frame<=start)checkpoint=i;
 Record_Restart(l7_checkpoints?&l7_checkpoints[checkpoint]:rec_state,Savestate_Silent);
 l7_preview_start=start;l7_preview_end=end;l7_preview_active=1;
 OSReport("L7 PREVIEW from=%d to=%d center=%d start=%d end=%d restored=%d\n",lo+1,hi+1,center+1,start,end,L7_Frame());
}
static void L7_Update(void){
 if(!l7_length||!l7_reference)return;
 int enabled=L7_On(C_ENABLED);
 if(enabled!=l7_was_enabled){
  l7_was_enabled=enabled;l7_entry=-1;l7_retry_after_post=0;Record_LoadSavestate(rec_state);
  LabOptions_Record[OPTREC_LOOP].val=enabled?0:l7_saved_loop;
  LabOptions_Record[OPTREC_AUTORESTORE].val=enabled?0:l7_saved_restore;
  LabOptions_Record[OPTREC_PLAYBACK_COUNTER].val=enabled?0:l7_saved_counter;
 }
 l7_ui_ticks++;if(l7_good_job_ticks>0)l7_good_job_ticks--;
 HSD_Pad *pad=PadGetMaster(stc_hmn_controller);
 int up=!!(pad->held&HSD_BUTTON_DPAD_UP),left=!!(pad->held&HSD_BUTTON_DPAD_LEFT),down=!!(pad->held&HSD_BUTTON_DPAD_DOWN),right=!!(pad->held&HSD_BUTTON_DPAD_RIGHT);
 if(enabled&&Pause_CheckStatus(1)!=2){
  if(up&&!l7_up_held)l7_pending_command=3;
  else if(L7_On(C_SHORTCUT)&&left&&!l7_left_held&&l7_ready)l7_pending_command=1;
  else if(right&&!l7_right_held&&l7_ready)l7_pending_command=4;
  else if(!l7_preview_active&&down&&!l7_down_held&&l7_ready&&l7.mode==L7_DEMO)l7_pending_command=2;
  HSD_Pad *engine=PadGetEngine(stc_hmn_controller);
  int mask=HSD_BUTTON_DPAD_UP|HSD_BUTTON_DPAD_DOWN|HSD_BUTTON_DPAD_RIGHT|(L7_On(C_SHORTCUT)?HSD_BUTTON_DPAD_LEFT:0);
  engine->held&=~mask;engine->down&=~mask;
 }
 l7_up_held=up;l7_left_held=left;l7_down_held=down;l7_right_held=right;
 if(enabled&&l7_pending_command==4&&LabOptions_General[OPTGEN_FRAME].val){l7_pending_command=0;L7_RestorePreview();}
 /* Selection while frame-advance paused must not require one accidental input. */
 if(enabled&&l7_pending_command==2&&LabOptions_General[OPTGEN_FRAME].val&&l7_ready&&l7.mode==L7_DEMO){
  l7_pending_command=0;l7_entry=L7_ChooseEntry(L7_Frame());
  if(l7_entry>=0){L7_Retry();if(l7_retry_after_post)L7_RestoreCountdown();}
 }
 if(enabled&&l7_pending_command==3&&LabOptions_General[OPTGEN_FRAME].val){l7_pending_command=0;l7_retry_after_post=0;Record_Restart(rec_state,Savestate_Silent);OSReport("L7 WATCH\n");}
 if(enabled&&l7_pending_command==1&&LabOptions_General[OPTGEN_FRAME].val){
  l7_pending_command=0;
  if(!L7_On(C_HANDOFF)&&l7_ready&&L7_On(C_TAKEOVER)){L7_Retry();if(l7_retry_after_post)L7_RestoreCountdown();}
  else Record_LoadSavestate(rec_state);
 }
}
static RecInputs L7_ScriptInput(int frame){
 RecInputData*r=rec_data.hmn_inputs[Record_GetSlot(0)];
 int scenario=L7_On(C_TEST);
 if(scenario==4||frame<1||frame>r->num)return(RecInputs){0};
 /* Shift the aerial sequence as a whole for native DI-window acceptance tests. */
 if(scenario>=6){
  int target=l7_length;
  for(int i=0;i<l7_group_count;i++)if(l7_groups[i].aerial&&l7_groups[i].frame>l7_entry){target=l7_groups[i].frame;break;}
  if(l7_test_shift_from>0)target=l7_test_shift_from;
  int shift=l7_test_shift_amount?l7_test_shift_amount:scenario==6?3:scenario==7?-3:7;
  if(frame>=target-4){int source=clamp(frame-shift,1,r->num);return r->inputs[source-1];}
 }
 RecInputs input=r->inputs[frame-1];
 /* Change only the selected jump's onset. Keep its release and other channels. */
 if(l7_test_target>0){
  if((scenario==3||scenario==5)&&frame>=l7_test_target-3&&frame<l7_test_target){
   RecInputs *target=&r->inputs[l7_test_target-1];input.btn_x=target->btn_x;input.btn_y=target->btn_y;
  }
  if(scenario==2&&frame>=l7_test_target&&frame<l7_test_target+3){input.btn_x=0;input.btn_y=0;}
 }
 return input;
}
static int L7_Pre(void){
 if(!L7_On(C_ENABLED)||!l7_length||!l7_reference)return 0;
 l7.config.countdown=!L7_On(C_HANDOFF);l7.config.enabled=1;l7.config.loop=L7_On(C_LOOP);l7.config.takeover=L7_On(C_TAKEOVER)&&l7_ready;l7.config.buffer_frames=L7_On(C_BUFFER);l7.config.attempt_tracking=L7_On(C_TRACK);
 int frame=L7_Frame();
 if(l7_pending_command==4){l7_pending_command=0;if(l7_selected_hit>=0){l7_preview_after_post=1;return 1;}}
 if(l7_pending_command==3){l7_pending_command=0;l7_retry_after_post=0;l7_watch_after_post=1;return 1;}
 if(l7_pending_command==1){l7_pending_command=0;
  if(l7.config.countdown&&l7_ready&&l7.config.takeover){L7_Retry();if(l7_retry_after_post)return 1;}
  else {l7_reset_after_post=1;return 1;}
 }
 if(l7_preview_active){if(frame>=l7_preview_end){l7_preview_after_post=1;return 1;}return 0;}
 if(frame>l7_length){
  if(!l7_ready){l7_ready=1;L7_BuildEvents();L7_BuildGroups();L7_BuildDI();L7_BuildSequence(1,l7_length,0);OSReport("L7 REFERENCE events=%d target=%d\n",l7.event_count,l7_test_target);}
  if(l7.mode==L7_ACTIVE){
   L7Fighter cpu=L7_Fighter(1),human=L7_Fighter(0);L7Sample *target=&l7_reference[l7_length];
   l7.current.outcome_known=target->valid;
   l7.current.outcome_met=cpu.percent+0.02f>=target->fighter[1].percent && human.percent<=target->fighter[0].percent+0.02f && (!target->fighter[1].dead||cpu.dead);
   OSReport("L7 OUTCOME %d cpu=%.4f/%.4f human=%.4f/%.4f\n",l7.current.outcome_met,cpu.percent,target->fighter[1].percent,human.percent,target->fighter[0].percent);
   l7_finish(&l7,frame,0);L7_ReportAttempt();
  }
  if(l7.config.loop && frame>=l7_length+CFG_RESET_DELAY){l7_loop++;OSReport("L7 LOOP %d errors=%d\n",l7_loop,l7_reference_errors);if(l7.config.countdown&&l7_scheduled_run&&l7_entry>=0&&l7.config.takeover)L7_Retry();else l7_reset_after_post=1;return 1;}
 }
 RecInputs effective=L7_RecordPad(PadGetMaster(stc_hmn_controller));
 int prior_mode=l7.mode;
 if(!L7_On(C_TEST)){
  l7_consumed_buttons&=L7_FromRecord(&effective).buttons;
  if(l7.mode==L7_ACTIVE)effective=L7_FilterRequest(effective);
 }
 L7Input input=L7_FromRecord(&effective);L7Fighter fighter=L7_Fighter(0);
 if(L7_On(C_TEST)&&l7_ready){
  if(frame==CFG_TEST_REQUEST&&l7.mode==L7_DEMO)input=(L7Input){.buttons=L7_SELECT};
  else if(l7.mode==L7_ACTIVE||(l7.mode==L7_COUNTDOWN&&frame>=l7_entry)){RecInputs scripted=L7_ScriptInput(frame);input=L7_FromRecord(&scripted);effective=scripted;}
  else input=(L7Input){0};
 }
 if(l7_pending_command==2&&l7_ready){l7_request(&l7,frame,fighter.state);l7_pending_command=0;}
 if(l7.mode==L7_ACTIVE)input=L7_Assist(frame,effective);else l7_override=0;
 if(l7.mode==L7_COUNTDOWN){
  int remaining=l7_entry-frame,lead=L7_Lead();
  int beat=l7_countdown_beat(remaining,lead);
  if(beat>=0&&beat!=l7_last_beat){if(L7_On(C_AUDIO))SFX_PlayCommon(1);OSReport("L7 BEAT %d frame=%d\n",beat,frame);l7_last_beat=beat;}
  if(!L7_On(C_TEST)){
   effective=L7_FilterRequest(effective);input=L7_FromRecord(&effective);
  }
 }
 fighter.event_mask=L7_EventMask(fighter.state,input);
 int matched=l7.current.matched;
 l7_tick(&l7,frame,fighter,input);
 if(l7_buffer_applied&&l7.current.matched>matched&&l7_buffer_early>l7.config.tolerance){
  l7.current.buffer_saves++;l7.current.buffer_kind=l7_buffer_kind;l7.current.buffer_delta=-l7_buffer_early;
  OSReport("L7 BUFFER frame=%d kind=%d delta=%d\n",frame,l7_buffer_kind,-l7_buffer_early);
 }
 l7_buffer_applied=0;
 if(l7.mode==L7_PENDING&&l7.config.countdown){
  l7_entry=L7_ChooseEntry(frame);if(l7_entry<0){l7.mode=L7_DEMO;l7.request_armed=0;return 0;}L7_Retry();return 1;
 }
 if(prior_mode==L7_DEMO&&l7.mode!=L7_DEMO&&!L7_On(C_TEST))l7_consumed_buttons=input.buttons;
 if(l7.mode==L7_ACTIVE&&!L7_On(C_TEST)&&l7_consumed_buttons&&frame!=l7.handoff_frame){l7_override=1;l7_effective=effective;}
 if(l7.mode==L7_ACTIVE&&prior_mode!=L7_ACTIVE){
  if(!l7.config.countdown){int hit=-1;l7_select_combo(&l7_reference[0].fighter[1],sizeof(L7Sample),l7_length+1,frame,CFG_HIT_WINDOW,&hit);l7_combo_end_frame=hit>=0?l7_combo_end(&l7_reference[0].fighter[1],sizeof(L7Sample),l7_length+1,hit):l7_length;l7_combo_total=l7_combo_hits(&l7_reference[0].fighter[1],sizeof(L7Sample),frame,l7_combo_end_frame);}
  l7_progress=(L7Progress){.total=l7_combo_total};l7_progress_previous=L7_Fighter(1);
  l7_di_previous=l7_progress_previous;l7_di_applied=0;l7_di_reset(&l7_di,l7_di_hits,l7_di_count,frame);
  L7_DiagnosticReset(frame);
 }
 if(l7.mode==L7_ACTIVE)l7.current.assisted=L7_On(C_BUFFER)>0||L7_On(C_SPEED)>0||L7_On(C_DI_BUFFER)>0;
 if(l7.mode!=l7_last_mode){OSReport("L7 MODE %d frame=%d state=%d\n",l7.mode,frame,fighter.state);l7_last_mode=l7.mode;}
 if(l7.mode==L7_ACTIVE||l7.mode==L7_REVIEW){stc_playback_cancelled_hmn=1;((FighterData*)Fighter_GetGObj(0)->userdata)->pad_index=stc_hmn_controller;}
 return 0;
}
/* Called after stock input playback, before the engine samples fighter inputs. */
static void L7_AfterInputs(void){
 if(!L7_On(C_ENABLED)||!l7_length)return;
 int frame=L7_Frame();
 if(l7.mode==L7_ACTIVE||l7.mode==L7_REVIEW){
  FighterData *human=Fighter_GetGObj(0)->userdata;HSD_Pad *pad=PadGetEngine(human->pad_index);
  int mask=HSD_BUTTON_DPAD_UP|HSD_BUTTON_DPAD_DOWN|HSD_BUTTON_DPAD_RIGHT|(L7_On(C_SHORTCUT)?HSD_BUTTON_DPAD_LEFT:0);pad->held&=~mask;pad->down&=~mask;
 }
 if(frame>=1&&frame<=l7_rng_count)**stc_rng_seed=((u32*)rec_data.hmn_inputs[5]->inputs)[frame-1];
 if(l7.mode==L7_ACTIVE && frame==l7.handoff_frame){RecInputs input=L7_FilterRequest(L7_RecordPad(PadGetMaster(stc_hmn_controller)));Record_SetInputs(Fighter_GetGObj(0),&input,0);}
 if(l7.mode==L7_ACTIVE && L7_On(C_TEST)){RecInputs input=L7_ScriptInput(frame);Record_SetInputs(Fighter_GetGObj(0),&input,0);}
 if(l7.mode==L7_ACTIVE && l7_override)Record_SetInputs(Fighter_GetGObj(0),&l7_effective,0);
 if(l7.mode==L7_ACTIVE&&l7_ready&&L7_On(C_OPPONENT)==0&&L7_On(C_DI_BUFFER)>0){
  FighterData *cpu=Fighter_GetGObj(1)->userdata;
  int sample=l7_di_sample(&l7_di,l7_di_hits,l7_di_count,frame,cpu->flags.hitlag?cpu->dmg.hitlag_frames:0);
  RecInputData *track=rec_data.cpu_inputs[Record_GetSlot(1)];
  if(sample>=1&&sample<=track->num&&frame>=1&&frame<=track->num){
   RecInputs input=track->inputs[frame-1],*reference=&track->inputs[sample-1];
   input.stickX=reference->stickX;input.stickY=reference->stickY;
   input.substickX=reference->substickX;input.substickY=reference->substickY;
   Record_SetInputs(Fighter_GetGObj(1),&input,0);l7_di_applied=1;
   if(L7_On(C_TELEMETRY))OSReport("L7 DI APPLY frame=%d sample=%d remaining=%.0f x=%d y=%d cx=%d cy=%d\n",frame,sample,cpu->dmg.hitlag_frames,input.stickX,input.stickY,input.substickX,input.substickY);
  }else if(l7_di_applied){
   OSReport("L7 DI RELEASE frame=%d kx=%.6f ky=%.6f\n",frame,cpu->phys.kb_vel.X,cpu->phys.kb_vel.Y);l7_di_applied=0;
  }
  L7Fighter live=L7_Fighter(1);
  int tech=l7_tech_response(&l7_di,l7_di_hits,l7_di_count,frame,&live);
  if(tech>=0&&frame>=1&&frame<=track->num){
   L7DIHit *hit=&l7_di_hits[tech];RecInputs input=track->inputs[frame-1];
   /* Use CE's CPULOGIC_FORCE_TECH mechanism, not an action-state transition.
    * The engine still decides whether/when the fighter reaches a surface.
    * Neutralize trigger edges so normal sampling cannot create a new lockout. */
   input.btn_L=input.btn_R=input.btn_Z=0;input.trigger=0;
   input.stickX=hit->tech_x;input.stickY=0;
   cpu->input.since_rapid_lr=-1;cpu->input.timer_lstick_smash_x=-1;
   cpu->input.timer_LR=l7_di.tech_wait_bounce||hit->tech_state==183||hit->tech_state==191?-1:0;
   if(hit->tech_state==202||hit->tech_state==203){
    input.btn_x=input.btn_y=0;
    input.stickY=hit->tech_state==203?80:0;
    cpu->input.timer_xy=hit->tech_state==203?0:-1;
   }
   Record_SetInputs(Fighter_GetGObj(1),&input,0);
   if(L7_On(C_TELEMETRY))OSReport("L7 TECH APPLY frame=%d hit=%d state=%d x=%d until=%d waiting_bounce=%d\n",frame,hit->frame,hit->tech_state,hit->tech_x,l7_di.tech_until,l7_di.tech_wait_bounce!=0);
  }
 }else {l7_di.matched=l7_di.tech_matched=-1;l7_di_applied=0;}
 if(l7.mode==L7_ACTIVE&&L7_On(C_OPPONENT)==2){
  FighterData*f=Fighter_GetGObj(1)->userdata;
  if(f->flags.hitlag){RecInputs neutral={0};Record_SetInputs(Fighter_GetGObj(1),&neutral,0);}
 }
 if(l7.mode==L7_ACTIVE&&L7_On(C_OPPONENT)==1 && l7_ready){
  /* Match hit ordinal, then sample the original track relative to hitlag onset. */
  static int ordinal,previous_hitlag,onset,ref_onset;
  FighterData*f=Fighter_GetGObj(1)->userdata;
  if(frame==l7.handoff_frame){ordinal=0;ref_onset=0;for(int i=1;i<frame&&i<=l7_length;i++)if(l7_reference[i].fighter[1].hitlag&&!l7_reference[i-1].fighter[1].hitlag){ordinal++;ref_onset=i+1;}onset=ref_onset;previous_hitlag=f->flags.hitlag;}
  if(f->flags.hitlag&&!previous_hitlag){ordinal++;onset=frame;int n=0;ref_onset=0;for(int i=1;i<=l7_length;i++)if(l7_reference[i].fighter[1].hitlag&&!l7_reference[i-1].fighter[1].hitlag && ++n==ordinal){ref_onset=i+1;break;}}
  previous_hitlag=f->flags.hitlag;
  if(f->flags.hitlag&&ref_onset){int index=ref_onset+frame-onset-1;RecInputData*r=rec_data.cpu_inputs[Record_GetSlot(1)];if(index>=0&&index<r->num)Record_SetInputs(Fighter_GetGObj(1),&r->inputs[index],0);}
 }
}
static void L7_Post(void){
 if(l7_preview_after_post){L7_RestorePreview();return;}
 if(l7_watch_after_post){l7_watch_after_post=0;Record_Restart(rec_state,Savestate_Silent);OSReport("L7 WATCH\n");return;}
 if(l7_retry_after_post){L7_RestoreCountdown();return;}
 if(l7_reset_after_post){l7_reset_after_post=0;Record_LoadSavestate(rec_state);return;}
 if(!L7_On(C_ENABLED)||!rec_state||!rec_state->is_exist||!l7_length||!l7_reference)return;
 int frame=L7_Frame();if(frame==l7_last_post||frame<0||frame>=L7_FRAMES)return;l7_last_post=frame;
 if(!l7_ready&&l7_checkpoints&&frame>=60&&frame/60<l7_checkpoint_count&&!l7_checkpoints[frame/60].is_exist&&!(*stc_gobj_lookup)[MATCHPLINK_ITEM]){
  int saved=event_vars->Savestate_Save_v1(&l7_checkpoints[frame/60],Savestate_Silent|Savestate_Checks);
  if(saved)OSReport("L7 CHECKPOINT frame=%d\n",frame);
 }
 L7_DiagnosticTick(frame);
 for(int p=0;p<2;p++){
  L7Fighter f=L7_Fighter(p);
  if(p==1){
   FighterData *sdi_cpu=Fighter_GetGObj(1)->userdata;
   if(sdi_cpu->cb.EveryHitlag==(void*)0x8008e4f0)sdi_cpu->cb.EveryHitlag=L7_SDIThink;
   if(sdi_cpu->cb.ExitHitlag==(void*)0x8008e714)sdi_cpu->cb.ExitHitlag=L7_DIExit;
   if(!l7_ready&&frame>0&&f.percent>l7_reference[frame-1].fighter[1].percent+.01f)OSReport("L7 DAMAGE frame=%d direct=%d instance=%d source=%x attacker=%x kind=%d\n",frame,f.direct_hit,f.hit_instance,(u32)((FighterData*)Fighter_GetGObj(1)->userdata)->dmg.hit_log.source,(u32)Fighter_GetGObj(0),L7_Fighter(0).attack_kind);
   if(l7.mode==L7_ACTIVE){
    if(l7_direct_hit(&l7_di_previous,&f)&&L7_On(C_OPPONENT)==0&&L7_On(C_DI_BUFFER)>0){
     FighterData *cpu=Fighter_GetGObj(1)->userdata;
     OSReport("L7 HIT PHYS frame=%d before_ground=%d before_state=%d ground=%d kx=%.6f ky=%.6f\n",frame,l7_di_previous.grounded,l7_di_previous.state,f.grounded,cpu->phys.kb_vel.X,cpu->phys.kb_vel.Y);
     int matched=l7_di_match(&l7_di,l7_di_hits,l7_di_count,frame,L7_Fighter(0).attack_kind,L7_On(C_DI_BUFFER));
     if(matched>=0&&l7_di_hits[matched].tech_bounce&&!l7_di_previous.grounded){
      Vec3 surface,normal;int line,kind;
      float x=cpu->phys.pos.X,y=cpu->phys.pos.Y;
      /* Query the live surface; stale collision indices can survive a ledge
       * departure. No position snap or forced action-state transition. */
      int at_floor=GrColl_RaycastGround(&surface,&line,&kind,&normal,-1,-1,-1,0,x,y+.5f,x,y-.5f,0);
      float old_y=cpu->phys.kb_vel.Y;
      int rebound=l7_tech_prepare(&l7_di,l7_di_hits,l7_di_count,&l7_di_previous,at_floor,&cpu->phys.kb_vel.Y,(*stc_ftcommon)->x1ec);
      OSReport("L7 TECH LAUNCH frame=%d hit=%d rebound=%d at_floor=%d ky=%.6f/%.6f\n",frame,l7_di_hits[matched].frame,rebound,at_floor,old_y,cpu->phys.kb_vel.Y);
     }
     if(matched>=0)OSReport("L7 DI MATCH frame=%d reference=%d delta=%d window=%d kind=%d\n",frame,l7_di_hits[matched].frame,frame-l7_di_hits[matched].frame,L7_On(C_DI_BUFFER),L7_Fighter(0).attack_kind);
     else OSReport("L7 DI MISS frame=%d window=%d kind=%d\n",frame,L7_On(C_DI_BUFFER),L7_Fighter(0).attack_kind);
    }
    if(!f.direct_hit&&f.percent>l7_di_previous.percent+.01f)l7_di.matched=l7_di.tech_matched=-1;
    if(l7_di.tech_matched>=0&&l7_tech_outcome(f.state)){
     L7DIHit *hit=&l7_di_hits[l7_di.tech_matched];
     int direction=(f.state==200||f.state==201)?(f.facing<0?-80:80)*(f.state==200?1:-1):0;
     OSReport("L7 TECH RESULT frame=%d hit=%d expected=%d actual=%d direction=%d\n",frame,hit->frame,hit->tech_state,f.state,direction);
     l7_di.tech_matched=-1;
    }
    l7_di_previous=f;
   }
   if(l7.mode==L7_ACTIVE){int hits=l7_progress.hits;l7_progress_tick(&l7_progress,&l7_progress_previous,&f);l7_progress_previous=f;if(hits!=l7_progress.hits)OSReport("L7 HIT frame=%d count=%d total=%d\n",frame,l7_progress.hits,l7_progress.total);
    if(!l7_progress.started&&frame>l7_selected_hit+45)l7_progress.closed=1;
    if(l7_progress.hits==l7_progress.total||l7_progress.closed||frame>=l7_combo_end_frame)if(L7_On(C_TRACK))L7_SaveProgress();
   }
  }
  if(L7_On(C_TELEMETRY))OSReport("L7 F %d %d %d %.6f %.6f %.6f %.6f\n",event_vars->game_timer,p,f.state,f.animation,f.x,f.y,f.percent);
  if(!l7_ready && l7.mode==L7_DEMO)l7_reference[frame].fighter[p]=f;
  else if(l7_reference[frame].valid && (l7.mode==L7_DEMO || l7.mode==L7_COUNTDOWN || (L7_On(C_TEST)==1&&L7_On(C_OPPONENT)==0))){
   L7Fighter*r=&l7_reference[frame].fighter[p];
   if(f.state!=r->state||fabs(f.x-r->x)>0.02||fabs(f.y-r->y)>0.02||fabs(f.percent-r->percent)>0.02){l7_reference_errors++;if(l7_reference_errors<=5)OSReport("L7 DIVERGE loop=%d frame=%d player=%d state=%d/%d x=%.4f/%.4f y=%.4f/%.4f percent=%.4f/%.4f\n",l7_loop,frame,p,f.state,r->state,f.x,r->x,f.y,r->y,f.percent,r->percent);}
  }
 }
 if(!l7_ready&&l7.mode==L7_DEMO){
  FighterData*f=Fighter_GetGObj(0)->userdata;
  for(int h=0;h<4;h++)l7_reference[frame].hit[h]=(L7Hit){f->hitbox[h].pos.X,f->hitbox[h].pos.Y,f->hitbox[h].size,f->hitbox[h].active};
  l7_reference[frame].valid=1;
 }
 if(L7_On(C_AUDIO))for(int i=0;i<l7.event_count;i++)if(l7.events[i].frame==frame && (l7.events[i].kind==L7_JUMP_EVENT||l7.events[i].kind==L7_ATTACK_EVENT)){SFX_PlayCommon(1);break;}
}
#include "icons.inc"
static void L7_Draw(GOBJ*g,int pass){
 if(pass!=2)return;
 l7_text_used=0;for(int i=0;i<16;i++)if(l7_text_cache[i])l7_text_cache[i]->hidden=true;
 if(!L7_On(C_ENABLED)||!l7_length)return;
 int frame=L7_Frame();char text[180];
 Rect box={-30,16,25,3};
 const char*mode=l7_preview_active?"Sequence preview":l7_progress_reported?(l7_progress.hits==l7_progress.total?"Combo complete":"Combo dropped"):l7.mode==L7_ACTIVE?"Your turn":l7.mode==L7_COUNTDOWN?"Get ready":l7.mode==L7_PENDING?"Finishing action":l7.mode==L7_REVIEW?"Attempt finished":!l7_ready?"Loading demonstration":"Watch then join";
 sprintf(text,"%s%s",mode,(L7_On(C_BUFFER)||(L7_On(C_OPPONENT)==0&&L7_On(C_DI_BUFFER)>0))?" - Assisted":"");L7_Text(text,&box,0.34f);
 int preparing=l7_scheduled_run&&!l7_preview_active&&!l7_progress_reported&&l7.mode==L7_COUNTDOWN;
 int show_sequence=L7_On(C_TIMELINE)&&((preparing&&l7_entry-frame>L7_Lead())||(l7_preview_active&&frame>=l7_preview_start));
 if(!l7_preview_active&&l7_scheduled_run&&!l7_progress_reported&&(l7.mode==L7_COUNTDOWN||(l7.mode==L7_ACTIVE&&frame<l7_entry+30))){
  int left=l7_entry-frame,lead=L7_Lead();Rect b={-8,2,30,4};
  if(left>lead)sprintf(text,"Get ready");else if(left>0)sprintf(text,"%d",l7_countdown_beat(left,lead));else sprintf(text,"Go");
  L7_Text(text,&b,0.8f);
 }
 int first=l7_preview_active?L7_SequenceCursor(frame):0;
 int sequence_end=l7_sequence_count;
 if(l7_preview_active)while(sequence_end>first&&l7_sequence[sequence_end-1].frame>l7_preview_end)sequence_end--;
 /* Technique names lead the central queue; small controller artwork is
  * secondary. Each label stays within the native SIS string limit. */
 if(show_sequence){
  for(int i=0;i<3&&first+i<sequence_end;i++){
   Rect label={-15+i*15,-3,14,3};
   L7_Text(l7_sequence[first+i].label,&label,0.34f);
   L7_DrawCue(&l7_sequence[first+i],-8+i*15,-4,1.1f,8);
   if(i<2&&first+i+1<sequence_end)L7_IconDirection(-1.5f+i*15,-1.5f,1,0,1.5f);
  }
 }
 int diagnosis=L7_On(C_DIAGNOSTICS)&&l7_diagnosis.code&&(l7.mode==L7_ACTIVE||l7.mode==L7_REVIEW)&&!(l7_progress_reported&&l7_progress.hits==l7_progress.total);
 if(diagnosis){char advice[128];L7_DiagnosticText(text,advice);Rect title={7,6,26,3},action={7,3.8f,26,3};L7_Text(text,&title,0.29f);L7_Text(advice,&action,0.28f);}
 if(!diagnosis&&L7_On(C_FEEDBACK)&&!l7_progress_reported&&l7.mode==L7_ACTIVE&&l7.first_error<0){Rect b={7,6,26,3};sprintf(text,"On time  %d / %d",l7.current.matched,l7.current.total);L7_Text(text,&b,0.34f);}
 if(!diagnosis&&L7_On(C_FEEDBACK)&&!l7_progress_reported&&l7.mode==L7_ACTIVE&&l7.first_error>=0){Rect b={7,6,26,3};sprintf(text,"%s: %s %df",l7_kind_name(l7.current.first_kind),l7.current.first_error==L7_EARLY?"early":l7.current.first_error==L7_LATE?"late":"missed",absolute(l7.current.first_delta));L7_Text(text,&b,0.34f);}
 if(L7_On(C_TRACK)&&l7_combo_total>0){
  Rect best={7,16,26,3},latest={7,13.5f,26,3},good={7,11,26,3};
  sprintf(text,"Best %d%% (%d/%d hits)",100*l7_best_hits/l7_combo_total,l7_best_hits,l7_combo_total);L7_Text(text,&best,0.28f);
  if(l7_latest_hits<0)sprintf(text,"Latest --");else sprintf(text,"Latest %d%% (%d/%d hits)",100*l7_latest_hits/l7_combo_total,l7_latest_hits,l7_combo_total);
  L7_Text(text,&latest,0.28f);
  if(l7_progress_reported&&l7_good_job_ticks>0&&(l7_ui_ticks/12)%2==0)L7_Text("Good job!",&good,0.4f);
 }
 if(!diagnosis&&L7_On(C_TRACK)&&l7.mode==L7_REVIEW&&l7.attempts_count){
  L7Attempt *last=&l7.attempts[(l7.attempts_count-1)%L7_MAX_ATTEMPTS];Rect b={7,6,26,3};
  if(last->completed)sprintf(text,"Completed%s",last->assisted?" - Assisted":"");
  else if(last->aborted)sprintf(text,"Attempt reset");
  else if(last->first_kind)sprintf(text,"%s: %s %df",l7_kind_name(last->first_kind),last->first_error==L7_EARLY?"early":last->first_error==L7_LATE?"late":"missed",absolute(last->first_delta));
  else sprintf(text,"Combo outcome differed");
  L7_Text(text,&b,0.34f);
 }
 if(L7_On(C_FEEDBACK)&&l7.current.buffer_saves){
  Rect b={7,2,26,3};sprintf(text,"%s: early %df",l7_kind_name(l7.current.buffer_kind),-l7.current.buffer_delta);L7_Text(text,&b,0.3f);
  Rect c={7,-0.2f,26,3};L7_Text("(saved by buffer)",&c,0.3f);
 }
 Rect h1={-31,-19,28,2},h2={-31,-21.3f,28,2},h3={-36,-14.4f,28,2},h4={-31,-16.7f,28,2};
 if(L7_On(C_SHORTCUT))L7_Text("D-pad Left: Retry",&h1,0.27f);
 L7_Text("D-pad Up: Watch",&h2,0.27f);
 Rect preview={-31,-12.1f,28,2};L7_Text("D-pad Right: Preview",&preview,0.27f);
 if(l7.mode==L7_DEMO){L7_Text("D-pad Down",&h3,0.25f);L7_Text(l7_selection_failed?"No nearby direct hit":"Choose playback combo hit",&h4,0.24f);}
 if(!L7_On(C_DIAGNOSTICS)&&!show_sequence&&L7_On(C_TRACK)&&l7.attempts_count>=3&&l7.current.first_kind){int count=0;for(int i=0;i<l7.attempts_stored;i++)if(l7.attempts[i].first_kind==l7.current.first_kind&&!l7.attempts[i].aborted)count++;if(count>=3){Rect b={-30,-13,60,3};static const char*drills[]={"Practice movement","Drill jump timing","Drill aerial timing","Drill shine cancels","Drill landing cancels","Drill dash starts","Drill fast falls"};L7_Text(drills[l7.current.first_kind],&b,0.36f);}}
}
static void L7_Ghost(GOBJ*g,int pass){
 if(pass!=2||!L7_On(C_ENABLED)||!l7_ready||!l7_reference)return;
 int frame=L7_Frame();if(frame<0||frame>l7_length)return;
 GXColor color={0,120,150,150};
 if(L7_On(C_GHOST)){
  int start=frame>30?frame-30:0;int end=frame+30<l7_length?frame+30:l7_length;
  event_vars->GFX_Start(end-start+1,(GFX_Params){.shape=GX_LINESTRIP,.size=18});
  for(int i=start;i<=end;i++)GFX_AddVtx(l7_reference[i].fighter[0].x,l7_reference[i].fighter[0].y,1,color);
  L7Fighter*f=&l7_reference[frame].fighter[0];event_vars->GFX_Start(4,(GFX_Params){.shape=GX_LINES,.size=24});GFX_AddVtx(f->x-2,f->y,1,color);GFX_AddVtx(f->x+2,f->y,1,color);GFX_AddVtx(f->x,f->y-2,1,color);GFX_AddVtx(f->x,f->y+2,1,color);
 }
 if(L7_On(C_HITBOX))for(int h=0;h<4;h++){
  L7Hit*b=&l7_reference[frame].hit[h];if(!b->active)continue;
  event_vars->GFX_Start(17,(GFX_Params){.shape=GX_LINESTRIP,.size=12});
  for(int j=0;j<=16;j++){float a=j*0.3926990817f;GFX_AddVtx(b->x+cos(a)*b->size,b->y+sin(a)*b->size,1,(GXColor){130,30,130,150});}
 }
}
