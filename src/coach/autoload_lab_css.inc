/* The launcher gives its selected file this internal memory-card name. */
#define L7_AUTOLOAD_CARD_NAME "TMREC_L7AUTOLOAD"

static void L7_AutoloadLabCSS(void)
{
    int slot = 0;
    int file_no = -1;
    __typeof__(((ExportHeader *)0)->metadata) metadata;
    s32 mem_size, sector_size;

    if (CARDProbeEx(slot, &mem_size, &sector_size) == CARD_RESULT_READY) {
        stc_memcard_work->is_done = 0;
        if (CARDMountAsync(slot, stc_memcard_work->work_area, 0,
                Memcard_RemovedCallback) == CARD_RESULT_READY) {
            Memcard_Wait();
            stc_memcard_work->is_done = 0;
            if (CARDCheckAsync(slot, Memcard_RemovedCallback) == CARD_RESULT_READY) {
                Memcard_Wait();
                for (int i = 0; i < CARD_MAX_FILE; i++) {
                    CARDStat stat;
                    if (CARDGetStatus(slot, i, &stat) != CARD_RESULT_READY)
                        continue;
                    if (strncmp(os_info->company, (char *)stat.company,
                            sizeof(os_info->company)) != 0 ||
                        strncmp(os_info->gameName, (char *)stat.gameName,
                            sizeof(os_info->gameName)) != 0 ||
                        strncmp(L7_AUTOLOAD_CARD_NAME, stat.fileName,
                            sizeof(L7_AUTOLOAD_CARD_NAME)) != 0)
                        continue;

                    void *buffer = calloc(CARD_READ_SIZE);
                    ExportHeader *header = GetExportHeaderFromCard(
                        slot, stat.fileName, buffer);
                    if (header != NULL) {
                        metadata = header->metadata;
                        file_no = i;
                    }
                    HSD_Free(buffer);
                    break;
                }
            }
            CARDUnmount(slot);
            stc_memcard_work->is_done = 0;
        }
    }

    if (file_no < 0) {
        OSReport("L7 AUTOLOAD file missing\n");
        return;
    }

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
    *onload_fileno = file_no;
    Preload_Update();
    *stc_css_exitkind = 1;
    for (int i = 0; i < 4; i++)
        stc_css_pad[i].held &= ~PAD_BUTTON_A;
    OSReport("L7 AUTOLOAD file=%d human=%d cpu=%d stage=%d\n",
        file_no, metadata.hmn, metadata.cpu, metadata.stage_external);
}
