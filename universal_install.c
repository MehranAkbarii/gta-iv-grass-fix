#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_pipeline.h"
#include "universal_hooks.h"
#include "universal_behavior.h"
#include "universal_plant_bank.h"
#include "universal_source_bank.h"
#include "universal_install.h"

struct GuUniversalInstallState g_gu_install;
volatile LONG g_gu_install_lifecycle_state;
volatile LONG g_gu_install_lifecycle_rejections;
#ifdef GTAIV_GRASS_INSTALL_TESTING
volatile LONG g_gu_install_test_pause_after_admission;
volatile LONG g_gu_install_test_admission_reached;
volatile LONG g_gu_install_test_admission_release;
#endif

static void gu_install_set_failure(DWORD stage)
{
    InterlockedExchange(
        &g_gu_install.failure_stage, (LONG)stage);
}

static int gu_install_float_is_finite(float value)
{
    DWORD bits = *(const DWORD *)(const void *)&value;
    return (bits & 0x7F800000u) != 0x7F800000u;
}

static void gu_install_release_prepared(void)
{
    gu_source_bank_release_prepared(
        &g_gu_install.source_bank);
    gu_plant_bank_release_prepared(
        &g_gu_install.plant_bank);
    gu_behavior_hook_release_stub(
        g_gu_install.procobj_commit_stub);
    g_gu_install.procobj_commit_stub = NULL;
    gu_behavior_hook_release_stub(
        g_gu_install.plant_commit_stub);
    g_gu_install.plant_commit_stub = NULL;
    gu_behavior_hook_release_stub(
        g_gu_install.release_observer_stub);
    g_gu_install.release_observer_stub = NULL;
    gu_behavior_hook_release_stub(
        g_gu_install.manager_prebuild_stub);
    g_gu_install.manager_prebuild_stub = NULL;
    gu_behavior_hook_release_stub(
        g_gu_install.definition_loaded_stub);
    g_gu_install.definition_loaded_stub = NULL;
    gu_manager_terminal_release_stub(
        g_gu_install.manager_terminal_stub);
    g_gu_install.manager_terminal_stub = NULL;
}

static int gu_install_abort_prepared(void)
{
    int source_ok;
    int plant_ok;
    gu_plant_bank_clear_selector();
    source_ok = gu_source_bank_shutdown();
    plant_ok = gu_plant_bank_shutdown();
    if (!source_ok || !plant_ok) {
        /* A wrapper may have entered a freshly committed stub before a
         * failing transaction was rolled back.  Retain every prepared code
         * allocation until process exit if rundown cannot prove it idle. */
        return 0;
    }
    gu_install_release_prepared();
    return 1;
}

static int gu_install_fail_unprepared(DWORD stage)
{
    gu_install_set_failure(stage);
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_IDLE);
    return 0;
}

static int gu_install_fail_dirty(DWORD stage);

static int gu_install_fail_prepared(DWORD stage)
{
    gu_install_set_failure(stage);
    if (!gu_install_abort_prepared()) {
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_PREPARED_RUNDOWN);
    }
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_IDLE);
    return 0;
}

static int gu_install_fail_dirty(DWORD stage)
{
    gu_install_set_failure(stage);
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_FAILED_DIRTY);
    return 0;
}

static int gu_install_rollback_writes(void)
{
    int ok = 1;
    if ((g_gu_install.transaction.committed ||
         gu_patch_transaction_has_unrestored(
             &g_gu_install.transaction)) &&
        !gu_patch_transaction_rollback(
            &g_gu_install.transaction)) {
        ok = 0;
    }
    return ok;
}

int gu_universal_prepare(
    BYTE *image_base,
    const struct GuUniversalExecutableProfile *profile,
    float distance_multiplier,
    float plant_density_multiplier,
    GuPlantSelectionCallback plant_selector,
    void *plant_selector_context,
    GuManagerPrebuildCallback manager_prebuild_callback,
    GuDefinitionLoadedCallback definition_loaded_callback)
{
    const struct GuPlantBankTargetContract *plant_bank_contract;
    const struct GuSourceBankTargetContract *source_bank_contract;
    HMODULE pinned_module = NULL;
    if (InterlockedCompareExchange(
            &g_gu_install_lifecycle_state,
            GU_INSTALL_LIFECYCLE_PREPARING,
            GU_INSTALL_LIFECYCLE_IDLE) !=
        GU_INSTALL_LIFECYCLE_IDLE) {
        InterlockedIncrement(
            &g_gu_install_lifecycle_rejections);
        return 0;
    }
#ifdef GTAIV_GRASS_INSTALL_TESTING
    if (InterlockedCompareExchange(
            &g_gu_install_test_pause_after_admission, 0, 0) != 0) {
        InterlockedExchange(
            &g_gu_install_test_admission_reached, 1);
        while (InterlockedCompareExchange(
                   &g_gu_install_test_admission_release, 0, 0) == 0) {
            Sleep(0u);
        }
    }
#endif
    if (!image_base || !profile ||
        profile->identity_id == 0u ||
        !profile->target_id || !profile->sha256 ||
        !manager_prebuild_callback ||
        !definition_loaded_callback ||
        !gu_install_float_is_finite(distance_multiplier) ||
        !gu_install_float_is_finite(plant_density_multiplier) ||
        plant_density_multiplier < GU_DENSITY_MULTIPLIER_MIN ||
        plant_density_multiplier > GU_DENSITY_MULTIPLIER_MAX ||
        (plant_density_multiplier != 1.0f && !plant_selector) ||
        !gu_patch_contract_validate(
            image_base,
            &profile->plant_distance_setter)) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_ARGUMENT);
    }
    if (distance_multiplier < GU_DISTANCE_MULTIPLIER_MIN) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_DISTANCE_LOW);
    }
    if (distance_multiplier > GU_DISTANCE_MULTIPLIER_MAX) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_DISTANCE_HIGH);
    }
    if (30.0f * distance_multiplier < 15.0f) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_RADIUS_LOW);
    }
    if (30.0f * distance_multiplier > 120.0f) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_RADIUS_HIGH);
    }
    if (!IsProcessorFeaturePresent(
            PF_XMMI_INSTRUCTIONS_AVAILABLE)) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_BEHAVIOR_SYNCHRONIZATION);
    }
    /* PLANT remains validation-only.  The exact PROCOBJ commit site and the
     * common release entry are installed later in the same transaction. */
    if (!gu_patch_contract_validate(
            image_base, &profile->plant_commit.site) ||
        !gu_patch_contract_validate(
            image_base, &profile->procobj_commit.site)) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_COMMIT_PREPARE);
    }
    if (!gu_patch_contract_validate(
            image_base, &profile->release) ||
        !gu_patch_contract_validate(
            image_base, &profile->release_observer.site)) {
        return gu_install_fail_unprepared(
            GU_INSTALL_FAILURE_RELEASE_PREPARE);
    }

    ZeroMemory(&g_gu_install, sizeof(g_gu_install));
    g_gu_install.image_base = image_base;
    g_gu_install.profile = profile;
    gu_patch_transaction_initialize(
        &g_gu_install.transaction);
    if (!gu_behavior_initialize(
            profile->identity_id, distance_multiplier)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_BEHAVIOR_SYNCHRONIZATION);
    }

    /* Put the one-shot procedural definition call first in the transaction.
     * This minimizes (but does not claim to eliminate) the interval in which
     * the original loader could pass its call site after preparation.  Its
     * callback remains unarmed until every transaction site is committed. */
    if (!gu_definition_loaded_prepare_transaction(
            &g_gu_install.transaction,
            image_base, &profile->definition_loaded_call,
            definition_loaded_callback,
            &g_gu_install.definition_loaded_stub)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_DEFINITION_LOAD_PREPARE);
    }

    source_bank_contract = gu_source_bank_contract_for_identity(
        profile->target_id, profile->sha256);
    if (!source_bank_contract ||
        !gu_source_bank_prepare_transaction(
            &g_gu_install.transaction, image_base,
            source_bank_contract, GU_SOURCE_BANK_CAPACITY,
            distance_multiplier, &g_gu_install.source_bank)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_SOURCE_PREPARE);
    }

    plant_bank_contract = gu_plant_bank_contract_for_target(
        profile->target_id);
    if (!plant_bank_contract || !plant_bank_contract->sha256 ||
        lstrcmpA(plant_bank_contract->sha256, profile->sha256) != 0 ||
        !gu_plant_bank_prepare_transaction(
            &g_gu_install.transaction, image_base,
            plant_bank_contract, &g_gu_install.plant_bank) ||
        (plant_selector && !gu_plant_bank_set_selector(
            plant_selector, plant_selector_context))) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_PLANT_COUNT_PREPARE);
    }

    if (!gu_manager_terminal_prepare_transaction(
            &g_gu_install.transaction,
            image_base, &profile->manager_terminal,
            gu_manager_postscale_hook,
            &g_gu_install.manager_terminal_stub)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_MANAGER_TERMINAL_PREPARE);
    }
    if (!gu_manager_prebuild_prepare_transaction(
            &g_gu_install.transaction,
            image_base, &profile->manager_prebuild,
            manager_prebuild_callback,
            &g_gu_install.manager_prebuild_stub)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_MANAGER_PREBUILD_PREPARE);
    }
    if (!gu_commit_hook_prepare_transaction(
            &g_gu_install.transaction,
            image_base, &profile->procobj_commit,
            gu_commit_procobj,
            &g_gu_install.procobj_commit_stub)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_COMMIT_PREPARE);
    }
    if (!gu_release_observer_prepare_transaction(
            &g_gu_install.transaction,
            image_base, &profile->release_observer,
            gu_release_observer_hook,
            &g_gu_install.release_observer_stub)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_RELEASE_PREPARE);
    }
    gu_behavior_set_gateways(
        NULL,
        NULL,
        (GuPlantDistanceSetter)(
            image_base +
            profile->plant_distance_setter.rva));
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(const void *)gu_universal_install,
            &pinned_module)) {
        return gu_install_fail_prepared(
            GU_INSTALL_FAILURE_MODULE_PIN);
    }
    g_gu_install.pinned_module = pinned_module;
    return 1;
}

int gu_universal_commit_prepared(void)
{
    if (InterlockedCompareExchange(
            &g_gu_install_lifecycle_state,
            GU_INSTALL_LIFECYCLE_COMMITTING,
            GU_INSTALL_LIFECYCLE_PREPARING) !=
            GU_INSTALL_LIFECYCLE_PREPARING) {
        InterlockedIncrement(
            &g_gu_install_lifecycle_rejections);
        return 0;
    }
    if (!gu_patch_transaction_commit(
            &g_gu_install.transaction)) {
        gu_install_set_failure(
            GU_INSTALL_FAILURE_TRANSACTION_COMMIT);
        if (!gu_install_rollback_writes()) {
            return gu_install_fail_dirty(
                GU_INSTALL_FAILURE_ROLLBACK);
        }
        /* Once any transaction write was attempted, a game thread may have
         * branched into a stub immediately before the restored site became
         * visible.  Production fails launch and retains every allocation
         * until process termination rather than claiming callback quiescence
         * and freeing possibly executing code. */
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_TRANSACTION_COMMIT);
    }

    gu_behavior_enable();
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) == 0) {
        if (!gu_install_rollback_writes()) {
            return gu_install_fail_dirty(
                GU_INSTALL_FAILURE_ROLLBACK);
        }
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_BEHAVIOR_SYNCHRONIZATION);
    }
    InterlockedExchange(&g_gu_install.installed, 1);
    gu_install_set_failure(GU_INSTALL_FAILURE_NONE);
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_ACTIVE);
    return 1;
}

int gu_universal_cancel_prepared(void)
{
    if (InterlockedCompareExchange(
            &g_gu_install_lifecycle_state,
            GU_INSTALL_LIFECYCLE_CANCELLING,
            GU_INSTALL_LIFECYCLE_PREPARING) !=
            GU_INSTALL_LIFECYCLE_PREPARING) {
        InterlockedIncrement(
            &g_gu_install_lifecycle_rejections);
        return 0;
    }
    gu_install_set_failure(
        GU_INSTALL_FAILURE_PREPARED_CANCELLED);
    if (!gu_install_abort_prepared()) {
        gu_install_fail_dirty(
            GU_INSTALL_FAILURE_PREPARED_RUNDOWN);
        return 0;
    }
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_IDLE);
    return 1;
}

int gu_universal_install(
    BYTE *image_base,
    const struct GuUniversalExecutableProfile *profile,
    float distance_multiplier,
    float plant_density_multiplier,
    GuPlantSelectionCallback plant_selector,
    void *plant_selector_context,
    GuManagerPrebuildCallback manager_prebuild_callback,
    GuDefinitionLoadedCallback definition_loaded_callback)
{
    if (!gu_universal_prepare(
            image_base, profile,
            distance_multiplier, plant_density_multiplier,
            plant_selector, plant_selector_context,
            manager_prebuild_callback,
            definition_loaded_callback)) {
        return 0;
    }
    return gu_universal_commit_prepared();
}

int gu_universal_uninstall(void)
{
    int manager_ok;
    int entities_ok = 1;
    int writes_ok;
    LONG prior_state = InterlockedCompareExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_UNINSTALLING,
        GU_INSTALL_LIFECYCLE_ACTIVE);
    if (prior_state == GU_INSTALL_LIFECYCLE_IDLE) {
        return 1;
    }
    if (prior_state != GU_INSTALL_LIFECYCLE_ACTIVE) {
        InterlockedIncrement(
            &g_gu_install_lifecycle_rejections);
        return 0;
    }
    if (!gu_behavior_disable()) {
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_UNINSTALL);
    }
    writes_ok = gu_install_rollback_writes();
    if (!writes_ok) {
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_ROLLBACK);
    }
    InterlockedExchange(&g_gu_install.installed, 0);
    gu_plant_bank_clear_selector();
    if (!gu_source_bank_shutdown() ||
        !gu_plant_bank_shutdown()) {
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_UNINSTALL);
    }
    gu_behavior_restore_all_owned();
    if (InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0) != 0) {
        entities_ok = 0;
    }
    manager_ok = gu_behavior_restore_manager();
    gu_install_release_prepared();
    if (!manager_ok || !entities_ok) {
        return gu_install_fail_dirty(
            GU_INSTALL_FAILURE_UNINSTALL);
    }
    gu_install_set_failure(GU_INSTALL_FAILURE_NONE);
    InterlockedExchange(
        &g_gu_install_lifecycle_state,
        GU_INSTALL_LIFECYCLE_IDLE);
    return 1;
}
