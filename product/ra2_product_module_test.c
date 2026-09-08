#include "ra2_product_module.h"
#include <stdio.h>

int main(void) {
    Ra2ModuleRegistry r;
    unsigned char original[2] = {0x2B, 0xC7};
    unsigned char same[2] = {0x2B, 0xC7};
    unsigned char changed[2] = {0x90, 0x90};
    ra2_registry_init(&r);
    if (ra2_begin_apply(&r, RA2_MODULE_MONEY) != RA2_RESULT_OK) return 1;
    if (ra2_begin_apply(&r, RA2_MODULE_MONEY) != RA2_RESULT_BUSY) return 2;
    if (ra2_commit_apply(&r, RA2_MODULE_MONEY, original, sizeof(original)) != RA2_RESULT_OK) return 3;
    if (ra2_begin_apply(&r, RA2_MODULE_MONEY) != RA2_RESULT_ALREADY_ON) return 4;
    if (ra2_begin_restore(&r, RA2_MODULE_MONEY) != RA2_RESULT_OK) return 5;
    if (ra2_commit_restore(&r, RA2_MODULE_MONEY, changed, sizeof(changed)) != RA2_RESULT_SNAPSHOT_MISMATCH) return 6;
    if (ra2_module_state(&r, RA2_MODULE_MONEY) != RA2_STATE_ERROR) return 7;
    ra2_registry_init(&r);
    if (ra2_begin_apply(&r, RA2_MODULE_MONEY) != RA2_RESULT_OK) return 8;
    if (ra2_commit_apply(&r, RA2_MODULE_MONEY, original, sizeof(original)) != RA2_RESULT_OK) return 9;
    if (ra2_begin_restore(&r, RA2_MODULE_MONEY) != RA2_RESULT_OK) return 10;
    if (ra2_commit_restore(&r, RA2_MODULE_MONEY, same, sizeof(same)) != RA2_RESULT_OK) return 11;
    puts("MODULE_STATE_TEST=PASS");
    return 0;
}
