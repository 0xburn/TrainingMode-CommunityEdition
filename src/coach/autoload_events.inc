/* Boot once from a recognized practice export, or retain normal navigation. */
#include "autoload_scan.inc"
#define L7_EVENT_CONTROL ((void (*)(GOBJ *))0x8024d864)
#define L7_EVENT_CONTROL_CALLBACK ((void *)0x8024d864)
#define L7_EVENT_GOBJ (*(GOBJ **)0x804d6c60)
#define L7_EVENT_INITIAL_WAIT (*(volatile u8 *)0x804d6c64)
#define L7_MENU_COOLDOWN (*(volatile u16 *)0x804d6bc8)
#define L7_MENU_CURRENT (*(volatile u8 *)0x804a04f0)
#define L7_ALL_PORTS_TRIGGER (*(volatile u64 *)0x80479cf8)
#define L7_PAD_CONFIRM (1ULL << 32)

typedef struct L7EventCursor {
    u8 page;
    u8 pad[3];
    s32 first_event;
} L7EventCursor;

static int l7_autoload_complete;
static GOBJProc *l7_event_control_proc;

static void L7_AutoloadEventThink(GOBJ *gobj)
{
    /* mnEvent_8024D864 owns both readiness counters. Let it advance them and
     * create its event-list GObj before supplying the one confirmed input. */
    if (L7_MENU_CURRENT != 7 || L7_MENU_COOLDOWN != 0 ||
        L7_EVENT_INITIAL_WAIT != 0 || L7_EVENT_GOBJ == 0 || Memcard_CheckStatus() == 11)
    {
        L7_EVENT_CONTROL(gobj);
        return;
    }

    l7_autoload_complete = 1;
    if (l7_event_control_proc != 0)
        l7_event_control_proc->cb = L7_EVENT_CONTROL_CALLBACK;

    L7AutoFile selected;
    if (L7_AutoBypass() || !L7_AutoFind(&selected)) {
        OSReport("L7 AUTOLOAD normal menu phase=%d seen=%d candidates=%d\n", l7_auto_scan_phase, l7_auto_scan_seen, l7_auto_scan_candidates);
        L7_EVENT_CONTROL(gobj);
        return;
    }
    /* Existing import handoff byte; CSS consumes -2 before setting card A. */
    *(s8 *)R13_OFFSET(-0x466F) = -2;
    OSReport("L7 AUTOLOAD boot=%s\n", selected.name);
    stc_memcard->TM_EventPage = 1;
    stc_memcard->EventBackup.event = 0;
    /* The native handler selects first_event + page from this live object. */
    L7EventCursor *cursor = (L7EventCursor *)L7_EVENT_GOBJ->userdata;
    cursor->page = 0;
    cursor->first_event = 0;

    /* mn_80229624(4) reads the all-controller entry in controller_map. Its
     * trigger field is cached before menu processes run, so set PAD_CONFIRM
     * there for this supported event-handler invocation. */
    L7_ALL_PORTS_TRIGGER |= L7_PAD_CONFIRM;
    OSReport("L7 AUTOLOAD event confirm\n");
    L7_EVENT_CONTROL(gobj);
}

static GOBJProc *L7_FindEventControlProc(void)
{
    GOBJProc **heads = *stc_gobjproc_lookup;
    GOBJProc *proc;

    if (heads == 0)
        return 0;
    /* TM-CE's "Change ESS Proc To Pri 1" patch changes the priority argument
     * in mnEvent_8024E838 from vanilla 0 to 1. */
    for (proc = heads[1]; proc != 0; proc = proc->next) {
        if (proc->cb == L7_EVENT_CONTROL_CALLBACK)
            return proc;
    }
    return 0;
}

static void L7_AutoloadPoll(GOBJ *gobj)
{
    GOBJProc *proc;

    if (l7_autoload_complete) return;
    if (L7_AutoBypass()) {
        l7_autoload_complete = 1;
        *(s8 *)R13_OFFSET(-0x466F) = -1;
        OSReport("L7 AUTOLOAD bypass B\n");
        return;
    }
    if (L7_MENU_CURRENT != 7) return;
    proc = L7_FindEventControlProc();
    if (proc == 0)
        return;

    l7_event_control_proc = proc;
    proc->cb = L7_AutoloadEventThink;
    OSReport("L7 AUTOLOAD event process wrapped\n");
}

static void L7_AutoloadInstall(void)
{
    GOBJ *gobj;

    if (l7_autoload_complete)
        return;
    /* OnSceneChange creates fresh process lists for the incoming scene. The
     * poller disappears with that scene and is recreated until the event menu
     * exposes its native control process. */
    gobj = GObj_Create(0, 1, 0x80);
    GObj_AddProc(gobj, L7_AutoloadPoll, 0);
}
