#ifndef GTAIV_GRASS_UNIVERSAL_BEHAVIOR_H
#define GTAIV_GRASS_UNIVERSAL_BEHAVIOR_H

#include <windows.h>

#if defined(_MSC_VER)
#  define GU_FASTCALL __fastcall
#  define GU_CDECL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#  define GU_FASTCALL __attribute__((fastcall))
#  define GU_CDECL __attribute__((cdecl))
#else
#  define GU_FASTCALL
#  define GU_CDECL
#endif

typedef void * (GU_FASTCALL *GuCreateGateway)(
    BYTE *manager, DWORD ignored_edx,
    DWORD argument1, DWORD argument2, DWORD argument3,
    DWORD argument4, DWORD argument5);
typedef DWORD (GU_FASTCALL *GuManagerInitGateway)(
    BYTE *manager, DWORD ignored_edx);
typedef void (GU_CDECL *GuPlantDistanceSetter)(
    float near_distance, float far_distance);
typedef void (GU_FASTCALL *GuManagerPrebuildCallback)(
    BYTE *manager, DWORD ignored_edx);

struct GuBehaviorState {
    volatile LONG enabled;
    volatile LONG shutdown_terminal;
    CRITICAL_SECTION ownership_lock;
    volatile LONG ownership_lock_ready;
    volatile LONG ownership_lock_acquisitions;
    volatile LONG ownership_lock_contentions;
    DWORD identity_id;
    DWORD distance_multiplier_bits;
    volatile LONG manager_updates;
    volatile LONG manager_failures;
    volatile LONG manager_restore_mismatches;
    HANDLE manager_setter_idle_event;
    volatile LONG manager_setter_worker_active;
    volatile LONG manager_setter_generation;
    volatile LONG manager_setter_applied_generation;
    volatile LONG manager_setter_calls;
    volatile LONG manager_setter_coalesced_updates;
    volatile LONG manager_setter_handoffs;
    volatile LONG manager_setter_waits;
    volatile LONG manager_setter_wait_failures;
    volatile LONG manager_setter_rundown_failures;
    volatile LONG manager_setter_generation_exhaustions;
    DWORD manager_setter_desired_near_bits;
    DWORD manager_setter_desired_far_bits;
    volatile LONG manager_key;
    DWORD manager_original_near_bits;
    DWORD manager_original_far_bits;
    DWORD manager_original_query_bits;
    DWORD manager_original_far_squared_bits;
    DWORD manager_scaled_near_bits;
    DWORD manager_scaled_far_bits;
    DWORD manager_scaled_query_bits;
    DWORD manager_scaled_far_squared_bits;
    volatile LONG entities_scaled;
    volatile LONG entities_clamped;
    volatile LONG entities_restored;
    volatile LONG entity_restore_mismatches;
    volatile LONG release_restores;
    volatile LONG release_restore_mismatches;
    volatile LONG distance_scaling_disabled;
    volatile LONG ownership_failures;
    volatile LONG ownership_publications;
    volatile LONG ownership_release_takes;
    volatile LONG ownership_unowned_release_skips;
    volatile LONG ownership_shutdown_commit_skips;
    volatile LONG ownership_reuse_refusals;
    volatile LONG ownership_publish_cas_losses;
    volatile LONG ownership_publish_cleanup_failures;
    volatile LONG ownership_third_party_relinquishments;
    volatile LONG distance_applied_active;
    volatile LONG clean_shutdown_balances;
    volatile LONG clean_shutdown_imbalances;
    GuCreateGateway create_gateway;
    GuManagerInitGateway manager_init_gateway;
    GuPlantDistanceSetter plant_distance_setter;
};

extern struct GuBehaviorState g_gu_behavior;

#if defined(GTAIV_GRASS_BEHAVIOR_TESTING)
extern volatile LONG g_gu_behavior_test_pause_commit_after_publish;
extern volatile LONG g_gu_behavior_test_commit_publish_reached;
extern volatile LONG g_gu_behavior_test_pause_commit_before_lock;
extern volatile LONG g_gu_behavior_test_commit_before_lock_reached;
extern volatile LONG g_gu_behavior_test_pause_release_after_take;
extern volatile LONG g_gu_behavior_test_release_take_reached;
#endif

int gu_behavior_initialize(DWORD identity_id, float distance_multiplier);
void gu_behavior_enable(void);
int gu_behavior_disable(void);
void gu_behavior_set_gateways(
    GuCreateGateway create_gateway,
    GuManagerInitGateway manager_init_gateway,
    GuPlantDistanceSetter plant_distance_setter);
void gu_behavior_restore_all_owned(void);
int gu_behavior_restore_manager(void);

void GU_FASTCALL gu_manager_postscale_hook(
    BYTE *manager, DWORD ignored_edx);
void GU_FASTCALL gu_manager_prebuild_hook(
    BYTE *manager, DWORD ignored_edx);
DWORD GU_FASTCALL gu_manager_init_hook(
    BYTE *manager, DWORD ignored_edx);
void *GU_FASTCALL gu_create_hook(
    BYTE *manager, DWORD ignored_edx,
    DWORD argument1, DWORD argument2, DWORD argument3,
    DWORD argument4, DWORD argument5);
void GU_FASTCALL gu_release_observer_hook(
    BYTE *wrapper, DWORD ignored_edx);
void GU_FASTCALL gu_commit_plant(
    BYTE *wrapper, DWORD source_owner);
void GU_FASTCALL gu_commit_procobj(
    BYTE *wrapper, DWORD surface_owner);

#endif
