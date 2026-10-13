#include "coach.h"
#ifndef GEKKO
#include <string.h>
#endif
static int absolute(int x){return x<0?-x:x;}
#include "selection.c"
#include "di_buffer.c"
#include "diagnostics.c"
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
void l7_init(L7Coach *c,L7Config cfg){
    memset(c,0,sizeof(*c));c->config=cfg;
    c->config.tolerance=clamp(cfg.tolerance,0,10);
    c->config.match_window=clamp(cfg.match_window,c->config.tolerance+1,60);
    c->config.buffer_frames=clamp(cfg.buffer_frames,0,6);
    c->config.request_deadzone=clamp(cfg.request_deadzone,20,75);
    c->config.pending_timeout=clamp(cfg.pending_timeout,30,600);
    c->last_frame=-1;c->first_error=-1;c->handoff_frame=-1;
}
int l7_deliberate(L7Input a,int dz){
    return a.buttons || absolute(a.x)>=dz || absolute(a.y)>=dz || absolute(a.cx)>=dz || absolute(a.cy)>=dz || a.trigger>=43;
}
int l7_input_kinds(L7Input a,L7Input b){
    int k=0;int edge=a.buttons&~b.buttons;
    if(edge&L7_JUMP)k|=1<<L7_JUMP_EVENT;
    if((edge&L7_A) || (absolute(a.cx)>=40&&absolute(b.cx)<40) || (absolute(a.cy)>=40&&absolute(b.cy)<40))k|=1<<L7_ATTACK_EVENT;
    if(edge&L7_B)k|=1<<L7_SPECIAL_EVENT;
    if((edge&L7_SHIELD)||(a.trigger>=43&&b.trigger<43))k|=1<<L7_SHIELD_EVENT;
    if(absolute(a.x)>=64 && (absolute(b.x)<64 || (a.x>0)!=(b.x>0)))k|=1<<L7_DASH_EVENT;
    if(a.y<=-53&&b.y>-53)k|=1<<L7_FASTFALL_EVENT;
    return k;
}
void l7_add_event(L7Coach*c,L7Event e){
    if(c->event_count>=L7_MAX_EVENTS)return;
    if(c->event_count && e.frame<c->events[c->event_count-1].frame)return;
    c->events[c->event_count++]=e;
}
static void fail(L7Coach*c,int reason,int delta,L7Event e){
    c->feedback_kind=reason;c->feedback_delta=delta;
    if(c->first_error<0){c->first_error=c->event_cursor;c->current.first_kind=e.kind;c->current.first_frame=e.frame;c->current.first_error=reason;c->current.first_delta=delta;}
}
void l7_request(L7Coach*c,int frame,int state){
    if(!c->config.enabled||!c->config.takeover||c->mode!=L7_DEMO)return;
    c->mode=L7_PENDING;c->pending_frame=frame;c->pending_state=state;
}
void l7_schedule(L7Coach*c,int start,int entry,L7Input before){
    l7_reset(c,start);c->mode=L7_COUNTDOWN;c->entry_frame=entry;c->previous=before;
}
void l7_pause(L7Coach*c,int paused){c->paused=!!paused;}
void l7_finish(L7Coach*c,int frame,int aborted){
    if(c->mode!=L7_ACTIVE)return;
    if(!aborted){
        while(c->event_cursor<c->event_count && c->events[c->event_cursor].frame<=frame){
            L7Event event=c->events[c->event_cursor];
            fail(c,L7_MISSING,frame-event.frame,event);c->event_cursor++;
        }
    }
    c->current.end=frame;c->current.aborted=aborted;
    c->current.completed=!aborted && c->current.total>0 && c->current.matched==c->current.total && c->first_error<0 && (!c->current.outcome_known||c->current.outcome_met);
    if(aborted && c->first_error<0)c->current.first_error=L7_ABORTED;
    if(c->config.attempt_tracking){
        c->current.sequence=++c->attempts_count;
        c->attempts[(c->attempts_count-1)%L7_MAX_ATTEMPTS]=c->current;
        if(c->attempts_stored<L7_MAX_ATTEMPTS)c->attempts_stored++;
        if(c->current.completed){c->successes++;c->streak++;if(c->streak>c->best_streak)c->best_streak=c->streak;}else c->streak=0;
    }
    c->mode=L7_REVIEW;c->review_until=frame+c->config.reset_delay;
}
void l7_reset(L7Coach*c,int frame){
    if(c->mode==L7_ACTIVE)l7_finish(c,c->frame,1);
    c->mode=L7_DEMO;c->frame=frame;c->last_frame=frame-1;c->handoff_frame=-1;
    c->first_error=-1;c->event_cursor=0;c->reset_requested=0;c->request_armed=0;
    c->previous=(L7Input){0};c->current=(L7Attempt){0};
}
void l7_tick(L7Coach*c,int frame,L7Fighter f,L7Input input){
    if(!c->config.enabled||c->paused||frame==c->last_frame)return;
    c->last_frame=frame;c->frame=frame;
    if((c->mode==L7_PENDING||c->mode==L7_COUNTDOWN)&&!c->config.takeover){c->mode=L7_DEMO;c->request_armed=0;}
    int active=l7_deliberate(input,c->config.request_deadzone);
    if(c->config.countdown?!(input.buttons&L7_SELECT):!active)c->request_armed=1;
    if(c->mode==L7_DEMO && c->request_armed && (c->config.countdown?!!(input.buttons&L7_SELECT):active)){l7_request(c,frame,f.state);c->request_armed=0;}
    if(c->mode==L7_PENDING && !c->config.countdown){
        /* Never cut hitlag/death; actionable comes from live engine mechanics. */
        if(!f.hitlag&&!f.dead&&f.actionable){
            c->mode=L7_ACTIVE;c->handoff_frame=frame;c->current=(L7Attempt){.start=frame,.assisted=c->config.buffer_frames>0};
            c->first_error=-1;c->event_cursor=0;
            while(c->event_cursor<c->event_count&&c->events[c->event_cursor].frame<=frame)c->event_cursor++;
            c->current.total=c->event_count-c->event_cursor;
            /* The request edge is consumed. Fresh edges begin on the next frame. */
            c->previous=input;return;
        }
        if(frame-c->pending_frame>=c->config.pending_timeout){c->mode=L7_DEMO;c->request_armed=0;}
    }
    if(c->mode==L7_COUNTDOWN){
        if(frame<c->entry_frame)return;
        c->mode=L7_ACTIVE;c->handoff_frame=frame;
        c->current=(L7Attempt){.start=frame,.assisted=c->config.buffer_frames>0};
        c->first_error=-1;c->event_cursor=0;
        while(c->event_cursor<c->event_count&&c->events[c->event_cursor].frame<frame)c->event_cursor++;
        c->current.total=c->event_count-c->event_cursor;
    }
    if(c->mode==L7_ACTIVE){
        int kinds=l7_input_kinds(input,c->previous);
        if(f.event_mask)kinds&=f.event_mask;
        while(c->event_cursor<c->event_count){
            L7Event e=c->events[c->event_cursor];int delta=frame-e.frame;
            if(delta>c->config.match_window){fail(c,L7_MISSING,delta,e);c->event_cursor++;continue;}
            if(delta < -c->config.match_window)break;
            if(kinds&(1<<e.kind)){
                c->current.matched++;
                c->feedback_delta=delta;c->feedback_kind=L7_ON_TIME;
                if(absolute(delta)>c->config.tolerance)fail(c,delta<0?L7_EARLY:L7_LATE,delta,e);
                kinds&=~(1<<e.kind);c->event_cursor++;continue;
            }
            break;
        }
    }
    c->previous=input;
}
const char*l7_kind_name(int k){switch(k){case L7_JUMP_EVENT:return "Jump";case L7_ATTACK_EVENT:return "Aerial / attack";case L7_SPECIAL_EVENT:return "Special";case L7_SHIELD_EVENT:return "L-cancel / air dodge";case L7_DASH_EVENT:return "Dash";case L7_FASTFALL_EVENT:return "Fast fall";default:return "Movement";}}
const char*l7_drill(int k){switch(k){case L7_JUMP_EVENT:return "Practice jump timing, then add the next action.";case L7_ATTACK_EVENT:return "Practice the aerial after a short hop.";case L7_SPECIAL_EVENT:return "Practice the special and its jump cancel.";case L7_SHIELD_EVENT:return "Practice landing cancels and air-dodge timing.";case L7_DASH_EVENT:return "Practice dash starts and direction changes.";case L7_FASTFALL_EVENT:return "Practice fast falls after the apex.";default:return "Repeat the first missed action in isolation.";}}

/* Melee's SIS encoder has a 128-byte output buffer and treats unsupported ASCII
 * as a two-byte Shift-JIS sequence. Bound the ENCODED size, not just strlen. */
void l7_hud_text(char out[128],const char *text){
 int used=0,n=0,kerning=0;
 for(int i=0;text[i]&&i<120;i++){
  char ch=text[i];
  if(ch=='('||ch==')'||ch=='%'||ch=='!'||ch=='/'){if(used+3>110)break;out[n++]=(char)0x81;out[n++]=ch=='('?0x69:ch==')'?0x6a:ch=='%'?(char)0x93:ch=='!'?0x49:0x5e;used+=3;kerning=0;continue;}
  int letter=(ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z');
  int compact=ch==' '||(ch>='0'&&ch<='9')||ch==':'||ch=='.';
  if(!letter&&!compact&&ch!='-'&&ch!=','&&ch!='\''&&ch!='"'){ch=' ';compact=1;}
  int cost=compact?(kerning?2:7):3;
  if(used+cost>110)break;
  used+=cost;kerning=compact;out[n++]=ch;
 }
 out[n]=0;
}
