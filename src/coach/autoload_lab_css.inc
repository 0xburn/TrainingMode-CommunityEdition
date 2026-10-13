#include "autoload_scan.inc"

static void L7_AutoloadLabCSS(void)
{
    /* Only the one boot request may auto-import. Later manual CSS visits stay manual. */
    if (*onload_slot != -2) return;
    *onload_slot = -1;
    L7AutoFile selected;
    if (L7_AutoBypass() || !L7_AutoFind(&selected)) {
        OSReport("L7 AUTOLOAD manual CSS\n");
        return;
    }
    int slot = 0;
    __typeof__(((ExportHeader *)0)->metadata) metadata;
    memcpy(&metadata, &selected.header, sizeof(metadata));
    OSReport("L7 AUTOLOAD selected=%s\n", selected.name);
    VSMinorData *css = *stc_css_minorscene;
    Preload *preload = Preload_GetTable();
    css->vs_data.match_init.playerData[0].c_kind = metadata.hmn;
    css->vs_data.match_init.playerData[0].costume = metadata.hmn_costume;
    css->vs_data.match_init.playerData[1].c_kind = metadata.cpu;
    css->vs_data.match_init.playerData[1].costume = metadata.cpu_costume;
    css->vs_data.match_init.stage = metadata.stage_external;
    preload->queued.fighters[0].kind = metadata.hmn;
    preload->queued.fighters[0].costume = metadata.hmn_costume;
    preload->queued.fighters[1].kind = metadata.cpu;
    preload->queued.fighters[1].costume = metadata.cpu_costume;
    preload->queued.stage = metadata.stage_external;
    event_desc->stage = metadata.stage_external;
    *onload_slot = slot;
    *onload_fileno = selected.file_no;
    Preload_Update();
    *stc_css_exitkind = 1;
    for (int i = 0; i < 4; i++)
        stc_css_pad[i].held &= ~PAD_BUTTON_A;
    OSReport("L7 AUTOLOAD file=%d human=%d cpu=%d stage=%d\n",
        selected.file_no, metadata.hmn, metadata.cpu, metadata.stage_external);
}
