#ifndef L7_AUTOLOAD_POLICY_H
#define L7_AUTOLOAD_POLICY_H
/* Shared fixed-width GCI identity rules. Never read past CARDStat.fileName. */
static int L7_AutoKind(const char name[32])
{
    static const char marker[] = "TMREC_L7AUTOLOAD";
    static const char prefix[] = "TMREC_L7_";
    int i;
    for (i = 0; i < (int)sizeof(marker); i++)
        if (name[i] != marker[i]) break;
    if (i == sizeof(marker)) return 2; /* Explicit launcher selection wins. */
    for (i = 0; i < 9; i++) if (name[i] != prefix[i]) return 0;
    for (i = 9; i < 22; i++) if (name[i] < '0' || name[i] > '9') return 0;
    if (name[22] != '_' || name[31] != 0) return 0;
    for (i = 23; i < 31; i++)
        if (!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f'))) return 0;
    return 1;
}
static int L7_AutoNewer(const char candidate[32], const char best[32])
{
    int kind = L7_AutoKind(candidate), previous = L7_AutoKind(best);
    if (!kind) return 0;
    if (kind != previous) return kind > previous;
    /* Millisecond export time first; random ID provides a stable tie-breaker. */
    for (int i = 0; i < 32; i++)
        if (candidate[i] != best[i]) return (unsigned char)candidate[i] > (unsigned char)best[i];
    return 0;
}
#endif
