#pragma once

#include <stddef.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RA2_MODULE_MAX 8
#define RA2_SNAPSHOT_MAX 32

typedef enum {
    RA2_MODULE_MONEY = 0,
    RA2_MODULE_POWER,
    RA2_MODULE_INSTANT_PRODUCTION,
    RA2_MODULE_SUPERWEAPON,
    RA2_MODULE_FOG,
    RA2_MODULE_BUILD_DISTANCE,
    RA2_MODULE_AUTO_REPAIR,
    RA2_MODULE_GARRISON_REPAIR
} Ra2ModuleId;

typedef enum {
    RA2_STATE_OFF = 0,
    RA2_STATE_APPLYING,
    RA2_STATE_ON,
    RA2_STATE_RESTORING,
    RA2_STATE_ERROR
} Ra2ModuleState;

typedef enum {
    RA2_RESULT_OK = 0,
    RA2_RESULT_INVALID_MODULE,
    RA2_RESULT_BUSY,
    RA2_RESULT_NOT_ON,
    RA2_RESULT_ALREADY_ON,
    RA2_RESULT_SNAPSHOT_MISSING,
    RA2_RESULT_SNAPSHOT_MISMATCH
} Ra2ModuleResult;

typedef struct {
    Ra2ModuleState state;
    unsigned char snapshot[RA2_SNAPSHOT_MAX];
    size_t snapshot_size;
    int snapshot_valid;
} Ra2ModuleRecord;

typedef struct {
    Ra2ModuleRecord modules[RA2_MODULE_MAX];
} Ra2ModuleRegistry;

void ra2_registry_init(Ra2ModuleRegistry *registry);
const wchar_t *ra2_module_name(Ra2ModuleId id);
Ra2ModuleState ra2_module_state(const Ra2ModuleRegistry *registry, Ra2ModuleId id);
Ra2ModuleResult ra2_begin_apply(Ra2ModuleRegistry *registry, Ra2ModuleId id);
Ra2ModuleResult ra2_commit_apply(Ra2ModuleRegistry *registry, Ra2ModuleId id,
                                 const void *original, size_t size);
Ra2ModuleResult ra2_begin_restore(Ra2ModuleRegistry *registry, Ra2ModuleId id);
Ra2ModuleResult ra2_commit_restore(Ra2ModuleRegistry *registry, Ra2ModuleId id,
                                   const void *current, size_t size);
const Ra2ModuleRecord *ra2_module_record(const Ra2ModuleRegistry *registry, Ra2ModuleId id);

#ifdef __cplusplus
}
#endif
