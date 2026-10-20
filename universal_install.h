#ifndef GTAIV_GRASS_UNIVERSAL_INSTALL_H
#define GTAIV_GRASS_UNIVERSAL_INSTALL_H

#include <windows.h>
#include "universal_hooks.h"
#include "universal_behavior.h"
#include "universal_behavior_hooks.h"
#include "universal_plant_density.h"
#include "universal_plant_bank.h"
#include "universal_source_bank.h"

enum GuInstallFailureStage {
    GU_INSTALL_FAILURE_NONE = 0,
    GU_INSTALL_FAILURE_ARGUMENT = 1,
    GU_INSTALL_FAILURE_SOURCE_PREPARE = 2,
    GU_INSTALL_FAILURE_MANAGER_TERMINAL_PREPARE = 3,
    GU_INSTALL_FAILURE_MANAGER_PREBUILD_PREPARE = 4,
    GU_INSTALL_FAILURE_COMMIT_PREPARE = 5,
    GU_INSTALL_FAILURE_RELEASE_PREPARE = 6,
    GU_INSTALL_FAILURE_RELEASE_TRANSACTION_PREPARE = 7,
    GU_INSTALL_FAILURE_BEHAVIOR_SYNCHRONIZATION = 8,
    GU_INSTALL_FAILURE_RESERVED_9 = 9,
    GU_INSTALL_FAILURE_TRANSACTION_COMMIT = 10,
    GU_INSTALL_FAILURE_PLANT_COUNT_PREPARE = 11,
    GU_INSTALL_FAILURE_ROLLBACK = 12,
    GU_INSTALL_FAILURE_UNINSTALL = 13,
    GU_INSTALL_FAILURE_MODULE_PIN = 14,
    GU_INSTALL_FAILURE_DISTANCE_ARGUMENT = 15,
    GU_INSTALL_FAILURE_DISTANCE_LOW = 16,
    GU_INSTALL_FAILURE_DISTANCE_HIGH = 17,
    GU_INSTALL_FAILURE_RADIUS_LOW = 18,
    GU_INSTALL_FAILURE_RADIUS_HIGH = 19,
    GU_INSTALL_FAILURE_DEFINITION_LOAD_PREPARE = 20,
    GU_INSTALL_FAILURE_PREPARED_RUNDOWN = 21,
    GU_INSTALL_FAILURE_PREPARED_CANCELLED = 22
};

enum GuInstallLifecycleState {
    GU_INSTALL_LIFECYCLE_IDLE = 0,
    GU_INSTALL_LIFECYCLE_PREPARING = 1,
    GU_INSTALL_LIFECYCLE_ACTIVE = 2,
    GU_INSTALL_LIFECYCLE_UNINSTALLING = 3,
    GU_INSTALL_LIFECYCLE_FAILED_DIRTY = 4,
    GU_INSTALL_LIFECYCLE_COMMITTING = 5,
    GU_INSTALL_LIFECYCLE_CANCELLING = 6
};

struct GuUniversalExecutableProfile {
    DWORD identity_id;
    const char *target_id;
    const char *sha256;
    struct GuPatchContract definition_loaded_call;
    struct GuManagerPrebuildContract manager_prebuild;
    struct GuPatchContract manager_terminal;
    struct GuPatchContract plant_distance_setter;
    struct GuCommitHookContract plant_commit;
    struct GuCommitHookContract procobj_commit;
    struct GuPatchContract release;
    struct GuReleaseObserverContract release_observer;
    struct GuSourceArrayContract source_array;
    struct GuPlantCountContract plant_count;
};

struct GuUniversalInstallState {
    volatile LONG installed;
    volatile LONG failure_stage;
    BYTE *image_base;
    HMODULE pinned_module;
    const struct GuUniversalExecutableProfile *profile;
    struct GuPatchTransaction transaction;
    BYTE *definition_loaded_stub;
    BYTE *manager_terminal_stub;
    BYTE *manager_prebuild_stub;
    BYTE *plant_commit_stub;
    BYTE *procobj_commit_stub;
    BYTE *release_observer_stub;
    struct GuSourceBankPrepared source_bank;
    struct GuPlantBankPrepared plant_bank;
};

extern struct GuUniversalInstallState g_gu_install;
extern volatile LONG g_gu_install_lifecycle_state;
extern volatile LONG g_gu_install_lifecycle_rejections;
#ifdef GTAIV_GRASS_INSTALL_TESTING
extern volatile LONG g_gu_install_test_pause_after_admission;
extern volatile LONG g_gu_install_test_admission_reached;
extern volatile LONG g_gu_install_test_admission_release;
#endif

int gu_universal_install(
    BYTE *image_base,
    const struct GuUniversalExecutableProfile *profile,
    float distance_multiplier,
    float plant_density_multiplier,
    GuPlantSelectionCallback plant_selector,
    void *plant_selector_context,
    GuManagerPrebuildCallback manager_prebuild_callback,
    GuDefinitionLoadedCallback definition_loaded_callback);
int gu_universal_prepare(
    BYTE *image_base,
    const struct GuUniversalExecutableProfile *profile,
    float distance_multiplier,
    float plant_density_multiplier,
    GuPlantSelectionCallback plant_selector,
    void *plant_selector_context,
    GuManagerPrebuildCallback manager_prebuild_callback,
    GuDefinitionLoadedCallback definition_loaded_callback);
int gu_universal_commit_prepared(void);
int gu_universal_cancel_prepared(void);
int gu_universal_uninstall(void);

#endif
