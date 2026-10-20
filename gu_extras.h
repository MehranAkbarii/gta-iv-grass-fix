/*
 * gu_extras.h - optional, opt-in additions for the procedural fix.
 *
 *   1. Generator matrix limit (+ matrix pool): lets more props be scaled and
 *      slope-aligned than the game's 512.
 *   2. Props out of the water reflection passes.
 *   3. ConeBack: props behind the camera are parked instead of made, and are
 *      made again, nearest first, before the camera turns to them.
 *
 * Everything is OFF unless its INI key is set, so the shipped behaviour does
 * not change. Each feature installs on its own and is all-or-nothing: every
 * pattern must match exactly once and every byte it touches is verified; on
 * any doubt nothing of that feature is written.
 *
 * The code locates its sites with byte patterns (wildcards for relocated
 * operands). It is meant to run only after the main fix has verified the exact
 * executable. Freestanding C: no C runtime calls.
 *
 * Hook safety: the cone stubs save and restore xmm0-xmm7 (the hook code is
 * built with SSE2); the reflection stub does not, its hook uses only general
 * registers. Hook bodies run under SEH (MSVC): an access violation switches
 * the feature off for the session and raises a counter instead of crashing.
 */
#ifndef GU_EXTRAS_H
#define GU_EXTRAS_H

#include <stddef.h>
#include <stdint.h>

#define GU_X_SITE_COUNT 16u

enum GuExtrasStatus {
    GU_X_OFF = 0,            /* not requested */
    GU_X_ON = 1,             /* installed */
    GU_X_ERR_SITE = 2,       /* a pattern did not match exactly once / bytes differ */
    GU_X_ERR_BUSY = 3,       /* a stopped thread is inside a window to patch */
    GU_X_ERR_MEMORY = 4,     /* allocation or protection failed */
    GU_X_ERR_WRITE = 5,      /* write or read-back verification failed (reverted) */
    GU_X_ERR_CAMERA = 6,     /* camera globals not found or implausible */
    GU_X_ERR_CONFIG = 7,     /* needs another piece that is missing */
    GU_X_NOT_NEEDED = 8      /* the game (or another mod) already has at least that much */
};

struct GuExtrasConfig {
    uint32_t matrix_limit;            /* 0 = vanilla 512, else 513..4096 */
    uint32_t matrix_pool;             /* 0 = automatic, else 7000..65536 */
    uint32_t reflections_props;       /* 1 = vanilla (drawn), 0 = skipped */
    uint32_t cone_back_meters;        /* 0 = off, else 20..1000 */
    uint32_t cone_full_angle_deg;     /* 30..120: full radius inside this half angle */
    uint32_t cone_release_per_update; /* 0 = unlimited, else parked props made per update */
    uint32_t cone_lead_ms;            /* turn prediction, 0..1000 */
    uint32_t drawable_refs;           /* 0 = off, else 13000..200000 (the game has 13000) */
    uint32_t render_cache;            /* 0 = off, else 9692..1048576 entities a frame can draw */
    uint32_t drawable_own_memory;     /* 1 = the new array comes from our memory, 0 = from the game's allocator */
    uint32_t spawns_per_update;       /* 0 = off, else 16..8192 props made per generator update */
    uint32_t spawn_near_always;       /* metres: props this close are always made at once (0..500) */
    /* provided by the host */
    int (*is_prop_entity)(uint32_t entity); /* NULL = flags test only */
};

struct GuExtrasReport {
    uint32_t matrix_status, reflections_status, cone_status;
    uint32_t drawable_status, render_cache_status, spawn_status;
    uint32_t drawable_refs_applied, render_cache_applied;
    volatile uint32_t drawable_allocs, drawable_alloc_fallbacks;
    uint32_t matrix_limit_applied, matrix_pool_applied;
    uint32_t matrix_limit_sites_original;   /* 1 = the two 0x200 immediates were still original */
    /* SEH guard: a memory fault inside a hook switches that feature off */
    volatile uint32_t reflections_faults, reflections_disabled;
    volatile uint32_t cone_faults, cone_disabled;
    volatile uint32_t spawn_faults, spawn_disabled;
    volatile uint32_t spawn_limited_updates, spawn_held_total, spawn_hold_max, spawn_dropped_removed;
    uint32_t site_matches[GU_X_SITE_COUNT];   /* matches found (0, 1, 2 = more than one) */
    uint32_t site_rva[GU_X_SITE_COUNT];       /* RVA of the first match */
    const char *site_name[GU_X_SITE_COUNT];
    uint32_t sites_known;
    /* counters (statistics only) */
    volatile uint32_t reflections_skipped;
    volatile uint32_t cone_updates, cone_parked_now, cone_parked_max;
    volatile uint32_t cone_made_directly, cone_parked_total, cone_released_total;
    volatile uint32_t cone_dropped_removed, cone_overflow_made, cone_burst_updates;
    volatile uint32_t cone_invalid_camera;
    volatile uint32_t cone_resets, cone_dropped_on_reset;
    volatile uint32_t cone_budget_limited, cone_room_limited;
    volatile uint32_t cone_max_released, cone_queue_max;
    volatile uint32_t reflections_candidates, reflections_flagged;
    volatile uint32_t matrix_pool_allocs, matrix_pool_alloc_fallbacks;
    volatile uint32_t matrix_pool_frees_ours, matrix_pool_frees_forwarded;
};

extern struct GuExtrasReport g_gu_extras_report;

/* Installs what is requested. Never fails the caller: the outcome is in the
 * report. addr_busy(start, length, ctx) returns non-zero if a stopped thread
 * executes inside [start, start+length). */
int gu_extras_install(
    const struct GuExtrasConfig *config,
    const uint8_t *image_base, uint32_t image_size,
    uint8_t *manager,
    int (*addr_busy)(uint32_t start, uint32_t length, void *ctx), void *ctx);

/* Same, over an explicit executable range (used by the tests). */
int gu_extras_install_range(
    const struct GuExtrasConfig *config,
    const uint8_t *text, uint32_t text_size,
    const uint8_t *image_base, uint32_t image_size,
    uint8_t *manager,
    int (*addr_busy)(uint32_t start, uint32_t length, void *ctx), void *ctx);

#endif
