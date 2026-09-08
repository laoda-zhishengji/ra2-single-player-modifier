#include "ra2_product_module.h"
#include <string.h>

static int valid(Ra2ModuleId id) { return id >= 0 && id < RA2_MODULE_MAX; }

void ra2_registry_init(Ra2ModuleRegistry *registry) {
    if (registry) memset(registry, 0, sizeof(*registry));
}

const wchar_t *ra2_module_name(Ra2ModuleId id) {
    static const wchar_t *names[RA2_MODULE_MAX] = {
        L"money", L"power", L"instant_production", L"superweapon",
        L"fog", L"build_distance", L"auto_repair", L"garrison_repair"
    };
    return valid(id) ? names[id] : L"unknown";
}

Ra2ModuleState ra2_module_state(const Ra2ModuleRegistry *registry, Ra2ModuleId id) {
    return registry && valid(id) ? registry->modules[id].state : RA2_STATE_ERROR;
}

Ra2ModuleResult ra2_begin_apply(Ra2ModuleRegistry *registry, Ra2ModuleId id) {
    if (!registry || !valid(id)) return RA2_RESULT_INVALID_MODULE;
    if (registry->modules[id].state == RA2_STATE_ON) return RA2_RESULT_ALREADY_ON;
    if (registry->modules[id].state != RA2_STATE_OFF) return RA2_RESULT_BUSY;
    registry->modules[id].state = RA2_STATE_APPLYING;
    return RA2_RESULT_OK;
}

Ra2ModuleResult ra2_commit_apply(Ra2ModuleRegistry *registry, Ra2ModuleId id,
                                 const void *original, size_t size) {
    if (!registry || !valid(id) || !original || size == 0 || size > RA2_SNAPSHOT_MAX)
        return RA2_RESULT_INVALID_MODULE;
    if (registry->modules[id].state != RA2_STATE_APPLYING) return RA2_RESULT_BUSY;
    memcpy(registry->modules[id].snapshot, original, size);
    registry->modules[id].snapshot_size = size;
    registry->modules[id].snapshot_valid = 1;
    registry->modules[id].state = RA2_STATE_ON;
    return RA2_RESULT_OK;
}

Ra2ModuleResult ra2_begin_restore(Ra2ModuleRegistry *registry, Ra2ModuleId id) {
    if (!registry || !valid(id)) return RA2_RESULT_INVALID_MODULE;
    if (registry->modules[id].state != RA2_STATE_ON) return RA2_RESULT_NOT_ON;
    if (!registry->modules[id].snapshot_valid) return RA2_RESULT_SNAPSHOT_MISSING;
    registry->modules[id].state = RA2_STATE_RESTORING;
    return RA2_RESULT_OK;
}

Ra2ModuleResult ra2_commit_restore(Ra2ModuleRegistry *registry, Ra2ModuleId id,
                                   const void *current, size_t size) {
    Ra2ModuleRecord *record;
    if (!registry || !valid(id) || !current) return RA2_RESULT_INVALID_MODULE;
    record = &registry->modules[id];
    if (record->state != RA2_STATE_RESTORING) return RA2_RESULT_BUSY;
    if (!record->snapshot_valid || record->snapshot_size != size ||
        memcmp(record->snapshot, current, size) != 0) {
        record->state = RA2_STATE_ERROR;
        return RA2_RESULT_SNAPSHOT_MISMATCH;
    }
    memset(record, 0, sizeof(*record));
    return RA2_RESULT_OK;
}

const Ra2ModuleRecord *ra2_module_record(const Ra2ModuleRegistry *registry, Ra2ModuleId id) {
    return registry && valid(id) ? &registry->modules[id] : NULL;
}
