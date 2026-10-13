#include "coach.h"
#include <assert.h>
#include <stdio.h>
static L7Config config={1,1,1,1,12,0,30,40,180,1,0};
static L7Fighter free_state={.state=14,.actionable=1};
static L7Input neutral={0};
static L7Coach c;
static void setup(void){l7_init(&c,config);l7_add_event(&c,(L7Event){.frame=10,.kind=L7_JUMP_EVENT});l7_add_event(&c,(L7Event){.frame=20,.kind=L7_ATTACK_EVENT});l7_add_event(&c,(L7Event){.frame=30,.kind=L7_SHIELD_EVENT});}
static void attempt(int delta){
 setup();l7_tick(&c,0,free_state,neutral);l7_tick(&c,1,free_state,(L7Input){.buttons=L7_B});assert(c.mode==L7_ACTIVE);
 for(int f=2;f<=45;f++){L7Input i={0};if(f==10)i.buttons=L7_JUMP;if(f==20+delta)i.buttons=L7_A;if(f==30)i.buttons=L7_SHIELD;l7_tick(&c,f,free_state,i);}l7_finish(&c,45,0);
}
int main(void){
 char safe[128];l7_hud_text(safe,"Watch / jump in [Assisted]");
 for(int i=0;safe[i];i++)assert(safe[i]!='['&&safe[i]!=']'&&safe[i]!='/');
 l7_hud_text(safe,"1 a 2 b 3 c 4 d 5 e 6 f 7 g 8 h 9 i 0 j 1 k 2 l 3 m 4 n 5 o 6 p");
 int bytes=0,kern=0;for(int i=0;safe[i];i++){int compact=safe[i]==' '||(safe[i]>='0'&&safe[i]<='9');bytes+=compact?(kern?2:7):3;kern=compact;}assert(bytes<=110);

 attempt(0);assert(c.current.completed&&c.successes==1&&c.current.matched==3);
 attempt(-3);assert(!c.current.completed&&c.current.first_error==L7_EARLY&&c.current.first_delta==-3);
 attempt(3);assert(!c.current.completed&&c.current.first_error==L7_LATE&&c.current.first_delta==3);
 setup();l7_tick(&c,0,free_state,neutral);L7Fighter attack={.state=65};l7_tick(&c,1,attack,(L7Input){.buttons=L7_B});assert(c.mode==L7_PENDING);
 for(int f=2;f<6;f++)l7_tick(&c,f,attack,neutral);assert(c.mode==L7_PENDING);
 l7_tick(&c,6,free_state,neutral);assert(c.mode==L7_ACTIVE&&c.handoff_frame==6);
 l7_pause(&c,1);l7_tick(&c,60,free_state,neutral);assert(c.frame==6);l7_pause(&c,0);
 l7_reset(&c,0);assert(c.mode==L7_DEMO&&c.attempts_count==1&&c.attempts[0].aborted);
 /* Held input during reset cannot immediately take over the next loop. */
 l7_tick(&c,0,free_state,(L7Input){.buttons=L7_A});assert(c.mode==L7_DEMO);
 l7_tick(&c,1,free_state,neutral);l7_tick(&c,2,free_state,(L7Input){.buttons=L7_B});assert(c.mode==L7_ACTIVE);
 /* Disabling takeover cancels a pending request. */
 setup();l7_request(&c,0,65);c.config.takeover=0;l7_tick(&c,1,free_state,neutral);assert(c.mode==L7_DEMO);
 /* Hitlag prevents handoff even when actionability was latched. */
 setup();l7_tick(&c,0,free_state,neutral);L7Fighter hitlag=free_state;hitlag.hitlag=1;l7_tick(&c,1,hitlag,(L7Input){.buttons=L7_B});assert(c.mode==L7_PENDING);
 l7_tick(&c,2,free_state,neutral);assert(c.mode==L7_ACTIVE);
 /* Pause, duplicate frames and history retention do not create attempts. */
 int frame=c.frame;l7_tick(&c,frame,free_state,(L7Input){.buttons=L7_JUMP});assert(c.current.matched==0);
 for(int i=0;i<80;i++){l7_reset(&c,0);l7_request(&c,0,14);l7_tick(&c,1,free_state,neutral);l7_finish(&c,2,1);}assert(c.attempts_stored==L7_MAX_ATTEMPTS);
 assert(l7_input_kinds((L7Input){.buttons=L7_JUMP},neutral)&(1<<L7_JUMP_EVENT));
 /* Scheduled handoff grades the first input on the Go frame, not a frame later. */
 setup();c.config.countdown=1;l7_tick(&c,0,free_state,neutral);
 l7_tick(&c,1,free_state,(L7Input){.buttons=L7_B});assert(c.mode==L7_DEMO);
 l7_tick(&c,2,free_state,(L7Input){.buttons=L7_A});assert(c.mode==L7_DEMO);
 l7_tick(&c,3,free_state,(L7Input){.buttons=L7_SELECT});assert(c.mode==L7_PENDING);
 l7_schedule(&c,0,10,neutral);
 for(int f=1;f<10;f++)l7_tick(&c,f,free_state,(L7Input){.buttons=L7_A});
 assert(c.mode==L7_COUNTDOWN&&c.attempts_count==0);
 l7_tick(&c,10,free_state,(L7Input){.buttons=L7_JUMP});
 assert(c.mode==L7_ACTIVE&&c.handoff_frame==10&&c.current.total==3&&c.current.matched==1);
 l7_tick(&c,11,free_state,neutral);l7_tick(&c,20,free_state,(L7Input){.buttons=L7_A});
 l7_tick(&c,21,free_state,neutral);l7_tick(&c,30,free_state,(L7Input){.buttons=L7_SHIELD});
 l7_finish(&c,35,0);assert(c.current.completed);
 l7_schedule(&c,0,10,neutral);l7_pause(&c,1);l7_tick(&c,10,free_state,neutral);assert(c.mode==L7_COUNTDOWN);
 l7_pause(&c,0);c.config.takeover=0;l7_tick(&c,10,free_state,neutral);assert(c.mode==L7_DEMO);
 L7Fighter reference[220]={0};for(int i=0;i<220;i++){reference[i].state=14;reference[i].direct_hit=1;reference[i].hit_instance=i<10?0:i<30?1:i<140?2:3;reference[i].percent=i<10?0:i<30?10:i<140?20:30;}
 for(int i=10;i<45;i++)reference[i].state=90;
 for(int i=140;i<160;i++)reference[i].state=90;
 int hit=-1;assert(l7_select_combo(reference,sizeof(*reference),220,32,30,&hit)==10&&hit==30);
 assert(l7_select_combo(reference,sizeof(*reference),220,141,30,&hit)==140&&hit==140);
 assert(l7_select_combo(reference,sizeof(*reference),220,100,10,&hit)==-1);
 assert(l7_countdown_beat(110,109)==-1&&l7_countdown_beat(109,109)==3);
 assert(l7_countdown_beat(72,109)==2&&l7_countdown_beat(36,109)==1&&l7_countdown_beat(0,109)==0);
 l7_hud_text(safe,"(saved by buffer)");assert((unsigned char)safe[0]==0x81&&safe[1]==0x69);
 reference[30].direct_hit=0;assert(l7_select_combo(reference,sizeof(*reference),220,32,30,&hit)==10&&hit==10);
 reference[10].direct_hit=0;assert(l7_select_combo(reference,sizeof(*reference),220,32,30,&hit)==-1);reference[10].direct_hit=1;
 reference[30].direct_hit=1;assert(l7_combo_hits(reference,sizeof(*reference),10,90)==2);
 assert(l7_combo_end(reference,sizeof(*reference),220,10)==90);
 L7Progress progress={.total=3};L7Fighter before={.state=14},after={.state=90,.direct_hit=0,.percent=3,.hit_instance=1,.hitlag=1};
 l7_progress_tick(&progress,&before,&after);assert(progress.hits==0);
 before=after;after.direct_hit=1;after.percent=12;after.hit_instance=2;l7_progress_tick(&progress,&before,&after);assert(progress.hits==1&&l7_progress_percent(&progress)==33);
 before=after;l7_progress_tick(&progress,&before,&after);assert(progress.hits==1);
 after.hitlag=0;after.percent++;l7_progress_tick(&progress,&before,&after);assert(progress.hits==1);
 after.state=14;after.hitlag=0;for(int i=0;i<46;i++)l7_progress_tick(&progress,&before,&after);assert(progress.closed);
 before=after;after.state=90;after.percent=22;after.hit_instance=3;l7_progress_tick(&progress,&before,&after);assert(progress.hits==1);
 l7_hud_text(safe,"Best 100% (5/5 hits) Good job!");int saw_percent=0,saw_bang=0;for(int i=0;safe[i];i++){saw_percent|=(unsigned char)safe[i]==0x93;saw_bang|=(unsigned char)safe[i]==0x81&&safe[i+1]==0x49;}assert(saw_percent&&saw_bang);
 puts("coach core: perfect, early, late, finish-action handoff, hitlag, pause, reset, held-input and bounded-history checks passed");
}
