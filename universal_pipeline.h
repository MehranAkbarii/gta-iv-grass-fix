#ifndef GTAIV_GRASS_UNIVERSAL_PIPELINE_H
#define GTAIV_GRASS_UNIVERSAL_PIPELINE_H

#include <windows.h>

/*
 * Corrected procedural behaviour shared by every exact-hash executable
 * profile.  This layer owns only mod memory and per-instance values.  It must
 * never mutate the global CBaseModelInfo draw-distance table because placed
 * objects may share the same model as a generated object.
 */

#define GU_DISTANCE_MULTIPLIER_MIN 0.5f
#define GU_DISTANCE_MULTIPLIER_MAX 4.0f
#define GU_DENSITY_MULTIPLIER_MIN 0.1f
#define GU_DENSITY_MULTIPLIER_MAX 2.0f

#define GU_SOURCE_BUFFER_CAPACITY 8192u
#define GU_EVENT_RING_CAPACITY 8192u
#define GU_EVENT_RING_MASK (GU_EVENT_RING_CAPACITY - 1u)
#define GU_EVENT_TYPE_COUNT 32u
#define GU_EVENT_CAS_RETRIES 8u
/* Open-addressing hash with a 32-probe window. Must stay a power of two and
 * keep the load at or below 25% at the rendered-object ceiling (checked at
 * compile time in GTAIVProceduralPool.c). 65536 only added cache footprint. */
#define GU_OWNERSHIP_CAPACITY 32768u
#define GU_OWNERSHIP_MASK (GU_OWNERSHIP_CAPACITY - 1u)
#define GU_OWNERSHIP_PROBES 32u
#define GU_OWNERSHIP_RETRIES 64u
#define GU_TOMBSTONE_KEY 1u
#define GU_CLAIMED_KEY 2u
#define GU_RECLAIMING_KEY 3u
#define GU_RESERVED_KEY_MAX GU_RECLAIMING_KEY

#define GU_CLASS_GRASS_BIT 0x01u
#define GU_CLASS_VEGETATION_BIT 0x02u
#define GU_CLASS_CLUTTER_BIT 0x04u
#define GU_CLASS_OTHER_BIT 0x08u
#define GU_CLASS_DEFAULT_MASK \
    (GU_CLASS_GRASS_BIT | GU_CLASS_VEGETATION_BIT)

enum GuEventType {
    GU_EVENT_NONE = 0,
    GU_EVENT_DEFINITION_COMMIT = 1,
    GU_EVENT_DEFINITION_CLASSIFIED = 2,
    GU_EVENT_DENSITY_APPLIED = 3,
    GU_EVENT_PROVIDER_ELIGIBILITY = 4,
    GU_EVENT_SURFACE_CONSTRUCT = 5,
    GU_EVENT_CANDIDATE_ATTEMPT = 6,
    GU_EVENT_CANDIDATE_REJECT = 7,
    GU_EVENT_CANDIDATE_ACCEPT = 8,
    GU_EVENT_SOURCE_QUERY_BEGIN = 9,
    GU_EVENT_SOURCE_QUERY_END = 10,
    GU_EVENT_WRAPPER_ALLOCATE = 11,
    GU_EVENT_ENTITY_CREATE = 12,
    GU_EVENT_ENTITY_RELEASE = 13,
    GU_EVENT_RENDER_DISTANCE = 14,
    GU_EVENT_RENDER_CULL = 15,
    GU_EVENT_FADE_ALPHA = 16,
    GU_EVENT_ACCOUNTING = 17,
    GU_EVENT_GOVERNOR = 18,
    GU_EVENT_HOOK_REFUSAL = 19
};

enum GuDefinitionClass {
    GU_CLASS_UNKNOWN = 0,
    GU_CLASS_GRASS = 1,
    GU_CLASS_VEGETATION = 2,
    GU_CLASS_CLUTTER = 3,
    GU_CLASS_OTHER = 4
};

enum GuOwnershipFlags {
    GU_OWNER_SURFACE = 0x01u,
    GU_OWNER_SOURCE = 0x02u,
    GU_OWNER_DISTANCE_APPLIED = 0x04u
};

struct GuDistancePlan {
    float original_near;
    float original_far;
    float original_query;
    float fade_band;
    float preload_lead;
    float scaled_near;
    float scaled_far;
    float scaled_query;
    float scaled_far_squared;
    float source_query_radius;
};

struct GuDensityPlan {
    float original_spacing;
    float original_inverse_square;
    float scaled_spacing;
    float scaled_inverse_square;
    float effective_multiplier;
    DWORD selected;
    DWORD use_grid;
};

/* Exactly 64 bytes so hot producers perform one bounded fixed-size copy. */
struct GuRawEvent {
    DWORD sequence;
    DWORD type;
    DWORD identity_id;
    DWORD frame_or_epoch;
    DWORD values[12];
};

struct GuEventSlot {
    volatile LONG sequence;
    struct GuRawEvent event;
};

struct GuEventRing {
    volatile LONG enqueue_position;
    volatile LONG dequeue_position;
    volatile LONG lost_full[GU_EVENT_TYPE_COUNT];
    volatile LONG lost_contention[GU_EVENT_TYPE_COUNT];
    volatile LONG lost_disabled[GU_EVENT_TYPE_COUNT];
    volatile LONG enabled;
    struct GuEventSlot slots[GU_EVENT_RING_CAPACITY];
};

struct GuEventDrainSummary {
    volatile LONG consumed_total;
    volatile LONG consumed_by_type[GU_EVENT_TYPE_COUNT];
    struct GuRawEvent last_by_type[GU_EVENT_TYPE_COUNT];
};

struct GuOwnedRecord {
    volatile LONG wrapper_key;
    volatile LONG entity_key;
    DWORD surface_owner;
    DWORD source_owner;
    DWORD original_distance_bits;
    DWORD scaled_distance_bits;
    volatile LONG token;
    DWORD flags;
};

struct GuEntityIndex {
    volatile LONG entity_key;
    DWORD record_index_plus_one;
    volatile LONG token;
};

struct GuOwnershipTable {
    volatile LONG next_token;
    volatile LONG token_exhausted;
    volatile LONG token_exhaustions;
    volatile LONG active_count;
    volatile LONG high_watermark;
    volatile LONG insert_failures;
    volatile LONG entity_index_failures;
    volatile LONG stale_lookups;
    struct GuOwnedRecord records[GU_OWNERSHIP_CAPACITY];
    struct GuEntityIndex entities[GU_OWNERSHIP_CAPACITY];
};

extern struct GuEventRing g_gu_event_ring;
extern struct GuEventDrainSummary g_gu_event_summary;
extern struct GuOwnershipTable g_gu_ownership;
extern DWORD g_gu_source_buffer[GU_SOURCE_BUFFER_CAPACITY];
extern float g_gu_source_radius;

void gu_event_ring_initialize(struct GuEventRing *ring);
int gu_event_try_emit(
    struct GuEventRing *ring, const struct GuRawEvent *event);
int gu_event_try_consume(
    struct GuEventRing *ring, struct GuRawEvent *event);
void gu_event_summary_initialize(
    struct GuEventDrainSummary *summary);
DWORD gu_event_drain(
    struct GuEventRing *ring,
    struct GuEventDrainSummary *summary,
    DWORD maximum_events);

void gu_ownership_initialize(struct GuOwnershipTable *table);
int gu_ownership_insert(
    struct GuOwnershipTable *table,
    DWORD wrapper, DWORD entity, DWORD surface, DWORD source,
    DWORD original_distance_bits, DWORD scaled_distance_bits,
    DWORD flags, DWORD *token_out);
int gu_ownership_find_wrapper(
    const struct GuOwnershipTable *table, DWORD wrapper,
    struct GuOwnedRecord *record_out, DWORD *record_index_out);
int gu_ownership_find_entity(
    const struct GuOwnershipTable *table, DWORD entity,
    struct GuOwnedRecord *record_out, DWORD *record_index_out);
int gu_ownership_remove(
    struct GuOwnershipTable *table, DWORD wrapper, DWORD entity,
    DWORD token);
int gu_ownership_take_wrapper(
    struct GuOwnershipTable *table, DWORD wrapper,
    struct GuOwnedRecord *record_out);

#if defined(GTAIV_GRASS_PIPELINE_TESTING)
extern volatile LONG g_gu_ownership_test_pause_take;
extern volatile LONG g_gu_ownership_test_take_reached;
#endif

int gu_make_distance_plan(
    float original_near, float original_far, float original_query,
    float multiplier, struct GuDistancePlan *plan);
int gu_make_density_plan(
    float spacing, float inverse_square, DWORD use_grid,
    DWORD selected, float multiplier, struct GuDensityPlan *plan);
float gu_scale_instance_distance(float original, float multiplier);
DWORD gu_definition_class_bit(DWORD definition_class);

#endif
