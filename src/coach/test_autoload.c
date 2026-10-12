#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "autoload_policy.h"
int main(void)
{
    char empty[32] = {0};
    char older[32] = "TMREC_L7_1791800000000_ffffffff";
    char newer[32] = "TMREC_L7_1791800000001_00000000";
    char tie[32] = "TMREC_L7_1791800000001_00000001";
    char explicit_file[32] = "TMREC_L7AUTOLOAD";
    char traditional[32] = "TMREC_10122026_123456_abcdef01";
    assert(L7_AutoKind(older) == 1 && L7_AutoKind(explicit_file) == 2);
    assert(!L7_AutoKind(traditional) && !L7_AutoKind(empty));
    assert(L7_AutoNewer(older, empty));
    assert(L7_AutoNewer(newer, older) && !L7_AutoNewer(older, newer));
    assert(L7_AutoNewer(tie, newer) && !L7_AutoNewer(tie, tie));
    assert(L7_AutoNewer(explicit_file, tie) && !L7_AutoNewer(tie, explicit_file));
    for (int i = 0; i < 32; i++) {
        char bad[32]; memcpy(bad, newer, 32); bad[i] = '!';
        assert(!L7_AutoKind(bad));
    }
    puts("autoload identity: latest export, ties, explicit selection, legacy/manual and malformed names passed");
}
