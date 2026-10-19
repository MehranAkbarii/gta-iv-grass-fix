#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_pipeline.h"

struct GuEventRing g_gu_event_ring;
struct GuEventDrainSummary g_gu_event_summary;
struct GuOwnershipTable g_gu_ownership;
DWORD g_gu_source_buffer[GU_SOURCE_BUFFER_CAPACITY];
float g_gu_source_radius = 30.0f;

static int gu_finite_nonnegative(float value)
{
    DWORD bits = *(const DWORD *)(const void *)&value;
    return (bits & 0x80000000u) == 0u &&
           (bits & 0x7F800000u) != 0x7F800000u;
}

static int gu_finite_positive(float value)
{
    return gu_finite_nonnegative(value) && value > 0.0f;
}

static float gu_sqrt(float value)
{
    float estimate;
    DWORD index;

    if (!gu_finite_positive(value)) {
        return 0.0f;
    }
    estimate = value >= 1.0f ? value : 1.0f;
    for (index = 0u; index < 12u; ++index) {
        estimate = 0.5f * (estimate + value / estimate);
    }
    return estimate;
}

static DWORD gu_hash_pointer(DWORD value)
{
    value >>= 3;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    return value;
}

static void gu_update_high_watermark(volatile LONG *value, LONG candidate)
{
    LONG observed = InterlockedCompareExchange(value, 0, 0);
    while (candidate > observed) {
        LONG previous = InterlockedCompareExchange(
            value, candidate, observed);
        if (previous == observed) {
            return;
        }
        observed = previous;
    }
}

void gu_event_ring_initialize(struct GuEventRing *ring)
{
    DWORD index;
    if (!ring) {
        return;
    }
    ZeroMemory(ring, sizeof(*ring));
    for (index = 0u; index < GU_EVENT_RING_CAPACITY; ++index) {
        ring->slots[index].sequence = (LONG)index;
    }
    ring->enabled = 1;
}

int gu_event_try_emit(
    struct GuEventRing *ring, const struct GuRawEvent *event)
{
    LONG position;
    DWORD retries;
    DWORD type;

    if (!ring || !event) {
        return 0;
    }
    type = event->type < GU_EVENT_TYPE_COUNT ? event->type : 0u;
    if (InterlockedCompareExchange(&ring->enabled, 0, 0) == 0) {
        InterlockedIncrement(&ring->lost_disabled[type]);
        return 0;
    }

    position = InterlockedCompareExchange(
        &ring->enqueue_position, 0, 0);
    for (retries = 0u; retries < GU_EVENT_CAS_RETRIES; ++retries) {
        struct GuEventSlot *slot =
            &ring->slots[(DWORD)position & GU_EVENT_RING_MASK];
        LONG sequence = InterlockedCompareExchange(
            &slot->sequence, 0, 0);
        LONG difference = sequence - position;
        if (difference == 0) {
            LONG previous = InterlockedCompareExchange(
                &ring->enqueue_position, position + 1, position);
            if (previous == position) {
                slot->event = *event;
                slot->event.sequence = (DWORD)position;
                InterlockedExchange(&slot->sequence, position + 1);
                return 1;
            }
            position = previous;
        } else if (difference < 0) {
            InterlockedIncrement(&ring->lost_full[type]);
            return 0;
        } else {
            position = InterlockedCompareExchange(
                &ring->enqueue_position, 0, 0);
        }
    }
    InterlockedIncrement(&ring->lost_contention[type]);
    return 0;
}

int gu_event_try_consume(
    struct GuEventRing *ring, struct GuRawEvent *event)
{
    LONG position;
    struct GuEventSlot *slot;
    LONG sequence;

    if (!ring || !event) {
        return 0;
    }
    position = InterlockedCompareExchange(
        &ring->dequeue_position, 0, 0);
    slot = &ring->slots[(DWORD)position & GU_EVENT_RING_MASK];
    sequence = InterlockedCompareExchange(&slot->sequence, 0, 0);
    if (sequence - (position + 1) != 0) {
        return 0;
    }
    if (InterlockedCompareExchange(
            &ring->dequeue_position, position + 1,
            position) != position) {
        return 0;
    }
    *event = slot->event;
    InterlockedExchange(
        &slot->sequence, position + (LONG)GU_EVENT_RING_CAPACITY);
    return 1;
}

void gu_event_summary_initialize(
    struct GuEventDrainSummary *summary)
{
    if (summary) {
        ZeroMemory(summary, sizeof(*summary));
    }
}

DWORD gu_event_drain(
    struct GuEventRing *ring,
    struct GuEventDrainSummary *summary,
    DWORD maximum_events)
{
    struct GuRawEvent event;
    DWORD consumed = 0u;
    if (!ring || !summary || maximum_events == 0u) {
        return 0u;
    }
    while (consumed < maximum_events &&
           gu_event_try_consume(ring, &event)) {
        DWORD type = event.type < GU_EVENT_TYPE_COUNT ?
            event.type : GU_EVENT_NONE;
        summary->last_by_type[type] = event;
        InterlockedCompareExchange(
            &summary->consumed_total, 0, 0);
        InterlockedIncrement(
            &summary->consumed_by_type[type]);
        InterlockedIncrement(&summary->consumed_total);
        ++consumed;
    }
    return consumed;
}

void gu_ownership_initialize(struct GuOwnershipTable *table)
{
    if (!table) {
        return;
    }
    ZeroMemory(table, sizeof(*table));
    table->next_token = (LONG)GU_RESERVED_KEY_MAX;
}

static int gu_allocate_ownership_token(
    struct GuOwnershipTable *table, DWORD *token_out)
{
    DWORD attempt;
    if (!table || !token_out ||
        InterlockedCompareExchange(
            &table->token_exhausted, 0, 0) != 0) {
        return 0;
    }
    for (attempt = 0u; attempt < GU_OWNERSHIP_RETRIES; ++attempt) {
        DWORD observed = (DWORD)InterlockedCompareExchange(
            &table->next_token, 0, 0);
        DWORD next;
        if (observed < GU_RESERVED_KEY_MAX || observed == 0xFFFFFFFFu) {
            if (InterlockedCompareExchange(
                    &table->token_exhausted, 1, 0) == 0) {
                InterlockedIncrement(&table->token_exhaustions);
            }
            return 0;
        }
        next = observed + 1u;
        if ((DWORD)InterlockedCompareExchange(
                &table->next_token, (LONG)next,
                (LONG)observed) == observed) {
            *token_out = next;
            return 1;
        }
    }
    return 0;
}

static int gu_claim_key(volatile LONG *key)
{
    LONG observed = InterlockedCompareExchange(key, 0, 0);
    if (observed == 0 || (DWORD)observed == GU_TOMBSTONE_KEY) {
        return InterlockedCompareExchange(
            key, (LONG)GU_CLAIMED_KEY, observed) == observed;
    }
    return 0;
}

static void gu_ownership_decrement_active(
    struct GuOwnershipTable *table)
{
    LONG active = InterlockedCompareExchange(
        &table->active_count, 0, 0);
    while (active > 0) {
        LONG previous = InterlockedCompareExchange(
            &table->active_count, active - 1, active);
        if (previous == active) {
            return;
        }
        active = previous;
    }
}

static void gu_clear_owned_record(
    struct GuOwnedRecord *record)
{
    record->entity_key = 0;
    record->surface_owner = 0u;
    record->source_owner = 0u;
    record->original_distance_bits = 0u;
    record->scaled_distance_bits = 0u;
    record->flags = 0u;
    record->token = 0;
}

static int gu_publish_entity_index(
    struct GuOwnershipTable *table, DWORD entity,
    DWORD record_index, DWORD token)
{
    DWORD start = gu_hash_pointer(entity) & GU_OWNERSHIP_MASK;
    DWORD attempt;
    DWORD probe;
    for (attempt = 0u; attempt < GU_OWNERSHIP_RETRIES; ++attempt) {
        int retry = 0;
        for (probe = 0u; probe < GU_OWNERSHIP_PROBES; ++probe) {
            DWORD index = (start + probe) & GU_OWNERSHIP_MASK;
            struct GuEntityIndex *entry = &table->entities[index];
            DWORD key = (DWORD)InterlockedCompareExchange(
                &entry->entity_key, 0, 0);
            if (key == entity) {
                InterlockedIncrement(&table->entity_index_failures);
                return 0;
            }
            if (key == GU_CLAIMED_KEY ||
                key == GU_RECLAIMING_KEY) {
                retry = 1;
                break;
            }
            if ((key == 0u || key == GU_TOMBSTONE_KEY) &&
                gu_claim_key(&entry->entity_key)) {
                entry->record_index_plus_one = record_index + 1u;
                entry->token = (LONG)token;
                InterlockedExchange(&entry->entity_key, (LONG)entity);
                return 1;
            }
            if (key == 0u || key == GU_TOMBSTONE_KEY) {
                retry = 1;
                break;
            }
        }
        if (!retry) {
            break;
        }
    }
    InterlockedIncrement(&table->entity_index_failures);
    return 0;
}

static int gu_reclaim_entity_index(
    struct GuOwnershipTable *table, DWORD entity,
    DWORD record_index, DWORD token)
{
    DWORD start = gu_hash_pointer(entity) & GU_OWNERSHIP_MASK;
    DWORD probe;
    for (probe = 0u; probe < GU_OWNERSHIP_PROBES; ++probe) {
        DWORD index = (start + probe) & GU_OWNERSHIP_MASK;
        struct GuEntityIndex *entry = &table->entities[index];
        DWORD key = (DWORD)InterlockedCompareExchange(
            &entry->entity_key, 0, 0);
        if (key == entity) {
            if ((DWORD)InterlockedCompareExchange(
                    &entry->entity_key,
                    (LONG)GU_RECLAIMING_KEY,
                    (LONG)entity) != entity) {
                return 0;
            }
            if ((DWORD)InterlockedCompareExchange(
                    &entry->token, 0, 0) != token ||
                entry->record_index_plus_one != record_index + 1u) {
                InterlockedExchange(
                    &entry->entity_key, (LONG)entity);
                return 0;
            }
            entry->record_index_plus_one = 0u;
            entry->token = 0;
            /* Publish the reclaimable state only after the old payload is
             * gone.  A concurrent insertion cannot observe a reusable slot
             * while this entry still carries the old token/index. */
            InterlockedExchange(
                &entry->entity_key, (LONG)GU_TOMBSTONE_KEY);
            return 1;
        }
        if (key == 0u) {
            return 0;
        }
    }
    return 0;
}

int gu_ownership_insert(
    struct GuOwnershipTable *table,
    DWORD wrapper, DWORD entity, DWORD surface, DWORD source,
    DWORD original_distance_bits, DWORD scaled_distance_bits,
    DWORD flags, DWORD *token_out)
{
    DWORD start;
    DWORD attempt;
    DWORD probe;
    DWORD token;

    if (!table || wrapper <= GU_RESERVED_KEY_MAX ||
        entity <= GU_RESERVED_KEY_MAX) {
        return 0;
    }
    start = gu_hash_pointer(wrapper) & GU_OWNERSHIP_MASK;
    if (!gu_allocate_ownership_token(table, &token)) {
        InterlockedIncrement(&table->insert_failures);
        return 0;
    }

    for (attempt = 0u; attempt < GU_OWNERSHIP_RETRIES; ++attempt) {
        int retry = 0;
        for (probe = 0u; probe < GU_OWNERSHIP_PROBES; ++probe) {
            DWORD index = (start + probe) & GU_OWNERSHIP_MASK;
            struct GuOwnedRecord *record = &table->records[index];
            DWORD key = (DWORD)InterlockedCompareExchange(
                &record->wrapper_key, 0, 0);
            if (key == wrapper) {
                InterlockedIncrement(&table->insert_failures);
                return 0;
            }
            if (key == GU_CLAIMED_KEY ||
                key == GU_RECLAIMING_KEY) {
                retry = 1;
                break;
            }
            if ((key == 0u || key == GU_TOMBSTONE_KEY) &&
                gu_claim_key(&record->wrapper_key)) {
                LONG active;
                record->entity_key = (LONG)entity;
                record->surface_owner = surface;
                record->source_owner = source;
                record->original_distance_bits = original_distance_bits;
                record->scaled_distance_bits = scaled_distance_bits;
                record->flags = flags;
                /* Token is the final payload field.  The wrapper key is the
                 * publication barrier for the complete record. */
                record->token = (LONG)token;
                if (!gu_publish_entity_index(
                        table, entity, index, token)) {
                    gu_clear_owned_record(record);
                    InterlockedExchange(
                        &record->wrapper_key,
                        (LONG)GU_TOMBSTONE_KEY);
                    return 0;
                }
                active = InterlockedIncrement(&table->active_count);
                gu_update_high_watermark(
                    &table->high_watermark, active);
                /* Account before publishing the wrapper.  A release can only
                 * acquire a published wrapper and therefore cannot decrement
                 * before this insertion has incremented the active count. */
                InterlockedExchange(
                    &record->wrapper_key, (LONG)wrapper);
                if (token_out) {
                    *token_out = token;
                }
                return 1;
            }
            if (key == 0u || key == GU_TOMBSTONE_KEY) {
                retry = 1;
                break;
            }
        }
        if (!retry) {
            break;
        }
    }
    InterlockedIncrement(&table->insert_failures);
    return 0;
}

int gu_ownership_find_wrapper(
    const struct GuOwnershipTable *table, DWORD wrapper,
    struct GuOwnedRecord *record_out, DWORD *record_index_out)
{
    DWORD start;
    DWORD attempt;
    DWORD probe;
    if (!table || wrapper <= GU_RESERVED_KEY_MAX) {
        return 0;
    }
    start = gu_hash_pointer(wrapper) & GU_OWNERSHIP_MASK;
    for (attempt = 0u; attempt < GU_OWNERSHIP_RETRIES; ++attempt) {
        int retry = 0;
        for (probe = 0u; probe < GU_OWNERSHIP_PROBES; ++probe) {
            DWORD index = (start + probe) & GU_OWNERSHIP_MASK;
            const struct GuOwnedRecord *record = &table->records[index];
            DWORD before = (DWORD)InterlockedCompareExchange(
                (volatile LONG *)&record->wrapper_key, 0, 0);
            if (before == wrapper) {
                DWORD token_before = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->token, 0, 0);
                struct GuOwnedRecord snapshot = *record;
                DWORD token_after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->token, 0, 0);
                DWORD after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->wrapper_key, 0, 0);
                if (before == after &&
                    token_before == token_after &&
                    token_before == (DWORD)snapshot.token &&
                    token_before > GU_RESERVED_KEY_MAX &&
                    (DWORD)snapshot.entity_key > GU_RESERVED_KEY_MAX) {
                    if (record_out) {
                        *record_out = snapshot;
                    }
                    if (record_index_out) {
                        *record_index_out = index;
                    }
                    return 1;
                }
                retry = 1;
                break;
            }
            if (before == GU_CLAIMED_KEY ||
                before == GU_RECLAIMING_KEY) {
                retry = 1;
                break;
            }
            if (before == 0u) {
                return 0;
            }
        }
        if (!retry) {
            return 0;
        }
    }
    return 0;
}

int gu_ownership_find_entity(
    const struct GuOwnershipTable *table, DWORD entity,
    struct GuOwnedRecord *record_out, DWORD *record_index_out)
{
    DWORD start;
    DWORD attempt;
    DWORD probe;
    if (!table || entity <= GU_RESERVED_KEY_MAX) {
        return 0;
    }
    start = gu_hash_pointer(entity) & GU_OWNERSHIP_MASK;
    for (attempt = 0u; attempt < GU_OWNERSHIP_RETRIES; ++attempt) {
        int retry = 0;
        for (probe = 0u; probe < GU_OWNERSHIP_PROBES; ++probe) {
            DWORD index = (start + probe) & GU_OWNERSHIP_MASK;
            const struct GuEntityIndex *entry = &table->entities[index];
            DWORD key_before = (DWORD)InterlockedCompareExchange(
                (volatile LONG *)&entry->entity_key, 0, 0);
            if (key_before == entity) {
                DWORD record_index = entry->record_index_plus_one;
                DWORD entry_token_before =
                    (DWORD)InterlockedCompareExchange(
                        (volatile LONG *)&entry->token, 0, 0);
                const struct GuOwnedRecord *record;
                struct GuOwnedRecord snapshot;
                DWORD record_key_before;
                DWORD record_key_after;
                DWORD record_token_before;
                DWORD record_token_after;
                DWORD entry_token_after;
                DWORD key_after;
                if (record_index == 0u ||
                    record_index > GU_OWNERSHIP_CAPACITY) {
                    retry = 1;
                    break;
                }
                --record_index;
                record = &table->records[record_index];
                record_key_before = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->wrapper_key, 0, 0);
                record_token_before = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->token, 0, 0);
                snapshot = *record;
                record_token_after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->token, 0, 0);
                record_key_after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&record->wrapper_key, 0, 0);
                entry_token_after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&entry->token, 0, 0);
                key_after = (DWORD)InterlockedCompareExchange(
                    (volatile LONG *)&entry->entity_key, 0, 0);
                if (key_before == key_after &&
                    entry_token_before == entry_token_after &&
                    entry_token_before > GU_RESERVED_KEY_MAX &&
                    record_key_before == record_key_after &&
                    record_key_before > GU_RESERVED_KEY_MAX &&
                    record_token_before == record_token_after &&
                    record_token_before == entry_token_before &&
                    record_token_before == (DWORD)snapshot.token &&
                    (DWORD)snapshot.wrapper_key == record_key_before &&
                    (DWORD)snapshot.entity_key == entity) {
                    if (record_out) {
                        *record_out = snapshot;
                    }
                    if (record_index_out) {
                        *record_index_out = record_index;
                    }
                    return 1;
                }
                retry = 1;
                break;
            }
            if (key_before == GU_CLAIMED_KEY ||
                key_before == GU_RECLAIMING_KEY) {
                retry = 1;
                break;
            }
            if (key_before == 0u) {
                return 0;
            }
        }
        if (!retry) {
            return 0;
        }
    }
    return 0;
}

int gu_ownership_remove(
    struct GuOwnershipTable *table, DWORD wrapper, DWORD entity,
    DWORD token)
{
    struct GuOwnedRecord record;
    DWORD record_index;
    struct GuOwnedRecord *owned;

    if (!table || !gu_ownership_find_wrapper(
            table, wrapper, &record, &record_index) ||
        (DWORD)record.entity_key != entity || record.token != token) {
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }

    owned = &table->records[record_index];
    if ((DWORD)InterlockedCompareExchange(
            &owned->wrapper_key,
            (LONG)GU_RECLAIMING_KEY,
            (LONG)wrapper) != wrapper) {
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }
    if ((DWORD)InterlockedCompareExchange(
            &owned->token, 0, 0) != token ||
        (DWORD)owned->entity_key != entity) {
        InterlockedExchange(&owned->wrapper_key, (LONG)wrapper);
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }
    if (!gu_reclaim_entity_index(
            table, entity, record_index, token)) {
        InterlockedIncrement(&table->stale_lookups);
    }
    gu_clear_owned_record(owned);
    gu_ownership_decrement_active(table);
    /* Reusable is published last; never expose a tombstone over live data. */
    InterlockedExchange(
        &owned->wrapper_key, (LONG)GU_TOMBSTONE_KEY);
    return 1;
}

#if defined(GTAIV_GRASS_PIPELINE_TESTING)
volatile LONG g_gu_ownership_test_pause_take;
volatile LONG g_gu_ownership_test_take_reached;

static void gu_ownership_test_pause_after_take(void)
{
    if (InterlockedCompareExchange(
            &g_gu_ownership_test_pause_take, 0, 0) == 0) {
        return;
    }
    InterlockedExchange(&g_gu_ownership_test_take_reached, 1);
    while (InterlockedCompareExchange(
            &g_gu_ownership_test_pause_take, 0, 0) != 0) {
        SwitchToThread();
    }
}
#endif

int gu_ownership_take_wrapper(
    struct GuOwnershipTable *table, DWORD wrapper,
    struct GuOwnedRecord *record_out)
{
    struct GuOwnedRecord record;
    DWORD record_index;
    DWORD entity;
    struct GuOwnedRecord *owned;

    if (!table || !record_out ||
        !gu_ownership_find_wrapper(
            table, wrapper, &record, &record_index)) {
        if (table) {
            InterlockedIncrement(&table->stale_lookups);
        }
        return 0;
    }
    owned = &table->records[record_index];
    if ((DWORD)InterlockedCompareExchange(
            &owned->wrapper_key,
            (LONG)GU_RECLAIMING_KEY,
            (LONG)wrapper) != wrapper) {
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }
    if ((DWORD)InterlockedCompareExchange(
            &owned->token, 0, 0) != (DWORD)record.token) {
        InterlockedExchange(&owned->wrapper_key, (LONG)wrapper);
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }
    record = *owned;
    record.wrapper_key = (LONG)wrapper;
    entity = (DWORD)record.entity_key;
    if (entity <= GU_RESERVED_KEY_MAX) {
        InterlockedExchange(&owned->wrapper_key, (LONG)wrapper);
        InterlockedIncrement(&table->stale_lookups);
        return 0;
    }
#if defined(GTAIV_GRASS_PIPELINE_TESTING)
    gu_ownership_test_pause_after_take();
#endif
    if (!gu_reclaim_entity_index(
            table, entity, record_index,
            (DWORD)record.token)) {
        InterlockedIncrement(&table->stale_lookups);
    }
    gu_clear_owned_record(owned);
    gu_ownership_decrement_active(table);
    InterlockedExchange(
        &owned->wrapper_key, (LONG)GU_TOMBSTONE_KEY);
    *record_out = record;
    return 1;
}

int gu_make_distance_plan(
    float original_near, float original_far, float original_query,
    float multiplier, struct GuDistancePlan *plan)
{
    float fade_band;
    float preload_lead;
    float scaled_far;
    float scaled_near;
    float scaled_query;

    if (!plan || !gu_finite_nonnegative(original_near) ||
        !gu_finite_positive(original_far) ||
        !gu_finite_positive(original_query) ||
        !gu_finite_positive(multiplier) ||
        multiplier < GU_DISTANCE_MULTIPLIER_MIN ||
        multiplier > GU_DISTANCE_MULTIPLIER_MAX ||
        original_near >= original_far ||
        original_query < original_far) {
        return 0;
    }

    fade_band = original_far - original_near;
    preload_lead = original_query - original_far;
    scaled_far = original_far * multiplier;
    scaled_near = scaled_far - fade_band;
    if (scaled_near < 0.0f) {
        scaled_near = 0.0f;
    }
    scaled_query = scaled_far + preload_lead;
    if (!gu_finite_nonnegative(scaled_near) ||
        !gu_finite_positive(scaled_far) ||
        !gu_finite_positive(scaled_query) ||
        scaled_far > 10000.0f || scaled_query > 10000.0f) {
        return 0;
    }

    plan->original_near = original_near;
    plan->original_far = original_far;
    plan->original_query = original_query;
    plan->fade_band = fade_band;
    plan->preload_lead = preload_lead;
    plan->scaled_near = scaled_near;
    plan->scaled_far = scaled_far;
    plan->scaled_query = scaled_query;
    plan->scaled_far_squared = scaled_far * scaled_far;
    plan->source_query_radius = 30.0f * multiplier;
    return gu_finite_positive(plan->scaled_far_squared) &&
           gu_finite_positive(plan->source_query_radius);
}

int gu_make_density_plan(
    float spacing, float inverse_square, DWORD use_grid,
    DWORD selected, float multiplier, struct GuDensityPlan *plan)
{
    float effective = selected ? multiplier : 1.0f;
    float root;
    float scaled_spacing;
    float scaled_inverse;

    if (!plan || !gu_finite_positive(spacing) ||
        !gu_finite_positive(inverse_square) ||
        !gu_finite_positive(multiplier) ||
        multiplier < GU_DENSITY_MULTIPLIER_MIN ||
        multiplier > GU_DENSITY_MULTIPLIER_MAX) {
        return 0;
    }
    root = gu_sqrt(effective);
    if (!gu_finite_positive(root)) {
        return 0;
    }
    scaled_spacing = use_grid ? spacing / root : spacing;
    scaled_inverse = inverse_square * effective;
    if (!gu_finite_positive(scaled_spacing) ||
        !gu_finite_positive(scaled_inverse) ||
        scaled_inverse > 1000000.0f) {
        return 0;
    }
    plan->original_spacing = spacing;
    plan->original_inverse_square = inverse_square;
    plan->scaled_spacing = scaled_spacing;
    plan->scaled_inverse_square = scaled_inverse;
    plan->effective_multiplier = effective;
    plan->selected = selected != 0u;
    plan->use_grid = use_grid != 0u;
    return 1;
}

float gu_scale_instance_distance(float original, float multiplier)
{
    float scaled;
    if (!gu_finite_positive(original) ||
        !gu_finite_positive(multiplier) ||
        multiplier < GU_DISTANCE_MULTIPLIER_MIN ||
        multiplier > GU_DISTANCE_MULTIPLIER_MAX) {
        return 0.0f;
    }
    scaled = original * multiplier;
    if (!gu_finite_positive(scaled) || scaled > 10000.0f) {
        return 0.0f;
    }
    return scaled;
}

DWORD gu_definition_class_bit(DWORD definition_class)
{
    switch (definition_class) {
    case GU_CLASS_GRASS:
        return GU_CLASS_GRASS_BIT;
    case GU_CLASS_VEGETATION:
        return GU_CLASS_VEGETATION_BIT;
    case GU_CLASS_CLUTTER:
        return GU_CLASS_CLUTTER_BIT;
    case GU_CLASS_OTHER:
        return GU_CLASS_OTHER_BIT;
    default:
        return 0u;
    }
}
