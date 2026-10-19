#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_pipeline.h"
#include "universal_behavior.h"

#define GU_MANAGER_NEAR_OFFSET 0x10u
#define GU_MANAGER_FAR_OFFSET 0x14u
#define GU_MANAGER_QUERY_OFFSET 0x1Cu
#define GU_MANAGER_FAR_SQUARED_OFFSET 0x20u
#define GU_WRAPPER_ENTITY_OFFSET 0x08u
#define GU_WRAPPER_SURFACE_OFFSET 0x0Cu
#define GU_WRAPPER_SOURCE_OFFSET 0x10u
#define GU_ENTITY_DISTANCE_OFFSET 0x50u
#define GU_STOCK_SOURCE_RADIUS 30.0f
#define GU_MANAGER_SETTER_RUNDOWN_TIMEOUT_MS 5000u
#define GU_MANAGER_SETTER_SCHEDULE_FAILED 0u
#define GU_MANAGER_SETTER_SCHEDULE_WORKER 1u
#define GU_MANAGER_SETTER_SCHEDULE_WAITER 2u
#define GU_MANAGER_SETTER_SCHEDULE_COMPLETE 3u
#define GU_MANAGER_SETTER_MAX_CALLS_PER_CALLBACK 1u

struct GuBehaviorState g_gu_behavior;

#if defined(GTAIV_GRASS_BEHAVIOR_TESTING)
volatile LONG g_gu_behavior_test_pause_commit_after_publish;
volatile LONG g_gu_behavior_test_commit_publish_reached;
volatile LONG g_gu_behavior_test_pause_commit_before_lock;
volatile LONG g_gu_behavior_test_commit_before_lock_reached;
volatile LONG g_gu_behavior_test_pause_release_after_take;
volatile LONG g_gu_behavior_test_release_take_reached;

static void gu_behavior_test_pause(
    volatile LONG *pause, volatile LONG *reached)
{
    if (InterlockedCompareExchange(pause, 0, 0) == 0) {
        return;
    }
    InterlockedExchange(reached, 1);
    while (InterlockedCompareExchange(pause, 0, 0) != 0) {
        SwitchToThread();
    }
}
#endif

static DWORD gu_float_bits(float value)
{
    return *(const DWORD *)(const void *)&value;
}

static float gu_bits_float(DWORD value)
{
    return *(const float *)(const void *)&value;
}

static DWORD gu_read_u32(const BYTE *address)
{
    return *(const volatile DWORD *)(const void *)address;
}

static void gu_write_u32(BYTE *address, DWORD value)
{
    *(volatile DWORD *)(void *)address = value;
}

static int gu_address_readable(const BYTE *address, DWORD length)
{
    MEMORY_BASIC_INFORMATION information;
    DWORD start;
    DWORD end;
    DWORD region_end;
    DWORD protection;
    if (!address || length == 0u) {
        return 0;
    }
    ZeroMemory(&information, sizeof(information));
    if (VirtualQuery(
            address, &information,
            sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT) {
        return 0;
    }
    protection = information.Protect & 0xFFu;
    if (protection == PAGE_NOACCESS ||
        protection == PAGE_EXECUTE ||
        (information.Protect & PAGE_GUARD) != 0u) {
        return 0;
    }
    start = (DWORD)address;
    if (start > 0xFFFFFFFFu - length) {
        return 0;
    }
    end = start + length;
    region_end = (DWORD)information.BaseAddress +
                 information.RegionSize;
    return end <= region_end;
}

static int gu_address_writable(BYTE *address, DWORD length)
{
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;
    if (!gu_address_readable(address, length)) {
        return 0;
    }
    ZeroMemory(&information, sizeof(information));
    if (VirtualQuery(
            address, &information,
            sizeof(information)) != sizeof(information)) {
        return 0;
    }
    protection = information.Protect & 0xFFu;
    return protection == PAGE_READWRITE ||
           protection == PAGE_WRITECOPY ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

static void gu_emit_entity_event(
    DWORD type, DWORD wrapper, DWORD entity,
    DWORD surface, DWORD source,
    DWORD original_bits, DWORD scaled_bits)
{
    struct GuRawEvent event;
    ZeroMemory(&event, sizeof(event));
    event.type = type;
    event.identity_id = g_gu_behavior.identity_id;
    event.values[0] = wrapper;
    event.values[1] = entity;
    event.values[2] = surface;
    event.values[3] = source;
    event.values[4] = original_bits;
    event.values[5] = scaled_bits;
    gu_event_try_emit(&g_gu_event_ring, &event);
}

static int gu_behavior_lock_enter(void)
{
    if (InterlockedCompareExchange(
            &g_gu_behavior.ownership_lock_ready, 0, 0) == 0) {
        return 0;
    }
    if (!TryEnterCriticalSection(&g_gu_behavior.ownership_lock)) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_lock_contentions);
        EnterCriticalSection(&g_gu_behavior.ownership_lock);
    }
    InterlockedIncrement(
        &g_gu_behavior.ownership_lock_acquisitions);
    return 1;
}

static void gu_behavior_lock_leave(void)
{
    LeaveCriticalSection(&g_gu_behavior.ownership_lock);
}

int gu_behavior_initialize(DWORD identity_id, float distance_multiplier)
{
    HANDLE previous_setter_event =
        g_gu_behavior.manager_setter_idle_event;
    if (InterlockedCompareExchange(
            &g_gu_behavior.ownership_lock_ready, 0, 0) != 0) {
        InterlockedExchange(
            &g_gu_behavior.ownership_lock_ready, 0);
        DeleteCriticalSection(&g_gu_behavior.ownership_lock);
    }
    if (previous_setter_event) {
        CloseHandle(previous_setter_event);
    }
    ZeroMemory(&g_gu_behavior, sizeof(g_gu_behavior));
    /* Every protected section is deliberately short and contains no engine
     * call.  A zero spin count avoids burning a game thread while another
     * callback is descheduled inside the lifecycle section. */
    if (!InitializeCriticalSectionAndSpinCount(
            &g_gu_behavior.ownership_lock, 0u)) {
        return 0;
    }
    g_gu_behavior.manager_setter_idle_event =
        CreateEventA(NULL, TRUE, TRUE, NULL);
    if (!g_gu_behavior.manager_setter_idle_event) {
        DeleteCriticalSection(&g_gu_behavior.ownership_lock);
        return 0;
    }
    InterlockedExchange(
        &g_gu_behavior.ownership_lock_ready, 1);
    gu_event_ring_initialize(&g_gu_event_ring);
    gu_event_summary_initialize(&g_gu_event_summary);
    gu_ownership_initialize(&g_gu_ownership);
    g_gu_behavior.identity_id = identity_id;
    g_gu_behavior.distance_multiplier_bits =
        gu_float_bits(distance_multiplier);
    InterlockedExchange(&g_gu_behavior.enabled, 0);
    return 1;
}

void gu_behavior_enable(void)
{
    DWORD enabled = 0u;
    if (gu_behavior_lock_enter()) {
        if (InterlockedCompareExchange(
                &g_gu_behavior.shutdown_terminal, 0, 0) == 0) {
            InterlockedExchange(&g_gu_behavior.enabled, 1);
            enabled = 1u;
        }
        gu_behavior_lock_leave();
    }
    InterlockedExchange(
        &g_gu_event_ring.enabled, (LONG)enabled);
}

int gu_behavior_disable(void)
{
    HANDLE idle_event = NULL;
    DWORD wait_required = 0u;
    /* Stop diagnostics admission first, then linearize feature shutdown with
     * every state-mutating callback.  When this function returns, a callback
     * that passed its locked enabled check has completed its owned mutation. */
    InterlockedExchange(&g_gu_event_ring.enabled, 0);
    if (gu_behavior_lock_enter()) {
        InterlockedExchange(
            &g_gu_behavior.shutdown_terminal, 1);
        InterlockedExchange(&g_gu_behavior.enabled, 0);
        idle_event = g_gu_behavior.manager_setter_idle_event;
        wait_required = (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_worker_active, 0, 0);
        gu_behavior_lock_leave();
    } else {
        InterlockedExchange(&g_gu_behavior.enabled, 0);
        InterlockedIncrement(
            &g_gu_behavior.manager_setter_rundown_failures);
        return 0;
    }
    if (wait_required != 0u &&
        (!idle_event || WaitForSingleObject(
            idle_event,
            GU_MANAGER_SETTER_RUNDOWN_TIMEOUT_MS) != WAIT_OBJECT_0)) {
        InterlockedIncrement(
            &g_gu_behavior.manager_setter_rundown_failures);
        return 0;
    }
    return 1;
}

void gu_behavior_set_gateways(
    GuCreateGateway create_gateway,
    GuManagerInitGateway manager_init_gateway,
    GuPlantDistanceSetter plant_distance_setter)
{
    g_gu_behavior.create_gateway = create_gateway;
    g_gu_behavior.manager_init_gateway = manager_init_gateway;
    g_gu_behavior.plant_distance_setter = plant_distance_setter;
}

/* Publish the newest desired PLANT distances while the lifecycle lock is
 * held.  Exactly one callback becomes the out-of-lock setter worker; other
 * callbacks only replace the desired generation and return. */
static DWORD gu_manager_schedule_setter_locked(
    DWORD near_bits, DWORD far_bits,
    DWORD *generation_out)
{
    DWORD generation;
    DWORD become_worker = 0u;
    if (generation_out) {
        *generation_out = 0u;
    }
    if (!g_gu_behavior.plant_distance_setter || !generation_out) {
        return 0u;
    }
    generation = (DWORD)InterlockedCompareExchange(
        &g_gu_behavior.manager_setter_generation, 0, 0);
    if (generation == 0xFFFFFFFFu) {
        InterlockedIncrement(
            &g_gu_behavior.manager_setter_generation_exhaustions);
        InterlockedIncrement(&g_gu_behavior.manager_failures);
        return 0u;
    }
    if (InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_worker_active, 0, 0) == 0) {
        if (!g_gu_behavior.manager_setter_idle_event ||
            !ResetEvent(g_gu_behavior.manager_setter_idle_event)) {
            InterlockedIncrement(
                &g_gu_behavior.manager_setter_rundown_failures);
            InterlockedIncrement(&g_gu_behavior.manager_failures);
            return 0u;
        }
        become_worker = 1u;
    } else {
        InterlockedIncrement(
            &g_gu_behavior.manager_setter_coalesced_updates);
    }
    g_gu_behavior.manager_setter_desired_near_bits = near_bits;
    g_gu_behavior.manager_setter_desired_far_bits = far_bits;
    InterlockedExchange(
        &g_gu_behavior.manager_setter_generation,
        (LONG)(generation + 1u));
    if (become_worker != 0u) {
        InterlockedExchange(
            &g_gu_behavior.manager_setter_worker_active, 1);
    }
    *generation_out = generation + 1u;
    return become_worker != 0u ?
        GU_MANAGER_SETTER_SCHEDULE_WORKER :
        GU_MANAGER_SETTER_SCHEDULE_WAITER;
}

/* Drain the latest published generation without holding an ASI lock across
 * the exact engine leaf setter.  A concurrent update is coalesced and applied
 * by this worker before it publishes the idle state. */
static int gu_manager_drain_setter(DWORD required_generation)
{
    DWORD applied_generation = 0u;
    DWORD calls_drained = 0u;
    for (;;) {
        DWORD generation;
        DWORD near_bits;
        DWORD far_bits;
        GuPlantDistanceSetter setter;
        if (!gu_behavior_lock_enter()) {
            InterlockedIncrement(
                &g_gu_behavior.manager_setter_rundown_failures);
            return 0;
        }
        if (InterlockedCompareExchange(
                &g_gu_behavior.manager_setter_worker_active,
                0, 0) == 0) {
            gu_behavior_lock_leave();
            return (DWORD)InterlockedCompareExchange(
                &g_gu_behavior.manager_setter_applied_generation,
                0, 0) >= required_generation;
        }
        if (applied_generation != 0u) {
            InterlockedExchange(
                &g_gu_behavior.manager_setter_applied_generation,
                (LONG)applied_generation);
        }
        generation = (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation, 0, 0);
        if (InterlockedCompareExchange(
                &g_gu_behavior.enabled, 0, 0) == 0 ||
            generation == applied_generation) {
            InterlockedExchange(
                &g_gu_behavior.manager_setter_worker_active, 0);
            if (!SetEvent(
                    g_gu_behavior.manager_setter_idle_event)) {
                InterlockedIncrement(
                    &g_gu_behavior.manager_setter_rundown_failures);
            }
            gu_behavior_lock_leave();
            return applied_generation >= required_generation;
        }
        if (calls_drained >=
                GU_MANAGER_SETTER_MAX_CALLS_PER_CALLBACK) {
            /* Bound work on this exact engine callback.  A coalesced caller
             * owns every later published generation and will claim the idle
             * worker token before applying it. */
            InterlockedExchange(
                &g_gu_behavior.manager_setter_worker_active, 0);
            InterlockedIncrement(
                &g_gu_behavior.manager_setter_handoffs);
            if (!SetEvent(
                    g_gu_behavior.manager_setter_idle_event)) {
                InterlockedIncrement(
                    &g_gu_behavior.manager_setter_rundown_failures);
            }
            gu_behavior_lock_leave();
            return applied_generation >= required_generation;
        }
        near_bits =
            g_gu_behavior.manager_setter_desired_near_bits;
        far_bits =
            g_gu_behavior.manager_setter_desired_far_bits;
        setter = g_gu_behavior.plant_distance_setter;
        if (!setter) {
            InterlockedIncrement(&g_gu_behavior.manager_failures);
            InterlockedExchange(
                &g_gu_behavior.manager_setter_worker_active, 0);
            if (!SetEvent(
                    g_gu_behavior.manager_setter_idle_event)) {
                InterlockedIncrement(
                    &g_gu_behavior.manager_setter_rundown_failures);
            }
            gu_behavior_lock_leave();
            return 0;
        }
        gu_behavior_lock_leave();
        setter(
            gu_bits_float(near_bits),
            gu_bits_float(far_bits));
        InterlockedIncrement(
            &g_gu_behavior.manager_setter_calls);
        applied_generation = generation;
        ++calls_drained;
    }
}

static DWORD gu_manager_claim_pending_worker(
    DWORD required_generation)
{
    DWORD result = GU_MANAGER_SETTER_SCHEDULE_FAILED;
    if (!gu_behavior_lock_enter()) {
        return result;
    }
    if ((DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_applied_generation,
            0, 0) >= required_generation) {
        result = GU_MANAGER_SETTER_SCHEDULE_COMPLETE;
    } else if (InterlockedCompareExchange(
                   &g_gu_behavior.enabled, 0, 0) != 0 &&
               InterlockedCompareExchange(
                   &g_gu_behavior.manager_setter_worker_active,
                   0, 0) == 0 &&
               g_gu_behavior.manager_setter_idle_event &&
               ResetEvent(
                   g_gu_behavior.manager_setter_idle_event)) {
        InterlockedExchange(
            &g_gu_behavior.manager_setter_worker_active, 1);
        result = GU_MANAGER_SETTER_SCHEDULE_WORKER;
    } else if (InterlockedCompareExchange(
                   &g_gu_behavior.manager_setter_worker_active,
                   0, 0) != 0) {
        result = GU_MANAGER_SETTER_SCHEDULE_WAITER;
    }
    gu_behavior_lock_leave();
    return result;
}

static int gu_manager_wait_setter(DWORD required_generation)
{
    HANDLE idle_event = g_gu_behavior.manager_setter_idle_event;
    DWORD attempt;
    InterlockedIncrement(&g_gu_behavior.manager_setter_waits);
    if (!idle_event) {
        goto failed;
    }
    /* The idle event is the shutdown-rundown signal, not a per-generation
     * condition.  Recheck the monotonic generation at short bounded
     * intervals so a later ResetEvent cannot hide completion of this exact
     * caller's generation. */
    for (attempt = 0u; attempt < 500u; ++attempt) {
        DWORD wait_result;
        if ((DWORD)InterlockedCompareExchange(
                &g_gu_behavior.manager_setter_applied_generation,
                0, 0) >= required_generation) {
            return 1;
        }
        if (InterlockedCompareExchange(
                &g_gu_behavior.manager_setter_worker_active,
                0, 0) == 0) {
            DWORD claim = gu_manager_claim_pending_worker(
                required_generation);
            if (claim == GU_MANAGER_SETTER_SCHEDULE_COMPLETE) {
                return 1;
            }
            if (claim == GU_MANAGER_SETTER_SCHEDULE_WORKER) {
                return gu_manager_drain_setter(
                    required_generation);
            }
            if (claim == GU_MANAGER_SETTER_SCHEDULE_FAILED) {
                goto failed;
            }
        }
        wait_result = WaitForSingleObject(idle_event, 10u);
        if (wait_result != WAIT_OBJECT_0 &&
            wait_result != WAIT_TIMEOUT) {
            goto failed;
        }
        if ((DWORD)InterlockedCompareExchange(
                &g_gu_behavior.manager_setter_applied_generation,
                0, 0) >= required_generation) {
            return 1;
        }
    }
failed:
    InterlockedIncrement(
        &g_gu_behavior.manager_setter_wait_failures);
    return 0;
}

static void gu_manager_scale(
    BYTE *manager, DWORD sync_plant_setter)
{
    struct GuDistancePlan plan;
    float multiplier;
    float original_near;
    float original_far;
    float original_query;
    DWORD existing_manager;
    DWORD setter_schedule = GU_MANAGER_SETTER_SCHEDULE_FAILED;
    DWORD setter_generation = 0u;
    if (!gu_behavior_lock_enter()) {
        InterlockedIncrement(&g_gu_behavior.manager_failures);
        return;
    }
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) == 0 ||
        !gu_address_writable(
            manager, GU_MANAGER_FAR_SQUARED_OFFSET + 4u)) {
        InterlockedIncrement(&g_gu_behavior.manager_failures);
        goto finished;
    }
    multiplier = gu_bits_float(
        g_gu_behavior.distance_multiplier_bits);
    existing_manager = (DWORD)InterlockedCompareExchange(
        &g_gu_behavior.manager_key, 0, 0);
    original_near = existing_manager == (DWORD)manager ?
        gu_bits_float(g_gu_behavior.manager_original_near_bits) :
        gu_bits_float(
            gu_read_u32(manager + GU_MANAGER_NEAR_OFFSET));
    original_far = gu_bits_float(
        gu_read_u32(manager + GU_MANAGER_FAR_OFFSET));
    original_query = gu_bits_float(
        gu_read_u32(manager + GU_MANAGER_QUERY_OFFSET));
    if (existing_manager == (DWORD)manager &&
        gu_read_u32(manager + GU_MANAGER_NEAR_OFFSET) ==
            g_gu_behavior.manager_scaled_near_bits &&
        gu_read_u32(manager + GU_MANAGER_FAR_OFFSET) ==
            g_gu_behavior.manager_scaled_far_bits &&
        gu_read_u32(manager + GU_MANAGER_QUERY_OFFSET) ==
            g_gu_behavior.manager_scaled_query_bits &&
        gu_read_u32(manager + GU_MANAGER_FAR_SQUARED_OFFSET) ==
            g_gu_behavior.manager_scaled_far_squared_bits) {
        if (sync_plant_setter &&
            g_gu_behavior.plant_distance_setter) {
            setter_schedule =
                gu_manager_schedule_setter_locked(
                    g_gu_behavior.manager_scaled_near_bits,
                    g_gu_behavior.manager_scaled_far_bits,
                    &setter_generation);
        }
        goto finished;
    }
    if (!gu_make_distance_plan(
            original_near, original_far, original_query,
            multiplier, &plan)) {
        InterlockedIncrement(&g_gu_behavior.manager_failures);
        goto finished;
    }
    if (sync_plant_setter &&
        g_gu_behavior.plant_distance_setter) {
        setter_schedule =
            gu_manager_schedule_setter_locked(
                gu_float_bits(plan.scaled_near),
                gu_float_bits(plan.scaled_far),
                &setter_generation);
        if (setter_schedule ==
                GU_MANAGER_SETTER_SCHEDULE_FAILED) {
            goto finished;
        }
    }
    if (existing_manager != (DWORD)manager) {
        g_gu_behavior.manager_original_near_bits =
            gu_float_bits(plan.original_near);
    }
    g_gu_behavior.manager_original_far_bits =
        gu_float_bits(plan.original_far);
    g_gu_behavior.manager_original_query_bits =
        gu_float_bits(plan.original_query);
    g_gu_behavior.manager_original_far_squared_bits =
        gu_read_u32(manager + GU_MANAGER_FAR_SQUARED_OFFSET);
    g_gu_behavior.manager_scaled_near_bits =
        gu_float_bits(plan.scaled_near);
    g_gu_behavior.manager_scaled_far_bits =
        gu_float_bits(plan.scaled_far);
    g_gu_behavior.manager_scaled_query_bits =
        gu_float_bits(plan.scaled_query);
    g_gu_behavior.manager_scaled_far_squared_bits =
        gu_float_bits(plan.scaled_far_squared);
    gu_write_u32(
        manager + GU_MANAGER_NEAR_OFFSET,
        gu_float_bits(plan.scaled_near));
    gu_write_u32(
        manager + GU_MANAGER_FAR_OFFSET,
        gu_float_bits(plan.scaled_far));
    gu_write_u32(
        manager + GU_MANAGER_QUERY_OFFSET,
        gu_float_bits(plan.scaled_query));
    gu_write_u32(
        manager + GU_MANAGER_FAR_SQUARED_OFFSET,
        gu_float_bits(plan.scaled_far_squared));
    InterlockedExchange(
        &g_gu_behavior.manager_key, (LONG)(DWORD)manager);
    InterlockedIncrement(&g_gu_behavior.manager_updates);

finished:
    gu_behavior_lock_leave();
    if (setter_schedule == GU_MANAGER_SETTER_SCHEDULE_WORKER) {
        if (!gu_manager_drain_setter(setter_generation)) {
            InterlockedIncrement(&g_gu_behavior.manager_failures);
        }
    } else if (setter_schedule ==
                   GU_MANAGER_SETTER_SCHEDULE_WAITER) {
        if (!gu_manager_wait_setter(setter_generation)) {
            InterlockedIncrement(&g_gu_behavior.manager_failures);
        }
    }
}

void GU_FASTCALL gu_manager_postscale_hook(
    BYTE *manager, DWORD ignored_edx)
{
    (void)ignored_edx;
    gu_manager_scale(manager, 1u);
}

void GU_FASTCALL gu_manager_prebuild_hook(
    BYTE *manager, DWORD ignored_edx)
{
    (void)ignored_edx;
    gu_manager_scale(manager, 0u);
}

int gu_behavior_restore_manager(void)
{
    BYTE *manager;
    GuPlantDistanceSetter plant_setter = NULL;
    float plant_near = 0.0f;
    float plant_far = 0.0f;
    int result = 0;
    if (!gu_behavior_lock_enter()) {
        InterlockedIncrement(
            &g_gu_behavior.manager_restore_mismatches);
        return 0;
    }
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) != 0 ||
        InterlockedCompareExchange(
            &g_gu_behavior.shutdown_terminal, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_worker_active,
            0, 0) != 0) {
        InterlockedIncrement(
            &g_gu_behavior.manager_restore_mismatches);
        goto finished;
    }
    manager = (BYTE *)(DWORD)InterlockedCompareExchange(
        &g_gu_behavior.manager_key, 0, 0);
    if (!manager) {
        result = 1;
        goto finished;
    }
    if (!gu_address_writable(
            manager, GU_MANAGER_FAR_SQUARED_OFFSET + 4u)) {
        InterlockedIncrement(
            &g_gu_behavior.manager_restore_mismatches);
        goto finished;
    }
    if (gu_read_u32(manager + GU_MANAGER_NEAR_OFFSET) !=
            g_gu_behavior.manager_scaled_near_bits ||
        gu_read_u32(manager + GU_MANAGER_FAR_OFFSET) !=
            g_gu_behavior.manager_scaled_far_bits ||
        gu_read_u32(manager + GU_MANAGER_QUERY_OFFSET) !=
            g_gu_behavior.manager_scaled_query_bits ||
        gu_read_u32(manager + GU_MANAGER_FAR_SQUARED_OFFSET) !=
            g_gu_behavior.manager_scaled_far_squared_bits) {
        InterlockedIncrement(
            &g_gu_behavior.manager_restore_mismatches);
        goto finished;
    }
    gu_write_u32(
        manager + GU_MANAGER_NEAR_OFFSET,
        g_gu_behavior.manager_original_near_bits);
    gu_write_u32(
        manager + GU_MANAGER_FAR_OFFSET,
        g_gu_behavior.manager_original_far_bits);
    gu_write_u32(
        manager + GU_MANAGER_QUERY_OFFSET,
        g_gu_behavior.manager_original_query_bits);
    gu_write_u32(
        manager + GU_MANAGER_FAR_SQUARED_OFFSET,
        g_gu_behavior.manager_original_far_squared_bits);
    if (g_gu_behavior.plant_distance_setter) {
        plant_setter = g_gu_behavior.plant_distance_setter;
        plant_near = gu_bits_float(
            g_gu_behavior.manager_original_near_bits);
        plant_far = gu_bits_float(
            g_gu_behavior.manager_original_far_bits);
    }
    InterlockedExchange(&g_gu_behavior.manager_key, 0);
    result = 1;

finished:
    gu_behavior_lock_leave();
    if (result && plant_setter) {
        plant_setter(plant_near, plant_far);
    }
    return result;
}

DWORD GU_FASTCALL gu_manager_init_hook(
    BYTE *manager, DWORD ignored_edx)
{
    GuManagerInitGateway gateway =
        g_gu_behavior.manager_init_gateway;
    DWORD result;
    if (!gateway) {
        return 0u;
    }
    result = gateway(manager, ignored_edx);
    result &= 0xFFu;
    if (result != 0u && InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) != 0) {
        gu_manager_postscale_hook(manager, ignored_edx);
    }
    return result;
}

static int gu_float_is_finite_positive(float value)
{
    DWORD bits = gu_float_bits(value);
    return (bits & 0x80000000u) == 0u &&
           (bits & 0x7F800000u) != 0x7F800000u &&
           value > 0.0f;
}

static int gu_make_owned_distance(
    DWORD original_bits, DWORD *scaled_bits_out,
    DWORD *clamped_out)
{
    float original;
    float multiplier;
    float scaled;
    float coverage;

    if (!scaled_bits_out || !clamped_out) {
        return 0;
    }
    original = gu_bits_float(original_bits);
    multiplier = gu_bits_float(
        g_gu_behavior.distance_multiplier_bits);
    if (!gu_float_is_finite_positive(original) ||
        !gu_float_is_finite_positive(multiplier)) {
        return 0;
    }
    *clamped_out = 0u;
    if (g_gu_behavior.distance_multiplier_bits == 0x3F800000u) {
        *scaled_bits_out = original_bits;
        return 1;
    }
    scaled = gu_scale_instance_distance(original, multiplier);
    coverage = GU_STOCK_SOURCE_RADIUS * multiplier;
    if (!gu_float_is_finite_positive(scaled) ||
        !gu_float_is_finite_positive(coverage)) {
        return 0;
    }
    if (scaled > coverage) {
        scaled = coverage;
        *clamped_out = 1u;
    }
    /* Coverage bounds must never turn an extension request into a reduction
     * of the stock/authored per-instance range. */
    if (scaled < original) {
        scaled = original;
        *clamped_out = 1u;
    }
    *scaled_bits_out = gu_float_bits(scaled);
    return 1;
}

static void gu_commit_procobj_owned_wrapper(
    BYTE *wrapper, DWORD surface)
{
    BYTE *entity = NULL;
    volatile LONG *distance = NULL;
    struct GuOwnedRecord existing;
    DWORD original_bits = 0u;
    DWORD scaled_bits = 0u;
    DWORD token = 0u;
    DWORD flags = GU_OWNER_SURFACE;
    DWORD clamped = 0u;
    DWORD emit_event = 0u;

#if defined(GTAIV_GRASS_BEHAVIOR_TESTING)
    gu_behavior_test_pause(
        &g_gu_behavior_test_pause_commit_before_lock,
        &g_gu_behavior_test_commit_before_lock_reached);
#endif
    if (!gu_behavior_lock_enter()) {
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        return;
    }
    /* The public callback performs a fast enabled check, but it can then wait
     * behind a release/restore owner.  Revalidate under the lifecycle lock so
     * a callback admitted just before disable cannot publish after shutdown. */
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) == 0) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_shutdown_commit_skips);
        goto finished;
    }
    if ((DWORD)wrapper <= GU_RESERVED_KEY_MAX ||
        !gu_address_readable(
            wrapper, GU_WRAPPER_SOURCE_OFFSET + 4u) ||
        surface <= GU_RESERVED_KEY_MAX ||
        gu_read_u32(wrapper + GU_WRAPPER_SURFACE_OFFSET) != surface) {
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    entity = (BYTE *)gu_read_u32(wrapper + GU_WRAPPER_ENTITY_OFFSET);
    if ((DWORD)entity <= GU_RESERVED_KEY_MAX ||
        !gu_address_writable(
            entity, GU_ENTITY_DISTANCE_OFFSET + 4u)) {
        InterlockedIncrement(
            &g_gu_behavior.distance_scaling_disabled);
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    distance = (volatile LONG *)(void *)(
        entity + GU_ENTITY_DISTANCE_OFFSET);
    original_bits = (DWORD)InterlockedCompareExchange(
        distance, 0, 0);
    if (!gu_make_owned_distance(
            original_bits, &scaled_bits, &clamped)) {
        InterlockedIncrement(
            &g_gu_behavior.distance_scaling_disabled);
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    /* A live key is never replaced.  A later pointer reuse is accepted only
     * after the exact earlier token has been taken by the release hook. */
    if (gu_ownership_find_wrapper(
            &g_gu_ownership, (DWORD)wrapper,
            &existing, NULL) ||
        gu_ownership_find_entity(
            &g_gu_ownership, (DWORD)entity,
            &existing, NULL)) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_reuse_refusals);
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    if (scaled_bits != original_bits) {
        flags |= GU_OWNER_DISTANCE_APPLIED;
    }
    if (!gu_ownership_insert(
            &g_gu_ownership,
            (DWORD)wrapper, (DWORD)entity,
            surface, 0u, original_bits, scaled_bits,
            flags, &token)) {
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    InterlockedIncrement(
        &g_gu_behavior.ownership_publications);
#if defined(GTAIV_GRASS_BEHAVIOR_TESTING)
    gu_behavior_test_pause(
        &g_gu_behavior_test_pause_commit_after_publish,
        &g_gu_behavior_test_commit_publish_reached);
#endif
    if ((flags & GU_OWNER_DISTANCE_APPLIED) != 0u &&
        (DWORD)InterlockedCompareExchange(
            distance, (LONG)scaled_bits,
            (LONG)original_bits) != original_bits) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_publish_cas_losses);
        if (!gu_ownership_remove(
                &g_gu_ownership, (DWORD)wrapper,
                (DWORD)entity, token)) {
            InterlockedIncrement(
                &g_gu_behavior.ownership_publish_cleanup_failures);
        }
        InterlockedIncrement(
            &g_gu_behavior.distance_scaling_disabled);
        InterlockedIncrement(&g_gu_behavior.ownership_failures);
        goto finished;
    }
    if ((flags & GU_OWNER_DISTANCE_APPLIED) != 0u) {
        InterlockedIncrement(&g_gu_behavior.entities_scaled);
        InterlockedIncrement(
            &g_gu_behavior.distance_applied_active);
    }
    if (clamped) {
        InterlockedIncrement(&g_gu_behavior.entities_clamped);
    }
    emit_event = 1u;

finished:
    gu_behavior_lock_leave();
    if (emit_event) {
        gu_emit_entity_event(
            GU_EVENT_ENTITY_CREATE,
            (DWORD)wrapper, (DWORD)entity,
            surface, 0u, original_bits, scaled_bits);
    }
}

void GU_FASTCALL gu_commit_plant(
    BYTE *wrapper, DWORD source_owner)
{
    /* PLANT has its own near/far setter and is never entity-scaled or entered
     * into the generated-PROCOBJ ownership sidecar. */
    (void)wrapper;
    (void)source_owner;
}

void GU_FASTCALL gu_commit_procobj(
    BYTE *wrapper, DWORD surface_owner)
{
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) != 0 ||
        InterlockedCompareExchange(
            &g_gu_behavior.shutdown_terminal, 0, 0) == 0) {
        gu_commit_procobj_owned_wrapper(
            wrapper, surface_owner);
    }
}

void *GU_FASTCALL gu_create_hook(
    BYTE *manager, DWORD ignored_edx,
    DWORD argument1, DWORD argument2, DWORD argument3,
    DWORD argument4, DWORD argument5)
{
    BYTE *wrapper;
    GuCreateGateway gateway = g_gu_behavior.create_gateway;
    if (!gateway) {
        return NULL;
    }
    wrapper = (BYTE *)gateway(
        manager, ignored_edx,
        argument1, argument2, argument3,
        argument4, argument5);
    return wrapper;
}

static void gu_take_releasing_wrapper(BYTE *wrapper)
{
    struct GuOwnedRecord record;
    DWORD restored = 0u;
    DWORD emit_event = 0u;
    if (!gu_behavior_lock_enter()) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_unowned_release_skips);
        return;
    }
    if (!gu_ownership_take_wrapper(
            &g_gu_ownership, (DWORD)wrapper, &record)) {
        InterlockedIncrement(
            &g_gu_behavior.ownership_unowned_release_skips);
        goto finished;
    }
    InterlockedIncrement(
        &g_gu_behavior.ownership_release_takes);
#if defined(GTAIV_GRASS_BEHAVIOR_TESTING)
    gu_behavior_test_pause(
        &g_gu_behavior_test_pause_release_after_take,
        &g_gu_behavior_test_release_take_reached);
#endif
    if ((record.flags & GU_OWNER_DISTANCE_APPLIED) != 0u) {
        DWORD live_entity = 0u;
        DWORD observed = 0u;
        if (gu_address_readable(
                wrapper, GU_WRAPPER_ENTITY_OFFSET + 4u)) {
            live_entity = (DWORD)InterlockedCompareExchange(
                (volatile LONG *)(void *)(
                    wrapper + GU_WRAPPER_ENTITY_OFFSET),
                0, 0);
        }
        if (live_entity == (DWORD)record.entity_key &&
            gu_address_writable(
                (BYTE *)live_entity,
                GU_ENTITY_DISTANCE_OFFSET + 4u)) {
            observed = (DWORD)InterlockedCompareExchange(
                (volatile LONG *)(void *)(
                    (BYTE *)live_entity +
                    GU_ENTITY_DISTANCE_OFFSET),
                (LONG)record.original_distance_bits,
                (LONG)record.scaled_distance_bits);
        }
        if (live_entity == (DWORD)record.entity_key &&
            observed == record.scaled_distance_bits) {
            InterlockedIncrement(&g_gu_behavior.entities_restored);
            InterlockedIncrement(&g_gu_behavior.release_restores);
            restored = 1u;
        } else {
            if (live_entity == (DWORD)record.entity_key &&
                observed != 0u &&
                observed != record.scaled_distance_bits) {
                InterlockedIncrement(
                    &g_gu_behavior.
                        ownership_third_party_relinquishments);
            }
            InterlockedIncrement(
                &g_gu_behavior.entity_restore_mismatches);
            InterlockedIncrement(
                &g_gu_behavior.release_restore_mismatches);
        }
        InterlockedDecrement(
            &g_gu_behavior.distance_applied_active);
    }
    emit_event = 1u;

finished:
    gu_behavior_lock_leave();
    if (emit_event) {
        gu_emit_entity_event(
            GU_EVENT_ENTITY_RELEASE,
            (DWORD)wrapper, (DWORD)record.entity_key,
            record.surface_owner, record.source_owner,
            record.original_distance_bits,
            restored ? record.original_distance_bits :
                record.scaled_distance_bits);
    }
}

void GU_FASTCALL gu_release_observer_hook(
    BYTE *wrapper, DWORD ignored_edx)
{
    (void)ignored_edx;
    if (wrapper) {
        gu_take_releasing_wrapper(wrapper);
    }
}

void gu_behavior_restore_all_owned(void)
{
    DWORD index;
    if (!gu_behavior_lock_enter()) {
        InterlockedIncrement(
            &g_gu_behavior.clean_shutdown_imbalances);
        return;
    }
    if (InterlockedCompareExchange(
            &g_gu_behavior.enabled, 0, 0) != 0 ||
        InterlockedCompareExchange(
            &g_gu_behavior.shutdown_terminal, 0, 0) == 0) {
        InterlockedIncrement(
            &g_gu_behavior.clean_shutdown_imbalances);
        gu_behavior_lock_leave();
        return;
    }
    for (index = 0u; index < GU_OWNERSHIP_CAPACITY; ++index) {
        struct GuOwnedRecord *record =
            &g_gu_ownership.records[index];
        DWORD wrapper = (DWORD)InterlockedCompareExchange(
            &record->wrapper_key, 0, 0);
        DWORD entity = (DWORD)record->entity_key;
        if (wrapper > GU_RESERVED_KEY_MAX &&
            entity > GU_RESERVED_KEY_MAX) {
            DWORD distance_applied =
                record->flags & GU_OWNER_DISTANCE_APPLIED;
            DWORD token = record->token;
            if (distance_applied != 0u) {
                BYTE *entity_pointer = (BYTE *)entity;
                if (gu_address_writable(
                        entity_pointer,
                        GU_ENTITY_DISTANCE_OFFSET + 4u) &&
                    (DWORD)InterlockedCompareExchange(
                        (volatile LONG *)(void *)(
                            entity_pointer +
                            GU_ENTITY_DISTANCE_OFFSET),
                        (LONG)record->original_distance_bits,
                        (LONG)record->scaled_distance_bits) ==
                            record->scaled_distance_bits) {
                    InterlockedIncrement(
                        &g_gu_behavior.entities_restored);
                } else {
                    InterlockedIncrement(
                        &g_gu_behavior.entity_restore_mismatches);
                }
            }
            /* The lifecycle lock makes removal deterministic.  Only retire
             * the applied-distance balance after the exact token was removed;
             * an unexpected table inconsistency must remain visible as a
             * shutdown imbalance instead of being hidden by the counter. */
            if (gu_ownership_remove(
                    &g_gu_ownership, wrapper, entity, token)) {
                if (distance_applied != 0u) {
                    InterlockedDecrement(
                        &g_gu_behavior.distance_applied_active);
                }
            } else {
                InterlockedIncrement(
                    &g_gu_behavior.ownership_failures);
            }
        }
    }
    if (InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0) == 0 &&
        InterlockedCompareExchange(
            &g_gu_behavior.distance_applied_active, 0, 0) == 0) {
        InterlockedIncrement(
            &g_gu_behavior.clean_shutdown_balances);
    } else {
        InterlockedIncrement(
            &g_gu_behavior.clean_shutdown_imbalances);
    }
    gu_behavior_lock_leave();
}
