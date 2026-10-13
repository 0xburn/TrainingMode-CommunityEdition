#ifndef L7_COACH_H
#define L7_COACH_H
#include <stdint.h>
#define L7_MAX_EVENTS 512
#define L7_MAX_ATTEMPTS 64
/* Portable controller bits; adapter translates the native controller layout. */
enum { L7_A=1, L7_B=2, L7_JUMP=4, L7_SHIELD=8, L7_SELECT=16 };
typedef struct { uint16_t buttons; int16_t x,y,cx,cy,trigger; } L7Input;
typedef struct { int state, actionable, hitlag, dead, event_mask, grounded, fastfall, lcancel, hit_instance, direct_hit, attack_instance, attack_kind; float animation,x,y,percent,facing; } L7Fighter;
enum { L7_JUMP_EVENT=1,L7_ATTACK_EVENT,L7_SPECIAL_EVENT,L7_SHIELD_EVENT,L7_DASH_EVENT,L7_FASTFALL_EVENT };
enum { L7_DEMO=0,L7_PENDING,L7_ACTIVE,L7_REVIEW,L7_COUNTDOWN };
enum { L7_ON_TIME=0,L7_EARLY,L7_LATE,L7_MISSING,L7_WRONG,L7_ABORTED };
typedef struct { int frame,kind,action,group; } L7Event;
typedef struct {
    int enabled,loop,takeover,tolerance,match_window,buffer_frames,reset_delay;
    int request_deadzone,pending_timeout,attempt_tracking,countdown;
} L7Config;
typedef struct {
    int sequence,start,end,first_kind,first_frame,first_error,first_delta;
    int matched,total,assisted,completed,aborted,outcome_known,outcome_met,buffer_saves,buffer_kind,buffer_delta;
} L7Attempt;
typedef struct {
    L7Config config;
    int mode,frame,paused,pending_frame,pending_state,handoff_frame;
    int entry_frame,reset_requested,request_armed,last_frame,review_until;
    int event_count,event_cursor,first_error,feedback_kind,feedback_delta;
    int attempts_count,attempts_stored,successes,streak,best_streak;
    L7Input previous;
    L7Event events[L7_MAX_EVENTS];
    L7Attempt current,attempts[L7_MAX_ATTEMPTS];
} L7Coach;
typedef struct { int hits,total,gap,started,closed; } L7Progress;
typedef struct { int frame,last_input,kind,tech_frame,tech_state,tech_x,tech_bounce; } L7DIHit;
typedef struct { int cursor,matched,onset,tech_matched,tech_until,tech_wait_bounce; float tech_start_y; } L7DIState;
void l7_di_reset(L7DIState*,const L7DIHit*,int,int);
int l7_di_match(L7DIState*,const L7DIHit*,int,int,int,int);
int l7_di_sample(L7DIState*,const L7DIHit*,int,int,float);
int l7_sdi_sample(const L7DIState*,const L7DIHit*,int,int);
int l7_di_launch(float,float,float,float,float,float*,float*);
int l7_tech_outcome(int);
int l7_tech_prepare(L7DIState*,const L7DIHit*,int,const L7Fighter*,int,float*,float);
void l7_tech_reference(L7DIHit*,const L7Fighter*,int,int);
int l7_tech_response(L7DIState*,const L7DIHit*,int,int,const L7Fighter*);
int l7_direct_hit(const L7Fighter*,const L7Fighter*);
int l7_combo_end(const L7Fighter*,int,int,int);
int l7_combo_hits(const L7Fighter*,int,int,int);
void l7_progress_tick(L7Progress*,const L7Fighter*,const L7Fighter*);
int l7_progress_percent(const L7Progress*);
int l7_select_combo(const L7Fighter*,int,int,int,int,int*);
int l7_countdown_beat(int,int);
void l7_init(L7Coach*,L7Config);
void l7_reset(L7Coach*,int);
void l7_request(L7Coach*,int,int);
void l7_schedule(L7Coach*,int,int,L7Input);
void l7_pause(L7Coach*,int);
int l7_input_kinds(L7Input,L7Input);
int l7_deliberate(L7Input,int);
void l7_tick(L7Coach*,int,L7Fighter,L7Input);
void l7_finish(L7Coach*,int,int);
void l7_add_event(L7Coach*,L7Event);
void l7_hud_text(char out[128],const char *text);
const char *l7_kind_name(int);
const char *l7_drill(int);
#endif
