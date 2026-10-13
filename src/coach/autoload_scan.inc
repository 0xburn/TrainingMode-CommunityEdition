#include "autoload_policy.h"

/* Same 68-byte on-card layout as ExportHeader; no lab module globals needed. */
typedef struct L7AutoHeader {
    u16 version, image_width, image_height, image_fmt;
    u8 hmn, hmn_costume, cpu, cpu_costume;
    u16 stage_external, stage_internal;
    u8 month, day;
    u16 year;
    u8 hour, minute, second;
    char filename[32];
    u32 screenshot, recording, settings;
} L7AutoHeader;
typedef char L7AutoHeaderSize[(sizeof(L7AutoHeader) == 68) ? 1 : -1];
typedef struct L7AutoFile { int file_no; char name[32]; L7AutoHeader header; } L7AutoFile;
static int l7_auto_scan_phase, l7_auto_scan_seen, l7_auto_scan_candidates;

static int L7_AutoBypass(void)
{
    for (int i = 0; i < 4; i++) {
        HSD_Pad *pad = PadGetMaster(i);
        if (pad && (pad->held & HSD_BUTTON_B)) return 1;
    }
    return 0;
}
static void L7_AutoWait(void) { while (!stc_memcard_work->is_done) blr2(); }

static int L7_AutoFind(L7AutoFile *best)
{
    s32 mem_size, sector_size;
    int slot = 0, seen = 0, candidates = 0;
    memset(best, 0, sizeof(*best)); best->file_no = -1;
    l7_auto_scan_phase = 1; l7_auto_scan_seen = l7_auto_scan_candidates = 0;
    if (Memcard_CheckStatus() == 11) return 0;
    l7_auto_scan_phase = 2;
    if (CARDProbeEx(slot, &mem_size, &sector_size) != CARD_RESULT_READY) return 0;
    l7_auto_scan_phase = 3;
    stc_memcard_work->is_done = 0;
    if (CARDMountAsync(slot, stc_memcard_work->work_area, 0, Memcard_RemovedCallback) != CARD_RESULT_READY) return 0;
    L7_AutoWait(); stc_memcard_work->is_done = 0;
    l7_auto_scan_phase = 4;
    if (CARDCheckAsync(slot, Memcard_RemovedCallback) == CARD_RESULT_READY) {
        L7_AutoWait();
        l7_auto_scan_phase = 5;
        void *buffer = calloc(CARD_READ_SIZE);
        if (buffer) for (int i = 0; i < CARD_MAX_FILE; i++) {
            CARDStat stat; CARDFileInfo file;
            if (CARDGetStatus(slot, i, &stat) != CARD_RESULT_READY) continue;
            seen++;
            if (stat.length < 8192 ||
                strncmp(os_info->company, (char *)stat.company, 2) ||
                strncmp(os_info->gameName, (char *)stat.gameName, 4) ||
                !L7_AutoNewer(stat.fileName, best->name)) continue;
            candidates++;
            int opened = CARDOpen(slot, stat.fileName, &file);
            if (opened != CARD_RESULT_READY) { OSReport("L7 AUTOLOAD open error=%d\n", opened); continue; }
            int read = CARDRead(&file, buffer, CARD_READ_SIZE, 0x1E00);
            CARDClose(&file);
            if (read != CARD_RESULT_READY) { OSReport("L7 AUTOLOAD read error=%d\n", read); continue; }
            Memcard_Deobfuscate(buffer, CARD_READ_SIZE);
            L7AutoHeader *header = (L7AutoHeader *)((u8 *)buffer + 0x90);
            /* Match stock import version and bound fields before preloading. */
            if (header->version != 1 || header->hmn >= 26 || header->cpu >= 26 ||
                header->hmn_costume >= 6 || header->cpu_costume >= 6 ||
                /* tm_replay writes a 67-byte header before its image padding. */
                header->stage_external >= 33 || header->screenshot < sizeof(*header) - 1 ||
                header->recording < header->screenshot || header->settings <= header->recording ||
                header->settings > stat.length - 6) continue;
            best->file_no = i; memcpy(best->name, stat.fileName, 32);
            best->header = *header;
        }
        if (buffer) HSD_Free(buffer);
    }
    CARDUnmount(slot); stc_memcard_work->is_done = 0;
    l7_auto_scan_seen = seen; l7_auto_scan_candidates = candidates;
    OSReport("L7 AUTOLOAD scan seen=%d candidates=%d selected=%d\n", seen, candidates, best->file_no);
    return best->file_no >= 0;
}
