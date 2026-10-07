#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "density_catalog.generated.h"
#include "universal_pipeline.h"
#include "universal_behavior.h"
#include "universal_install.h"
#include "universal_profiles.h"
#include "grass_compat.generated.h"

/* TinyCC's bundled Win32 headers omit the native thread-enumeration
 * declarations used by the startup transaction.  Resolve both ntdll entry
 * points before closing the thread gate; the stopped-world phase itself
 * performs only fixed-storage system calls and direct memory operations. */
#define GU_STARTUP_MAX_THREADS 256u
#define GU_STARTUP_MAX_PATCH_SPANS 128u
#define GU_STARTUP_EXACT_GUARD_INTERVALS 5u
#define GU_STARTUP_GUARD_HALO_LENGTH 14u
#define GU_STARTUP_MAX_EXACT_RETURN_RVAS 16384u
#define GU_STARTUP_STABLE_SNAPSHOTS 2u
#define GU_STARTUP_MAX_SNAPSHOT_PASSES 16u
#define GU_STARTUP_RESUME_ATTEMPTS 4u
#define GU_STARTUP_LOADER_WAIT_MS 30000u

typedef BYTE (WINAPI *GuRtlIsThreadWithinLoaderCalloutFn)(void);
#define GU_STATUS_NO_MORE_ENTRIES 0x8000001Au

typedef LONG (WINAPI *GuNtQueryInformationThreadFn)(
    HANDLE, DWORD, void *, DWORD, DWORD *);
typedef LONG (WINAPI *GuNtGetNextThreadFn)(
    HANDLE, HANDLE, DWORD, DWORD, DWORD, HANDLE *);

struct GuThreadBasicInformation32 {
    LONG exit_status;
    BYTE *teb_base;
    DWORD client_process;
    DWORD client_thread;
    DWORD affinity_mask;
    LONG priority;
    LONG base_priority;
};

struct GuStartupPatchSpan {
    BYTE *address;
    DWORD length;
    const BYTE *expected;
};

enum GuStartupGuardRole {
    GU_STARTUP_GUARD_PROCEDURAL = 1,
    GU_STARTUP_GUARD_GENERATOR = 2,
    GU_STARTUP_GUARD_SOURCE = 3,
    GU_STARTUP_GUARD_DEFINITION = 4,
    GU_STARTUP_GUARD_PLANT_SETTER = 5
};

/*
 * Generated independently from each registered executable SHA-256.  Each
 * semantic EIP guard is surrounded by a fourteen-byte validation halo, which
 * is enough to contain every legal x86 instruction that overlaps either
 * boundary.  The complete preferred-image byte authority is revalidated only
 * after all discoverable peer threads have been suspended.  HIGHLOW offsets
 * identify DWORD operands that the Windows loader adjusts when the executable
 * is not mapped at its preferred base.
 */
struct GuStartupGuardInterval {
    DWORD role;
    DWORD guard_first_rva;
    DWORD guard_length;
    DWORD validation_first_rva;
    DWORD validation_length;
    const BYTE *preferred_bytes;
    DWORD relocation_count;
    const DWORD *relocation_offsets;
};

struct GuStartupExactCallContract {
    DWORD executable_identity_id;
    DWORD build_profile_id;
    DWORD preferred_image_base;
    DWORD interval_count;
    const struct GuStartupGuardInterval *intervals;
    DWORD return_count;
    const DWORD *return_rvas;
    const char *target_id;
    const char *specimen_sha256;
    const char *guard_interval_contract_sha256;
    const char *return_rva_catalog_sha256;
    const char *full_startup_authority_sha256;
};

struct GuStartupQuiescence {
    HANDLE handles[GU_STARTUP_MAX_THREADS];
    DWORD thread_ids[GU_STARTUP_MAX_THREADS];
    BYTE live[GU_STARTUP_MAX_THREADS];
    DWORD count;
    DWORD active;
};

#define PLUGIN_NAME "GTAIV.EFLC.ProceduralFixes"
#define PLUGIN_VERSION "v1.3-Conservative-SAFE-GRASS-BLD-0051"
#define DEFAULT_CAPACITY 16384u
#define VANILLA_CAPACITY 512u
#define MIN_CAPACITY 512u
#define MAX_OPERATIONAL_CAPACITY 16384u
#define DEFAULT_RENDERED_OBJECT_CAPACITY 8192u
#define VANILLA_RENDERED_OBJECT_CAPACITY 512u
#define MIN_RENDERED_OBJECT_CAPACITY 512u
#define MAX_RENDERED_OBJECT_CAPACITY 8192u
#define DEFAULT_PROVIDER_CAPACITY 40u
#define VANILLA_PROVIDER_CAPACITY 40u
#define MIN_PROVIDER_CAPACITY 40u
#define MAX_PROVIDER_CAPACITY 40u
#define DEFAULT_DISTANCE_MULTIPLIER 1u
#define MIN_DISTANCE_MULTIPLIER 1u
#define MAX_DISTANCE_MULTIPLIER 50u
#define DEFAULT_CORRECTED_DISTANCE_MULTIPLIER 1.0f
#define MIN_CORRECTED_DISTANCE_MULTIPLIER 0.5f
#define MAX_CORRECTED_DISTANCE_MULTIPLIER 1.5f
/*
 * Retained VCNE telemetry exhausted the former 4096-record automatic
 * baseline during a neutral one-hour session (4008 high-watermark plus
 * bounded fallback allocations).  The fixed conservative ceiling costs bounded
 * private storage and removes capacity guessing from public defaults.
 */
#define AUTO_RENDER_PROFILE_1 8192u
#define AUTO_RENDER_PROFILE_4 8192u
#define LEGACY_DISTANCE_OPERAND_PATCH_ENABLED 0u
#define LEGACY_MODEL_GLOBAL_DRAW_DISTANCE_MUTATION_ENABLED 0u
#define GENERATED_PROCOBJ_DISTANCE_LIFECYCLE_ENABLED 1u
#define TUNING_GOVERNOR_OBSERVATION_ONLY 1u
#define DEFAULT_PLANT_DENSITY_MULTIPLIER 1u
#define MIN_PLANT_DENSITY_MULTIPLIER 1u
#define MAX_PLANT_DENSITY_MULTIPLIER 50u
#define DEFAULT_CORRECTED_PLANT_DENSITY_MULTIPLIER 1.0f
#define MIN_CORRECTED_PLANT_DENSITY_MULTIPLIER 0.1f
#define MAX_CORRECTED_PLANT_DENSITY_MULTIPLIER 2.0f
#define DEFAULT_PROCOBJ_DENSITY_MULTIPLIER 1.0f
#define MIN_PROCOBJ_DENSITY_MULTIPLIER 0.1f
#define MAX_PROCOBJ_DENSITY_MULTIPLIER 2.0f
/* Public v1.2 scale preserves the validated predecessor effective range.
 * Configured 0.2..4.0 maps once to effective 0.1..2.0; default 1 -> 0.5. */
#define PUBLIC_DENSITY_BASELINE_SCALE 0.5f
#define MIN_PUBLIC_DENSITY_MULTIPLIER 0.2f
#define MAX_PUBLIC_DENSITY_MULTIPLIER 4.0f
#define DEFAULT_DENSITY_CLASS_MASK 3u
#define MIN_DENSITY_CLASS_MASK 0u
#define MAX_DENSITY_CLASS_MASK 15u
#define DEFAULT_DEBUG_LOG 1u
#define MIN_DEBUG_LOG 0u
#define MAX_DEBUG_LOG 1u
#define DEFAULT_HANG_WATCHDOG 1u
#define MIN_HANG_WATCHDOG 0u
#define MAX_HANG_WATCHDOG 1u
#define DEFAULT_HANG_TIMEOUT_SECONDS 30u
#define MIN_HANG_TIMEOUT_SECONDS 20u
#define MAX_HANG_TIMEOUT_SECONDS 120u
#define DEFAULT_HANG_MESSAGE_BOX 0u
#define MIN_HANG_MESSAGE_BOX 0u
#define MAX_HANG_MESSAGE_BOX 1u
#define DEFAULT_FULL_HANG_DUMP 0u
#define MIN_FULL_HANG_DUMP 0u
#define MAX_FULL_HANG_DUMP 1u

#define VERIFIED_PE_TIMESTAMP (g_build_profile->pe_timestamp)
#define VERIFIED_IMAGE_SIZE (g_build_profile->image_size)
#define VERIFIED_PREFERRED_IMAGE_BASE 0x00400000u
#define VERIFIED_TARGET_RVA (g_build_profile->target_rva)
#define VERIFIED_MANAGER_RVA (g_build_profile->manager_rva)
#define VERIFIED_DEFINITION_MANAGER_RVA \
    (g_build_profile->definition_manager_rva)
#define VERIFIED_MODEL_INFO_TABLE_RVA \
    (g_build_profile->model_info_table_rva)
#define VERIFIED_GENERATOR_MANAGER_RVA \
    (g_build_profile->generator_manager_rva)
#define VERIFIED_GENERATOR_POP_RVA (g_build_profile->generator_pop_rva)
#define VERIFIED_GENERATOR_ALLOCATOR_RVA \
    (g_build_profile->generator_allocator_rva)
#define VERIFIED_GENERATOR_SPECIAL_CAP_RVA \
    (g_build_profile->generator_special_cap_rva)
#define VERIFIED_GENERATOR_RENDER_CAP_RVA \
    (g_build_profile->generator_render_cap_rva)
#define VERIFIED_GENERATOR_STAGING_ALLOC_RVA \
    (g_build_profile->generator_staging_alloc_rva)
#define VERIFIED_GENERATOR_STAGING_CAP_RVA \
    (g_build_profile->generator_staging_cap_rva)
#define VERIFIED_DEFINITION_CALL_RVA \
    (g_build_profile->definition_call_rva)
#define VERIFIED_DEFINITION_CALL_LENGTH 15u

#define TARGET_PATTERN_LENGTH (g_build_profile->target_pattern_length)
#define TARGET_IMMEDIATE_OFFSET (g_build_profile->target_immediate_offset)
#define TARGET_WRAPPER_DISTANCE (g_build_profile->target_wrapper_distance)
#define DISTANCE_INIT_PATTERN_LENGTH \
    (g_build_profile->distance_init_pattern_length)
#define DISTANCE_UPDATE_PATTERN_LENGTH \
    (g_build_profile->distance_update_pattern_length)

#define DIST_INIT_BASE_POINTER_OFFSET \
    (g_build_profile->distance_init_offsets[0])
#define DIST_INIT_DETAIL_POINTER_OFFSET \
    (g_build_profile->distance_init_offsets[1])
#define DIST_INIT_NEAR_POINTER_OFFSET \
    (g_build_profile->distance_init_offsets[2])
#define DIST_INIT_FAR_POINTER_OFFSET \
    (g_build_profile->distance_init_offsets[3])

#define DIST_UPDATE_BASE_POINTER_OFFSET 4u
#define DIST_UPDATE_DETAIL_POINTER_OFFSET 12u
#define DIST_UPDATE_NEAR_POINTER_OFFSET 20u
#define DIST_UPDATE_FAR_POINTER_OFFSET 33u

#define PROVIDER_PATCH_CAPACITY 12u
#define PROVIDER_PATCH_MAX_LENGTH 24u
#define PATCH_PROTECTION_RESTORE_ATTEMPTS 3u
#define PROVIDER_RELOCATION_PATCH_COUNT \
    (g_build_profile->provider_patch_count)
#define PROVIDER_PATCH_COUNT \
    (PROVIDER_RELOCATION_PATCH_COUNT + 2u)
#define PROVIDER_PATCH_FIRST_RVA (g_build_profile->provider_sites[0])
#define PROVIDER_PATCH_LAST_RVA \
    (g_build_profile->provider_sites[PROVIDER_RELOCATION_PATCH_COUNT - 1u])
#define PROVIDER_MANAGER_POINTER_RVA \
    (g_build_profile->provider_manager_rva)
#define PROVIDER_NEXT_INDEX_POINTER_RVA \
    (g_build_profile->provider_next_index_rva)
#define PROVIDER_RECORD_INDEX_POINTER_RVA \
    (g_build_profile->provider_record_index_rva)
#define PROVIDER_HEAD_QUARANTINE_RVA \
    (g_build_profile->provider_head_quarantine_rva)
#define PROVIDER_REBUILD_EPILOGUE_RVA \
    (g_build_profile->provider_rebuild_epilogue_rva)
#define PROCEDURAL_CODE_FIRST_RVA \
    (g_build_profile->procedural_code_first_rva)
#define PROCEDURAL_CODE_LAST_RVA \
    (g_build_profile->procedural_code_last_rva)
#define GENERATOR_CODE_FIRST_RVA \
    (g_build_profile->generator_code_first_rva)
#define GENERATOR_CODE_LAST_RVA \
    (g_build_profile->generator_code_last_rva)
#define PROVIDER_EXCEPTION_ADDRESS_MARGIN 0x1000u

#define GENERATOR_PATCH_COUNT 5u
#define GENERATOR_PATCH_MAX_LENGTH 16u
#define GENERATOR_RECORD_SIZE 0x18u
#define GENERATOR_FREE_LIST_OFFSET 0x3008u
#define GENERATOR_FREE_COUNT_OFFSET 0x3008u
#define GENERATOR_FREE_HEAD_OFFSET 0x300Cu
#define GENERATOR_ACTIVE_RENDERED_COUNT_OFFSET 0x0000u
#define GENERATOR_SECONDARY_ENTITY_COUNT_OFFSET 0x0004u
#define GENERATOR_STAGING_CAPACITY_OFFSET 0x5220u
#define GENERATOR_STAGING_BUFFER_OFFSET 0x5224u
#define GENERATOR_STAGING_COUNT_OFFSET 0x5228u
#define GENERATOR_STAGING_ALLOCATED_COUNT_OFFSET 0x522Au
#define GENERATOR_STAGING_RECORD_SIZE 0x20u
#define GENERATOR_STAGING_VANILLA_BYTES 0x4000u
#define GENERATOR_CAPACITY_WORD_MAX 0xFFFFu
#define GENERATOR_MANAGER_MINIMUM_SIZE 0x5234u

#define FLOAT_BITS_HALF 0x3F000000u
#define FLOAT_BITS_20 0x41A00000u
#define FLOAT_BITS_40 0x42200000u

#define MANAGER_SOURCE_FREE_HEAD_OFFSET 0x00u
#define MANAGER_SOURCE_ACTIVE_HEAD_OFFSET 0x02u
#define MANAGER_SURFACE_FREE_HEAD_OFFSET 0x04u
#define MANAGER_SURFACE_LIST0_OFFSET 0x06u
#define MANAGER_SURFACE_LIST_COUNT 4u
#define MANAGER_FADE_CLOSE_OFFSET 0x10u
#define MANAGER_PLANT_FAR_OFFSET 0x14u
#define MANAGER_CAPACITY_OFFSET 0x18u
#define MANAGER_QUERY_RADIUS_OFFSET 0x1Cu
#define MANAGER_PLANT_FAR_SQUARED_OFFSET 0x20u
#define MANAGER_SOURCE_RECORDS_OFFSET 0x30u
#define MANAGER_SOURCE_RECORD_COUNT VANILLA_PROVIDER_CAPACITY
#define MANAGER_BUFFER0_OFFSET 0xF30u
#define MANAGER_POSITION_OFFSET 0xF40u
#define MANAGER_STAGING_COUNTS_OFFSET 0xF50u
#define MANAGER_BUFFER1_OFFSET 0xF60u
#define MANAGER_BUFFER2_OFFSET 0xF64u
#define MANAGER_STAGING_INDEX_OFFSET 0xF68u
#define MANAGER_MINIMUM_SIZE 0xF78u

#define PROCEDURAL_RECORD_SIZE 0x60u
#define SURFACE_FLAGS_OFFSET 0x55u
#define SURFACE_NEXT_OFFSET 0x56u
#define SOURCE_NEXT_OFFSET 0x58u
#define SOURCE_RECORD_ACTIVE_NEXT_OFFSET 0x58u
#define TELEMETRY_INTERVAL_MS 5000u
#define TELEMETRY_EXHAUSTION_POLL_MS 100u
#define TELEMETRY_FULL_SAMPLE_INTERVAL 12u
#define HANG_WATCHDOG_POLL_MS 5000u
#define HANG_WATCHDOG_PROBE_TIMEOUT_MS 1000u
#define HANG_WATCHDOG_STARTUP_GRACE_MS 60000u

#define DEFINITION_PROCOBJ_COUNT_OFFSET 0x0004u
#define DEFINITION_PROCOBJ_RECORDS_OFFSET 0x0008u
#define DEFINITION_PROCOBJ_RECORD_SIZE 0x44u
#define DEFINITION_PROCOBJ_MODEL_INDEX_OFFSET 0x04u
#define DEFINITION_PROCOBJ_SPACING_OFFSET 0x08u
#define DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET 0x0Cu
#define DEFINITION_PROCOBJ_DISTANCE_GATE_SQUARED_OFFSET 0x10u
#define DEFINITION_PROCOBJ_FIELD14_OFFSET 0x14u
#define DEFINITION_PROCOBJ_FIELD18_OFFSET 0x18u
#define DEFINITION_PROCOBJ_ALIGN_OFFSET 0x34u
#define DEFINITION_PROCOBJ_USEGRID_OFFSET 0x35u
#define DEFINITION_PROCOBJ_USESEED_OFFSET 0x36u
#define DEFINITION_PROCOBJ_FLOATS_OFFSET 0x37u
#define DEFINITION_PROCOBJ_CAPACITY 256u
#define DEFINITION_PLANT_COUNT_OFFSET 0x4408u
#define DEFINITION_PLANT_RECORDS_OFFSET 0x440Cu
#define DEFINITION_PLANT_RECORD_SIZE 0x30u
#define DEFINITION_PLANT_DENSITY_OFFSET 0x2Cu
#define DEFINITION_PLANT_CAPACITY 128u
#define DEFINITION_MANAGER_REQUIRED_SIZE 0x5C0Cu
#define DEFINITION_MATERIAL_MAP_OFFSET 0x5C10u
#define DEFINITION_MATERIAL_MAP_STRIDE 0x08u
#define DEFINITION_MATERIAL_PROCOBJ_OFFSET 0x00u
#define DEFINITION_MATERIAL_PLANT_OFFSET 0x04u
#define DEFINITION_MATERIAL_COUNT 256u
#define DEFINITION_MATERIAL_MAP_REQUIRED_SIZE 0x6410u
#define MODEL_INFO_POINTER_CAPACITY 31000u
#define MODEL_INFO_POINTER_SIZE 4u
#define MODEL_INFO_TABLE_REQUIRED_SIZE \
    (MODEL_INFO_POINTER_CAPACITY * MODEL_INFO_POINTER_SIZE)
#define MODEL_INFO_REQUIRED_SIZE 0x60u
#define MODEL_INFO_DRAW_DISTANCE_OFFSET 0x2Cu
#define MODEL_DISTANCE_RECORD_CAPACITY DEFINITION_PROCOBJ_CAPACITY
#define MODEL_DRAW_DISTANCE_MAX 10000.0f
#define PROCOBJ_MIN_SAFE_SPACING 2.0f
#define DEFINITION_VALUE_MAX 1000000.0f
#define DENSITY_CLASS_UNKNOWN 0u
#define DENSITY_CLASS_GRASS 1u
#define DENSITY_CLASS_VEGETATION 2u
#define DENSITY_CLASS_CLUTTER 3u
#define DENSITY_CLASS_OTHER 4u
#define DENSITY_CLASS_MASK_GRASS 1u
#define DENSITY_CLASS_MASK_VEGETATION 2u
#define DENSITY_CLASS_MASK_CLUTTER 4u
#define DENSITY_CLASS_MASK_OTHER 8u
#define DENSITY_CATALOG_NONE 0u
#define DENSITY_CATALOG_STOCK_BASE 1u
#define DENSITY_CATALOG_STOCK_EXPANDED 2u
#define TUNING_GOVERNOR_MARGIN_DIVISOR 32u
#define TUNING_GOVERNOR_MIN_MARGIN 16u
#define TUNING_GOVERNOR_MAX_MARGIN 64u
#define TUNING_GOVERNOR_SUSTAINED_POLLS 5u
#define TUNING_GOVERNOR_REASON_PRESSURE 1u
#define TUNING_GOVERNOR_REASON_DENSITY_MISMATCH 2u
#define TUNING_GOVERNOR_REASON_DISTANCE_MISMATCH 3u
#define TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID 4u
#define TUNING_GOVERNOR_REASON_EXPANSION_EXHAUSTED 5u
#define PROVIDER_BITMAP_POINTER_OFFSET 0x48u
#define PROVIDER_BITMAP_DWORD_COUNT_OFFSET 0x4Cu
#define PROVIDER_BITMAP_BIT_COUNT_OFFSET 0x4Eu
#define PROVIDER_OWNER_POINTER_OFFSET 0x44u
#define PROVIDER_AUXILIARY_POINTER_OFFSET 0x50u
#define PROVIDER_RESOURCE_FLAGS_OFFSET 0x54u
#define DEFINITION_WAIT_LOG_INTERVAL_MS 30000u
#define DEFINITION_STABLE_POLLS 50u
#define LOG_BUFFER_CAPACITY 65536u
#define CRASH_LOG_BUFFER_CAPACITY 8192u
#define MINIDUMP_COMPACT_TYPE 0x00001924u
#define MINIDUMP_COMPACT_FALLBACK_TYPE 0x00001820u
#define MINIDUMP_FULL_OPT_IN_TYPE 0x00001926u

enum PatchStatus {
    STATUS_NOT_RUN = 0,
    STATUS_PATCH_APPLIED = 1,
    STATUS_CODE_ALREADY_PATCHED = 2,
    STATUS_ALREADY_ACTIVE = 3,
    STATUS_NO_CHANGE_REQUESTED = 4,

    STATUS_BAD_MAIN_MODULE = 100,
    STATUS_BAD_PE_IMAGE = 101,
    STATUS_UNSUPPORTED_MACHINE = 102,
    STATUS_INVALID_CONFIG = 103,
    STATUS_SIGNATURE_NOT_FOUND = 104,
    STATUS_SIGNATURE_AMBIGUOUS = 105,
    STATUS_BAD_MANAGER_REFERENCE = 106,
    STATUS_TOO_LATE = 107,
    STATUS_UNEXPECTED_CAPACITY = 108,
    STATUS_VIRTUAL_PROTECT_FAILED = 109,
    STATUS_WRITE_VERIFY_FAILED = 110,
    STATUS_FLUSH_FAILED_REVERTED = 111,
    STATUS_PROTECTION_RESTORE_FAILED = 112,
    STATUS_DISTANCE_INIT_SIGNATURE_NOT_FOUND = 113,
    STATUS_DISTANCE_INIT_SIGNATURE_AMBIGUOUS = 114,
    STATUS_DISTANCE_UPDATE_SIGNATURE_NOT_FOUND = 115,
    STATUS_DISTANCE_UPDATE_SIGNATURE_AMBIGUOUS = 116,
    STATUS_DISTANCE_SOURCE_VALIDATION_FAILED = 117,
    STATUS_DISTANCE_VIRTUAL_PROTECT_FAILED = 118,
    STATUS_DISTANCE_WRITE_VERIFY_FAILED = 119,
    STATUS_DISTANCE_FLUSH_FAILED_REVERTED = 120,
    STATUS_DISTANCE_PROTECTION_RESTORE_FAILED = 121,
    STATUS_CAPACITY_ROLLBACK_FAILED = 122,
    STATUS_PROVIDER_UNSUPPORTED_BUILD = 123,
    STATUS_PROVIDER_CODE_MISMATCH = 124,
    STATUS_PROVIDER_ALLOCATION_FAILED = 125,
    STATUS_PROVIDER_VIRTUAL_PROTECT_FAILED = 126,
    STATUS_PROVIDER_WRITE_VERIFY_FAILED = 127,
    STATUS_PROVIDER_FLUSH_FAILED_REVERTED = 128,
    STATUS_PROVIDER_PROTECTION_RESTORE_FAILED = 129,
    STATUS_PROVIDER_ROLLBACK_FAILED = 130,
    STATUS_DEFINITION_UNSUPPORTED_BUILD = 131,
    STATUS_PROVIDER_INITIALIZATION_VERIFY_FAILED = 132,
    STATUS_GENERATOR_UNSUPPORTED_BUILD = 133,
    STATUS_GENERATOR_CODE_MISMATCH = 134,
    STATUS_GENERATOR_ALLOCATION_FAILED = 135,
    STATUS_GENERATOR_VIRTUAL_PROTECT_FAILED = 136,
    STATUS_GENERATOR_WRITE_VERIFY_FAILED = 137,
    STATUS_GENERATOR_FLUSH_FAILED_REVERTED = 138,
    STATUS_GENERATOR_PROTECTION_RESTORE_FAILED = 139,
    STATUS_GENERATOR_ROLLBACK_FAILED = 140,
    STATUS_REQUIRED_LAYOUT_OUT_OF_RANGE = 141,
    STATUS_UNSUPPORTED_BUILD_PROFILE = 142,
    STATUS_EXECUTABLE_HASH_READ_FAILED = 143,
    STATUS_UNREGISTERED_EXECUTABLE_HASH = 144,
    STATUS_EXECUTABLE_IDENTITY_MISMATCH = 145,
    STATUS_GENERATOR_INITIALIZED_BEFORE_PATCH = 146,
    STATUS_UNIVERSAL_BEHAVIOR_PROFILE_MISSING = 147,
    STATUS_UNIVERSAL_BEHAVIOR_INSTALL_FAILED = 148,
    STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED = 149,
    STATUS_BEHAVIOR_AXIS_ISOLATION_REFUSED = 150,
    STATUS_ASI_IDENTITY_READ_FAILED = 151,
    STATUS_IDENTITY_REVALIDATION_FAILED = 152,
    STATUS_STARTUP_WORKER_PIN_FAILED = 153,
    STATUS_INI_IDENTITY_READ_FAILED = 154,
    STATUS_DEFINITION_LOAD_ALREADY_STARTED = 155,
    STATUS_DEFINITION_CALLBACK_BEFORE_ACTIVATION = 156,
    STATUS_DEFINITION_CALLBACK_MISSED = 157,
    STATUS_STARTUP_QUIESCENCE_FAILED = 158,
    STATUS_STARTUP_QUIESCENCE_IN_FLIGHT = 159,
    STATUS_STARTUP_QUIESCENCE_RESUME_FAILED = 160,
    STATUS_COMPATIBILITY_PROBE_DECLINED = 161,
    STATUS_COMPATIBILITY_PROBE_NO_UNIQUE_FAMILY = 162
};

enum ConfigErrorKind {
    CONFIG_ERROR_NONE = 0,
    CONFIG_ERROR_MISSING = 1,
    CONFIG_ERROR_NOT_WHOLE_NUMBER = 2,
    CONFIG_ERROR_OUT_OF_RANGE = 3,
    CONFIG_ERROR_NOT_DECIMAL_FLOAT = 4
};

enum DefinitionScalingStatus {
    DEFINITION_SCALING_WAITING = 0,
    DEFINITION_SCALING_APPLIED = 1,
    DEFINITION_SCALING_NO_CHANGE = 2,
    DEFINITION_SCALING_ROLLED_BACK_SAFETY = 3,
    DEFINITION_SCALING_BAD_MANAGER = 100,
    DEFINITION_SCALING_BAD_COUNTS = 101,
    DEFINITION_SCALING_BAD_FLOAT = 102,
    DEFINITION_SCALING_WRITE_VERIFY_FAILED = 103,
    DEFINITION_SCALING_ROLLBACK_FAILED = 104,
    DEFINITION_SCALING_FINGERPRINT_REFUSED = 105,
    DEFINITION_SCALING_CALLBACK_BEFORE_ACTIVATION = 106,
    DEFINITION_SCALING_CALLBACK_MISSED = 107
};

enum DefinitionCallbackState {
    DEFINITION_CALLBACK_UNARMED = 0,
    DEFINITION_CALLBACK_ARMED = 1,
    DEFINITION_CALLBACK_RUNNING = 2,
    DEFINITION_CALLBACK_COMPLETE = 3,
    DEFINITION_CALLBACK_FAILED = 4
};

enum TuningGovernorState {
    TUNING_GOVERNOR_STATE_IDLE = 0,
    TUNING_GOVERNOR_STATE_RESTART_REQUIRED = 1
};

enum ModelDistanceScalingStatus {
    MODEL_DISTANCE_WAITING = 0,
    MODEL_DISTANCE_APPLIED = 1,
    MODEL_DISTANCE_NO_CHANGE = 2,
    MODEL_DISTANCE_ROLLED_BACK_SAFETY = 3,
    MODEL_DISTANCE_BAD_DEFINITION_MANAGER = 100,
    MODEL_DISTANCE_BAD_DEFINITION_COUNT = 101,
    MODEL_DISTANCE_BAD_MODEL_TABLE = 102,
    MODEL_DISTANCE_BAD_MODEL_INDEX = 103,
    MODEL_DISTANCE_BAD_MODEL_POINTER = 104,
    MODEL_DISTANCE_BAD_DRAW_DISTANCE = 105,
    MODEL_DISTANCE_WRITE_VERIFY_FAILED = 106,
    MODEL_DISTANCE_ROLLBACK_FAILED = 107,
    MODEL_DISTANCE_BAD_QUERY_RADIUS = 108,
    MODEL_DISTANCE_SAFETY_ROLLBACK_FAILED = 109,
    MODEL_DISTANCE_RETIRED_GLOBAL_MUTATION = 110
};

struct PatchReport {
    DWORD status;
    DWORD requested_capacity;
    DWORD requested_rendered_object_capacity;
    DWORD requested_provider_capacity;
    DWORD capacity_auto_selected;
    DWORD rendered_capacity_auto_selected;
    DWORD provider_capacity_auto_selected;
    DWORD distance_multiplier;
    DWORD corrected_distance_multiplier_bits;
    DWORD plant_density_multiplier;
    DWORD corrected_plant_density_multiplier_bits;
    DWORD procobj_density_multiplier_bits;
    DWORD density_class_mask;
    DWORD debug_log_enabled;
    DWORD hang_watchdog_enabled;
    DWORD hang_timeout_seconds;
    DWORD hang_message_box_enabled;
    DWORD full_hang_dump_enabled;
    DWORD hang_watchdog_thread_started;
    DWORD signature_matches;
    DWORD distance_init_matches;
    DWORD distance_update_matches;
    DWORD target_rva;
    DWORD distance_init_rva;
    DWORD distance_update_rva;
    DWORD original_immediate;
    DWORD final_immediate;
    DWORD manager_rva;
    DWORD manager_capacity;
    DWORD manager_buffer0;
    DWORD manager_buffer1;
    DWORD manager_buffer2;
    DWORD provider_free_head_at_load;
    DWORD provider_active_head_at_load;
    DWORD pe_timestamp;
    DWORD size_of_image;
    DWORD image_base;
    DWORD loaded_header_preferred_image_base;
    DWORD preferred_image_base;
    DWORD image_rebase_delta;
    DWORD old_protection;
    DWORD last_error;
    DWORD flush_result;
    DWORD protection_restore_result;
    DWORD scaled_twenty;
    DWORD scaled_forty;
    DWORD distance_old_protection;
    DWORD distance_flush_result;
    DWORD distance_protection_restore_result;
    DWORD crash_handler_installed;
    DWORD targeted_exception_handler_installed;
    DWORD detail_distance_source;
    DWORD detail_multiplier_source;
    DWORD distance_near_source;
    DWORD distance_far_source;
    DWORD scaled_half_bits;
    DWORD telemetry_thread_started;
    DWORD provider_patch_sites_verified;
    DWORD provider_patch_sites_applied;
    DWORD provider_patch_site_mismatches;
    DWORD provider_first_mismatch_site;
    DWORD provider_first_mismatch_byte;
    DWORD provider_storage;
    DWORD provider_records;
    DWORD provider_storage_bytes;
    DWORD provider_free_list_verified;
    DWORD provider_old_protection;
    DWORD provider_flush_result;
    DWORD provider_protection_restore_result;
    DWORD definition_manager_rva;
    DWORD definition_scaling_status;
    DWORD definition_callback_state;
    DWORD definition_callback_calls;
    DWORD definition_callback_thread_id;
    DWORD startup_quiescence_attempted;
    DWORD startup_quiescence_snapshots;
    DWORD startup_quiescence_threads_suspended;
    DWORD startup_quiescence_unsafe_eip;
    DWORD startup_quiescence_unsafe_stack_return;
    DWORD startup_quiescence_failure_kind;
    DWORD startup_quiescence_last_error;
    DWORD startup_quiescence_resume_failures;
    DWORD startup_quiescence_completed;
    DWORD startup_nonzero_manager_region;
    DWORD startup_nonzero_manager_size;
    DWORD startup_nonzero_manager_offset;
    DWORD startup_nonzero_manager_byte;
    DWORD startup_thread_attach_gate_waits;
    DWORD startup_call_contract_identity_id;
    DWORD startup_call_contract_record_count;
    DWORD startup_call_contract_preimages_verified;
    DWORD startup_call_contract_mismatch_address;
    DWORD startup_guard_interval_count;
    DWORD startup_guard_interval_validation_passes;
    DWORD startup_guard_interval_bytes_verified;
    DWORD startup_guard_interval_relocations_verified;
    DWORD startup_definition_guard_first_rva;
    DWORD startup_definition_guard_last_rva;
    DWORD startup_plant_setter_guard_first_rva;
    DWORD startup_plant_setter_guard_last_rva;
    DWORD procobj_definition_count;
    DWORD plant_definition_count;
    DWORD procobj_catalog_selection;
    DWORD procobj_catalog_actual_fingerprint;
    DWORD procobj_catalog_expected_fingerprint;
    DWORD plant_catalog_actual_fingerprint;
    DWORD plant_catalog_expected_fingerprint;
    DWORD plant_catalog_match;
    DWORD definition_fingerprint_refused;
    DWORD procobj_definitions_selected;
    DWORD procobj_random_definitions_scaled;
    DWORD procobj_grid_definitions_scaled;
    DWORD procobj_definitions_unselected;
    DWORD procobj_definitions_scaled;
    DWORD plant_definitions_scaled;
    DWORD procobj_original_spacing_min_bits;
    DWORD procobj_scaled_spacing_min_bits;
    DWORD procobj_original_inverse_sum_bits;
    DWORD procobj_scaled_inverse_sum_bits;
    DWORD plant_original_density_sum_bits;
    DWORD plant_scaled_density_sum_bits;
    DWORD procobj_spacing_floor_clamps;
    DWORD procobj_value_limit_clamps;
    DWORD plant_value_limit_clamps;
    DWORD procobj_effective_density_min_bits;
    DWORD procobj_effective_density_max_bits;
    DWORD definition_live_fields_verified;
    DWORD definition_live_mismatches;
    DWORD definition_first_live_mismatch_kind;
    DWORD definition_first_live_mismatch_index;
    DWORD plant_live_fields_observed;
    DWORD plant_live_observation_mismatches;
    DWORD tuning_governor_active;
    DWORD tuning_governor_triggered;
    DWORD tuning_governor_reason;
    DWORD tuning_governor_live_writes;
    DWORD tuning_governor_pressure_polls;
    DWORD tuning_governor_trigger_active_wrappers;
    DWORD tuning_governor_trigger_staging_count;
    DWORD tuning_governor_trigger_rendered_entities;
    DWORD tuning_governor_state;
    DWORD tuning_governor_detection_thread_id;
    DWORD model_distance_status;
    DWORD model_info_table_rva;
    DWORD model_info_table_address;
    DWORD model_distance_definitions_examined;
    DWORD model_distance_unique_models;
    DWORD model_distance_duplicate_references;
    DWORD model_distance_models_scaled;
    DWORD model_distance_invalid_definition_index;
    DWORD model_distance_invalid_model_index;
    DWORD model_distance_invalid_model_pointer;
    DWORD model_distance_invalid_draw_distance_bits;
    DWORD model_distance_write_mismatch_index;
    DWORD model_distance_rollback_verified;
    DWORD model_distance_original_min_bits;
    DWORD model_distance_original_max_bits;
    DWORD model_distance_scaled_min_bits;
    DWORD model_distance_scaled_max_bits;
    DWORD model_distance_query_radius_bits;
    DWORD model_distance_models_clamped;
    DWORD model_distance_live_verified;
    DWORD model_distance_live_mismatches;
    DWORD model_distance_first_live_mismatch;
    DWORD generator_manager_rva;
    DWORD generator_patch_sites_verified;
    DWORD generator_patch_sites_applied;
    DWORD generator_patch_site_mismatches;
    DWORD generator_first_mismatch_site;
    DWORD generator_first_mismatch_byte;
    DWORD generator_extra_storage;
    DWORD generator_extra_record_count;
    DWORD generator_extra_storage_bytes;
    DWORD generator_original_free_count;
    DWORD generator_original_active_rendered;
    DWORD generator_original_staging_capacity;
    DWORD generator_original_staging_count;
    DWORD generator_old_protection;
    DWORD generator_flush_result;
    DWORD generator_protection_restore_result;
    DWORD definition_code_verified;
    DWORD compatibility_checks_passed;
    DWORD build_profile_id;
    DWORD executable_identity_id;
    DWORD asi_file_size;
    DWORD asi_hash_valid;
    DWORD ini_file_size;
    DWORD ini_hash_valid;
    DWORD executable_file_size;
    DWORD executable_hash_valid;
    DWORD effective_rendered_object_capacity;
    DWORD generator_expansion_enabled_for_profile;
    DWORD universal_behavior_profile_found;
    DWORD universal_behavior_installed;
    DWORD universal_behavior_failure_stage;
    DWORD universal_behavior_rollback_verified;
    DWORD compatibility_probe_prompted;
    DWORD compatibility_probe_authorized;
    DWORD compatibility_probe_identity_matches;
    DWORD compatibility_probe_family_matches;
    DWORD compatibility_probe_selected_identity_id;
    DWORD compatibility_probe_selected_profile_id;
    DWORD compatibility_probe_safe_continue;
    DWORD startup_worker_started;
    DWORD asi_identity_lock_held;
    DWORD ini_identity_lock_held;
    DWORD executable_identity_lock_held;
    DWORD identity_revalidation_passed;
};

struct ProviderPatch {
    BYTE *address;
    DWORD length;
    BYTE original[PROVIDER_PATCH_MAX_LENGTH];
    BYTE replacement[PROVIDER_PATCH_MAX_LENGTH];
    BYTE observed_at_validation[PROVIDER_PATCH_MAX_LENGTH];
    DWORD original_match;
};

struct GeneratorPatch {
    BYTE *address;
    DWORD length;
    BYTE original[GENERATOR_PATCH_MAX_LENGTH];
    BYTE replacement[GENERATOR_PATCH_MAX_LENGTH];
    BYTE observed_at_validation[GENERATOR_PATCH_MAX_LENGTH];
    DWORD original_match;
};

enum BuildCodeVariant {
    BUILD_CODE_CE = 1,
    BUILD_CODE_PATCH8 = 2
};

enum ProviderPatchVariant {
    PROVIDER_PATCH_VARIANT_CE = 1,
    PROVIDER_PATCH_VARIANT_PATCH8 = 2
};

struct BuildProfile {
    DWORD id;
    const char *name;
    DWORD pe_timestamp;
    DWORD image_size;
    DWORD code_variant;
    DWORD target_rva;
    DWORD target_pattern_length;
    DWORD target_immediate_offset;
    DWORD target_wrapper_distance;
    DWORD distance_init_pattern_length;
    DWORD distance_update_pattern_length;
    DWORD distance_init_offsets[4];
    DWORD manager_rva;
    DWORD definition_manager_rva;
    DWORD model_info_table_rva;
    DWORD definition_call_rva;
    BYTE definition_call_tail[10];
    DWORD generator_manager_rva;
    DWORD generator_pop_rva;
    DWORD generator_allocator_rva;
    DWORD generator_special_cap_rva;
    DWORD generator_render_cap_rva;
    DWORD generator_staging_alloc_rva;
    DWORD generator_staging_cap_rva;
    BYTE generator_allocator_original[11];
    DWORD provider_supported;
    DWORD provider_patch_variant;
    DWORD provider_patch_count;
    DWORD provider_manager_rva;
    DWORD provider_next_index_rva;
    DWORD provider_record_index_rva;
    DWORD provider_sites[PROVIDER_PATCH_CAPACITY];
    DWORD provider_head_quarantine_rva;
    DWORD provider_rebuild_epilogue_rva;
    DWORD procedural_code_first_rva;
    DWORD procedural_code_last_rva;
    DWORD generator_code_first_rva;
    DWORD generator_code_last_rva;
    DWORD source_code_first_rva;
    DWORD source_code_last_rva;
};

struct ExecutableIdentity {
    DWORD id;
    const char *evidence_label;
    const char *sha256;
    DWORD file_size;
    DWORD build_profile_id;
};

/*
 * PERMANENT COMPATIBILITY CONTRACT
 * --------------------------------
 * This registry is append-only. Never replace or renumber an existing
 * entry when adding support for another GTAIV.exe. The native test build
 * exposes a canonical contract hash for every entry, and the release test
 * suite pins the hashes of all previously shipped profiles. A release must
 * fail if any earlier profile disappears or any of its verified mappings
 * change without an explicit compatibility-contract review.
 */
static const struct BuildProfile g_build_profiles[] = {
    {
        1u, "Complete Edition 1.2.0.59",
        0x63D3E735u, 0x01BE6400u, BUILD_CODE_CE,
        0x00886C20u, 35u, 17u, 16u,
        70u, 57u, {10u, 18u, 29u, 47u},
        0x012FB6A0u, 0x012C8FB0u, 0x00E95CD8u, 0x001C1592u,
        {0xE8, 0x74, 0x7D, 0x66, 0x00,
         0xE8, 0xEF, 0x00, 0xE4, 0xFF},
        0x01283290u, 0x0095C4E0u, 0x00809D10u,
        0x00808E25u, 0x0080926Fu,
        0x00809D94u, 0x00809DA8u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xC5, 0x27, 0x15, 0x00},
        1u, PROVIDER_PATCH_VARIANT_CE, 10u,
        0x012FB6A0u, 0x012FB6C8u, 0x012FB670u,
        {0x008869AFu, 0x0088712Fu, 0x008874D9u,
         0x008874DFu, 0x00887510u, 0x00887863u,
         0x00887877u, 0x008878A1u, 0x00887960u,
         0x00887A96u},
        0x008873B9u,
        0x00887416u,
        0x00886600u, 0x00888800u,
        0x00808D00u, 0x0080AA80u,
        0x0080AA90u, 0x0080AD79u
    },
    {
        2u, "Legacy 1.0.8.0 CE-layout variant",
        0x5ED51FD0u, 0x01BE6400u, BUILD_CODE_CE,
        0x00886600u, 35u, 17u, 16u,
        70u, 57u, {10u, 18u, 29u, 47u},
        0x012FB6A0u, 0x012C8FB0u, 0x00E95CD8u, 0x001C1592u,
        {0xE8, 0xB4, 0x77, 0x66, 0x00,
         0xE8, 0xEF, 0x00, 0xE4, 0xFF},
        0x01283290u, 0x0095BEC0u, 0x008095A0u,
        0x008086B5u, 0x00808AFFu,
        0x008096A4u, 0x008096B8u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0x15, 0x29, 0x15, 0x00},
        1u, PROVIDER_PATCH_VARIANT_CE, 10u,
        0x012FB6A0u, 0x012FB6C8u, 0x012FB670u,
        {0x0088638Fu, 0x00886B0Fu, 0x00886EB9u,
         0x00886EBFu, 0x00886EF0u, 0x00887243u,
         0x00887257u, 0x00887281u, 0x00887340u,
         0x00887476u},
        0x00886D99u,
        0x00886DF6u,
        0x00885FE0u, 0x008881E0u,
        0x00808580u, 0x0080A400u,
        0x0080A3A0u, 0x0080A689u
    },
    {
        3u, "Patch 8 1.0.8.0",
        0x57C6FB75u, 0x018B2000u, BUILD_CODE_PATCH8,
        0x00817DC9u, 7u, 3u, 30u,
        73u, 60u, {10u, 18u, 26u, 46u},
        0x012ABCD0u, 0x01020EA8u, 0x00E2C168u, 0x00093CEAu,
        {0xE8, 0x2C, 0xA4, 0x5D, 0x00,
         0xE8, 0x77, 0x0C, 0x5F, 0x00},
        0x00F69120u, 0x00867360u, 0x005F86C0u,
        0x005F8C36u, 0x005F909Du,
        0x005F8A76u, 0x005F8A86u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0x95, 0xEC, 0x26, 0x00},
        1u, PROVIDER_PATCH_VARIANT_PATCH8, 9u,
        0x012ABCD0u, 0x012ABCF8u, 0x012ABCA0u,
        {0x00816144u, 0x0081692Fu, 0x008169D1u,
         0x00816C39u, 0x00816C3Fu, 0x00816C82u,
         0x00817BB0u, 0x00817CE0u, 0x00817D60u, 0u},
        0x00817AF3u,
        0x00817B49u,
        0x00815F00u, 0x00818200u,
        0x005F8600u, 0x005FA000u,
        0x005FA320u, 0x005FA5E9u
    },
    {
        4u, "Complete Edition 1.2.0.59 alternate image",
        0x63D3E735u, 0x01AEB000u, BUILD_CODE_CE,
        0x00886C20u, 35u, 17u, 16u,
        70u, 57u, {10u, 18u, 29u, 47u},
        0x012FB6A0u, 0x012C8FB0u, 0x00E95CD8u, 0x001C1592u,
        {0xE8, 0x74, 0x7D, 0x66, 0x00,
         0xE8, 0xEF, 0x00, 0xE4, 0xFF},
        0x01283290u, 0x0095C4E0u, 0x00809D10u,
        0x00808E25u, 0x0080926Fu,
        0x00809D94u, 0x00809DA8u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xC5, 0x27, 0x15, 0x00},
        1u, PROVIDER_PATCH_VARIANT_CE, 10u,
        0x012FB6A0u, 0x012FB6C8u, 0x012FB670u,
        {0x008869AFu, 0x0088712Fu, 0x008874D9u,
         0x008874DFu, 0x00887510u, 0x00887863u,
         0x00887877u, 0x008878A1u, 0x00887960u,
         0x00887A96u},
        0x008873B9u,
        0x00887416u,
        0x00886600u, 0x00888800u,
        0x00808D00u, 0x0080AA80u,
        0x0080AA90u, 0x0080AD79u
    },
    {
        5u, "Complete Edition 1.2.0.59 exact-hash stability method",
        0x63D3E735u, 0x01BE6400u, BUILD_CODE_CE,
        0x00886C20u, 35u, 17u, 16u,
        70u, 57u, {10u, 18u, 29u, 47u},
        0x012FB6A0u, 0x012C8FB0u, 0x00E95CD8u, 0x001C1592u,
        {0xE8, 0x74, 0x7D, 0x66, 0x00,
         0xE8, 0xEF, 0x00, 0xE4, 0xFF},
        0x01283290u, 0x0095C4E0u, 0x00809D10u,
        0x00808E25u, 0x0080926Fu,
        0x00809D94u, 0x00809DA8u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xC5, 0x27, 0x15, 0x00},
        1u, PROVIDER_PATCH_VARIANT_CE, 10u,
        0x012FB6A0u, 0x012FB6C8u, 0x012FB670u,
        {0x008869AFu, 0x0088712Fu, 0x008874D9u,
         0x008874DFu, 0x00887510u, 0x00887863u,
         0x00887877u, 0x008878A1u, 0x00887960u,
         0x00887A96u},
        0x008873B9u,
        0x00887416u,
        0x00886600u, 0x00888800u,
        0x00808D00u, 0x0080AA80u,
        0x0080AA90u, 0x0080AD79u
    },
    {
        6u, "Complete Edition 1.2.0.59 alternate-image stability method",
        0x63D3E735u, 0x01AEB000u, BUILD_CODE_CE,
        0x00886C20u, 35u, 17u, 16u,
        70u, 57u, {10u, 18u, 29u, 47u},
        0x012FB6A0u, 0x012C8FB0u, 0x00E95CD8u, 0x001C1592u,
        {0xE8, 0x74, 0x7D, 0x66, 0x00,
         0xE8, 0xEF, 0x00, 0xE4, 0xFF},
        0x01283290u, 0x0095C4E0u, 0x00809D10u,
        0x00808E25u, 0x0080926Fu,
        0x00809D94u, 0x00809DA8u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xC5, 0x27, 0x15, 0x00},
        1u, PROVIDER_PATCH_VARIANT_CE, 10u,
        0x012FB6A0u, 0x012FB6C8u, 0x012FB670u,
        {0x008869AFu, 0x0088712Fu, 0x008874D9u,
         0x008874DFu, 0x00887510u, 0x00887863u,
         0x00887877u, 0x008878A1u, 0x00887960u,
         0x00887A96u},
        0x008873B9u,
        0x00887416u,
        0x00886600u, 0x00888800u,
        0x00808D00u, 0x0080AA80u,
        0x0080AA90u, 0x0080AD79u
    },
    {
        7u, "Patch 7 1.0.7.0",
        0x4BD9EFBEu, 0x01851000u, BUILD_CODE_PATCH8,
        0x00524BC9u, 7u, 3u, 30u,
        73u, 60u, {10u, 18u, 26u, 46u},
        0x0113FA80u, 0x00FD3630u, 0x011F73B0u, 0x0002016Au,
        {0xE8, 0x3C, 0x80, 0x4E, 0x00,
         0xE8, 0x67, 0xAD, 0x8D, 0x00},
        0x0112F770u, 0x0074F1C0u, 0x0051D0D0u,
        0x0051D6C6u, 0x0051DB2Du,
        0x0051D506u, 0x0051D516u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xE5, 0x20, 0x23, 0x00},
        1u, PROVIDER_PATCH_VARIANT_PATCH8, 9u,
        0x0113FA80u, 0x0113FAA8u, 0x0113FA50u,
        {0x00522F14u, 0x0052372Fu, 0x005237D1u,
         0x00523A39u, 0x00523A3Fu, 0x00523A82u,
         0x005249B0u, 0x00524AE0u, 0x00524B60u, 0u},
        0x005248F3u,
        0x00524949u,
        0x00522D00u, 0x00525000u,
        0x0051D000u, 0x0051E723u,
        0x0051EDB0u, 0x0051F079u
    },
    {
        8u, "Patch 4 1.0.4.0",
        0x4A1AE9B0u, 0x01679000u, BUILD_CODE_PATCH8,
        0x00781419u, 7u, 3u, 30u,
        73u, 60u, {10u, 18u, 26u, 46u},
        0x010F9290u, 0x011B1700u, 0x00CC90F8u, 0x0008E4D6u,
        {0xE8, 0x00, 0x4F, 0x75, 0x00,
         0xE8, 0x0B, 0x2E, 0x75, 0x00},
        0x00E80190u, 0x007AB890u, 0x0053DFD0u,
        0x0053E546u, 0x0053E9ADu,
        0x0053E386u, 0x0053E396u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0xB5, 0xD8, 0x26, 0x00},
        1u, PROVIDER_PATCH_VARIANT_PATCH8, 9u,
        0x010F9290u, 0x010F92B8u, 0x010F9260u,
        {0x0077F794u, 0x0077FF7Fu, 0x00780021u,
         0x00780289u, 0x0078028Fu, 0x007802D2u,
         0x00781200u, 0x00781330u, 0x007813B0u, 0u},
        0x00781143u,
        0x00781199u,
        0x0077F600u, 0x00781600u,
        0x0053DF00u, 0x00540000u,
        0x0053FC30u, 0x0053FEF9u
    },
    /* V1.2_NEW_TARGET_PROFILE_9: independently checked VCNGE exact image. */
{
        9u, "Patch 8 VCNGE exact 2D230E3C",
        0x57C6FB75u, 0x018C8000u, BUILD_CODE_PATCH8,
        0x00817DC9u, 7u, 3u, 30u,
        73u, 60u, {10u, 18u, 26u, 46u},
        0x012ABCD0u, 0x01020EA8u, 0x00E2C168u, 0x00093CEAu,
        {0xE8, 0x2C, 0xA4, 0x5D, 0x00,
         0xE8, 0x77, 0x0C, 0x5F, 0x00},
        0x00F69120u, 0x00867360u, 0x005F86C0u,
        0x005F8C36u, 0x005F909Du,
        0x005F8A76u, 0x005F8A86u,
        {0x81, 0xC1, 0x08, 0x30, 0x00, 0x00,
         0xE9, 0x95, 0xEC, 0x26, 0x00},
        1u, PROVIDER_PATCH_VARIANT_PATCH8, 9u,
        0x012ABCD0u, 0x012ABCF8u, 0x012ABCA0u,
        {0x00816144u, 0x0081692Fu, 0x008169D1u,
         0x00816C39u, 0x00816C3Fu, 0x00816C82u,
         0x00817BB0u, 0x00817CE0u, 0x00817D60u, 0u},
        0x00817AF3u,
        0x00817B49u,
        0x00815F00u, 0x00818200u,
        0x005F8600u, 0x005FA000u,
        0x005FA320u, 0x005FA5E9u
    },
};

static const DWORD g_build_profile_count =
    sizeof(g_build_profiles) / sizeof(g_build_profiles[0]);
static const struct BuildProfile *g_build_profile = &g_build_profiles[2];

/*
 * Exact executable identities are append-only. A SHA-256 may be listed only
 * once; duplicate user submissions remain separate in permanent evidence but
 * resolve to the same immutable identity here. The identity selects a patch
 * method/profile. PE metadata is checked afterwards as a second guard and is
 * never used to guess a method for an unknown executable.
 */
static const struct ExecutableIdentity g_executable_identities[] = {
#include "grass_compat_main.generated.inc"
};
static const DWORD g_executable_identity_count =
    sizeof(g_executable_identities) /
    sizeof(g_executable_identities[0]);
static const struct ExecutableIdentity *g_executable_identity;

/* Generated from independently decoded exact executable specimens. */
#include "startup_call_catalog.generated.inc"

static const DWORD g_startup_exact_call_contract_count =
    sizeof(g_startup_exact_call_contracts) /
    sizeof(g_startup_exact_call_contracts[0]);

static const struct GuStartupExactCallContract *
startup_find_exact_call_contract(DWORD executable_identity_id)
{
    const struct GuStartupExactCallContract *match = NULL;
    DWORD index;
    for (index = 0u;
         index < g_startup_exact_call_contract_count;
         ++index) {
        const struct GuStartupExactCallContract *candidate =
            &g_startup_exact_call_contracts[index];
        if (candidate->executable_identity_id !=
                executable_identity_id) {
            continue;
        }
        if (match) {
            return NULL;
        }
        match = candidate;
    }
    return match;
}

struct LockedFileIdentity {
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION snapshot;
    DWORD file_size;
    char sha256[65];
};

static HMODULE g_self_module;
static struct LockedFileIdentity g_self_file_identity;
static struct LockedFileIdentity g_ini_file_identity;
static struct LockedFileIdentity g_main_file_identity;
static volatile LONG g_startup_worker_state;
static volatile LONG g_startup_loader_callback_calls;
static volatile LONG g_startup_loader_callback_deferred;
static volatile LONG g_startup_loader_callback_timeouts;
static struct PatchReport g_report;
static char g_self_path[MAX_PATH];
static char g_main_path[MAX_PATH];
static char g_ini_path[MAX_PATH];
static char g_log_path[MAX_PATH];
static char g_crash_dump_path[MAX_PATH];
static char g_crash_fallback_dump_path[MAX_PATH];
static char g_hang_dump_path[MAX_PATH];
static char g_hang_fallback_dump_path[MAX_PATH];
static char g_hang_summary_path[MAX_PATH];
static char g_self_sha256[65];
static char g_ini_sha256[65];
static char g_main_sha256[65];
static char g_config_error_key[64];
static char g_config_error_value[64];
static DWORD g_config_error_kind;
static DWORD g_config_error_min;
static DWORD g_config_error_max;
static DWORD g_config_error_float_range;
static char g_config_error_min_text[16];
static char g_config_error_max_text[16];
static char g_startup_error_message[1024];
static char g_compatibility_probe_message[1024];
static volatile LONG g_compatibility_probe_prompted;
static volatile LONG g_compatibility_probe_authorized;
static volatile LONG g_compatibility_probe_safe_continue;
static DWORD g_debug_log_enabled = DEFAULT_DEBUG_LOG;
static char g_log_buffer[LOG_BUFFER_CAPACITY];
static DWORD g_log_length;
static float g_scaled_half = 0.5f;
static float g_scaled_twenty = 20.0f;
static float g_scaled_forty = 40.0f;
static LONG g_crash_dump_started;
static LONG g_hang_dump_started;
static volatile LONG g_hang_watchdog_stop;
static volatile LONG g_hang_consecutive_failures;
static volatile LONG g_hang_last_probe_error;
static volatile LONG g_hang_last_window;
static DWORD g_existing_hang_dump_at_start;
static DWORD g_hang_watchdog_enabled = DEFAULT_HANG_WATCHDOG;
static DWORD g_hang_timeout_seconds = DEFAULT_HANG_TIMEOUT_SECONDS;
static DWORD g_hang_message_box_enabled = DEFAULT_HANG_MESSAGE_BOX;
static DWORD g_full_hang_dump_enabled = DEFAULT_FULL_HANG_DUMP;
static LPTOP_LEVEL_EXCEPTION_FILTER g_previous_unhandled_filter;
static DWORD g_unhandled_filter_installed;
static DWORD g_existing_crash_dump_at_start;
static PVOID g_targeted_vectored_handler;
static LONG g_targeted_exception_logged;
static BYTE *g_manager;
static BYTE *g_provider_storage;
static BYTE *g_provider_records;
static BYTE *g_provider_rebuild_stub;
static DWORD g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
static BYTE *g_definition_manager;
static volatile LONG g_definition_callback_state;
static volatile LONG g_definition_callback_calls;
#ifdef GTAIVPOOL_TEST_EXPORTS
static volatile LONG g_definition_callback_test_pause_unarmed;
static volatile LONG g_definition_callback_test_unarmed_reached;
static volatile LONG g_definition_callback_test_unarmed_release;
static volatile LONG g_definition_callback_test_force_success;
#endif
static BYTE *g_generator_manager;
static BYTE *g_generator_extra_records;
static DWORD g_generator_extra_record_count;
static DWORD g_generator_render_capacity =
    VANILLA_RENDERED_OBJECT_CAPACITY;
static void *(*g_generator_builtin_pop)(void);
static volatile LONG g_generator_extra_issued;
static volatile LONG g_generator_extra_exhaustion_observed;
static volatile LONG g_generator_fallback_calls;
static volatile LONG g_generator_active_high_watermark;
static volatile LONG g_generator_wrapper_high_watermark;
static volatile LONG g_generator_staging_high_watermark;
static DWORD g_plant_density_multiplier =
    DEFAULT_PLANT_DENSITY_MULTIPLIER;
static float g_corrected_plant_density_multiplier =
    DEFAULT_CORRECTED_PLANT_DENSITY_MULTIPLIER;
static volatile LONG g_plant_selector_positive_calls;
static volatile LONG g_plant_selector_neutral_calls;
static volatile LONG g_plant_selector_last_output_key;
static volatile LONG g_plant_selector_last_source_hash;
static volatile LONG g_plant_selector_last_submesh;
static float g_procobj_density_multiplier =
    DEFAULT_PROCOBJ_DENSITY_MULTIPLIER;
static DWORD g_density_class_mask = DEFAULT_DENSITY_CLASS_MASK;
static float g_configured_plant_density_multiplier = 1.0f;
static float g_configured_procobj_density_multiplier = 1.0f;
static struct ProviderPatch g_provider_patch_manifest[
    PROVIDER_PATCH_CAPACITY];
static DWORD g_provider_manifest_configured;
static struct GeneratorPatch g_generator_patch_manifest[
    GENERATOR_PATCH_COUNT];
static DWORD g_generator_manifest_configured;
/*
 * These flags distinguish a completely installed patch set from a
 * transaction that began writing but has not yet restored its code-page
 * protection.  A dirty transaction must be rolled back even when the
 * public "sites applied" count has not reached its normal success point.
 */
static volatile LONG g_provider_patch_transaction_dirty;
static volatile LONG g_generator_patch_transaction_dirty;
static volatile LONG g_capacity_patch_transaction_dirty;
#ifdef GTAIVPOOL_TEST_NO_FATAL
static volatile LONG g_fixture_patch_protection_restore_failures;
#endif
static float g_definition_original_proc_spacing[
    DEFINITION_PROCOBJ_CAPACITY];
static float g_definition_original_proc_inverse[
    DEFINITION_PROCOBJ_CAPACITY];
static float g_definition_scaled_proc_spacing[
    DEFINITION_PROCOBJ_CAPACITY];
static float g_definition_scaled_proc_inverse[
    DEFINITION_PROCOBJ_CAPACITY];
static float g_definition_effective_proc_density[
    DEFINITION_PROCOBJ_CAPACITY];
static BYTE g_definition_proc_class[
    DEFINITION_PROCOBJ_CAPACITY];
static BYTE g_definition_proc_selected[
    DEFINITION_PROCOBJ_CAPACITY];
static BYTE g_definition_proc_usegrid[
    DEFINITION_PROCOBJ_CAPACITY];
static BYTE g_definition_proc_mutated[
    DEFINITION_PROCOBJ_CAPACITY];
static float g_definition_original_plant_density[
    DEFINITION_PLANT_CAPACITY];
static float g_definition_scaled_plant_density[
    DEFINITION_PLANT_CAPACITY];
static DWORD g_model_distance_model_indices[
    MODEL_DISTANCE_RECORD_CAPACITY];
static BYTE *g_model_distance_model_pointers[
    MODEL_DISTANCE_RECORD_CAPACITY];
static float g_model_distance_original[
    MODEL_DISTANCE_RECORD_CAPACITY];
static float g_model_distance_scaled[
    MODEL_DISTANCE_RECORD_CAPACITY];
static volatile LONG g_telemetry_stop;
static volatile LONG g_provider_exhaustion_observed;
static volatile LONG g_provider_rebuilds_verified;
static volatile LONG g_provider_rebuild_failures;
static volatile LONG g_surface_exhaustion_observed;
static volatile LONG g_tuning_governor_pressure_polls;
static volatile LONG g_tuning_governor_triggered;
static volatile LONG g_tuning_governor_state;
static volatile LONG g_tuning_governor_event_pending;
static volatile LONG g_background_threads_ready;
static volatile LONG g_startup_patch_thread_attach_gate;
static volatile LONG g_startup_thread_attach_gate_waits;

static DWORD tuning_governor_near_capacity_threshold(DWORD capacity);
static void tuning_safety_governor_poll(void);
static void trigger_tuning_safety_governor(
    DWORD reason, DWORD active_wrappers,
    DWORD staging_count, DWORD rendered_entities);
static int wait_for_background_threads_ready(
    volatile LONG *stop_flag);
static void copy_string(
    char *destination, DWORD capacity, const char *source);

struct MiniDumpExceptionInformationLocal {
    DWORD thread_id;
    PEXCEPTION_POINTERS exception_pointers;
    BOOL client_pointers;
};

typedef BOOL (WINAPI *MiniDumpWriteDumpFunction)(
    HANDLE process, DWORD process_id, HANDLE file, DWORD dump_type,
    const struct MiniDumpExceptionInformationLocal *exception_information,
    const void *user_stream_information,
    const void *callback_information);
static HMODULE g_dbghelp_module;
static MiniDumpWriteDumpFunction g_mini_dump_write_dump;

typedef LPTOP_LEVEL_EXCEPTION_FILTER
    (WINAPI *SetUnhandledExceptionFilterFunction)(
        LPTOP_LEVEL_EXCEPTION_FILTER filter);
static SetUnhandledExceptionFilterFunction
    g_set_unhandled_exception_filter;
typedef PVOID (WINAPI *AddVectoredExceptionHandlerFunction)(
    ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
typedef ULONG (WINAPI *RemoveVectoredExceptionHandlerFunction)(
    PVOID handle);
static RemoveVectoredExceptionHandlerFunction
    g_remove_vectored_exception_handler;

static int startup_patch_span_add(
    struct GuStartupPatchSpan *spans, DWORD *count,
    BYTE *address, DWORD length, const BYTE *original)
{
    struct GuStartupPatchSpan *span;
    if (!spans || !count || !address || !original || length == 0u ||
        *count >= GU_STARTUP_MAX_PATCH_SPANS) {
        return 0;
    }
    span = &spans[*count];
    ZeroMemory(span, sizeof(*span));
    span->address = address;
    span->length = length;
    span->expected = original;
    ++*count;
    return 1;
}

static int startup_patch_spans_match_preimages(
    const struct GuStartupPatchSpan *spans, DWORD count,
    DWORD *mismatch_address)
{
    DWORD index;
    if (!spans || count == 0u || !mismatch_address) {
        return 0;
    }
    *mismatch_address = 0u;
    for (index = 0u; index < count; ++index) {
        DWORD byte_index;
        if (!spans[index].address ||
            !spans[index].expected ||
            spans[index].length == 0u) {
            *mismatch_address =
                (DWORD)(ULONG_PTR)spans[index].address;
            return 0;
        }
        for (byte_index = 0u;
             byte_index < spans[index].length;
             ++byte_index) {
            if (spans[index].address[byte_index] !=
                    spans[index].expected[byte_index]) {
                *mismatch_address = (DWORD)(ULONG_PTR)(
                    spans[index].address + byte_index);
                return 0;
            }
        }
    }
    return 1;
}

static int startup_patch_spans_are_disjoint_in_image(
    const struct GuStartupPatchSpan *spans, DWORD count,
    BYTE *image_base, DWORD image_size)
{
    DWORD index;
    DWORD image_start;
    DWORD image_end;
    if (!spans || count == 0u || !image_base || image_size == 0u) {
        return 0;
    }
    image_start = (DWORD)(ULONG_PTR)image_base;
    image_end = image_start + image_size;
    if (image_end < image_start) {
        return 0;
    }
    for (index = 0u; index < count; ++index) {
        DWORD start = (DWORD)(ULONG_PTR)spans[index].address;
        DWORD end = start + spans[index].length;
        DWORD other;
        if (spans[index].length == 0u || end < start ||
            start < image_start || end > image_end) {
            return 0;
        }
        for (other = index + 1u; other < count; ++other) {
            DWORD other_start =
                (DWORD)(ULONG_PTR)spans[other].address;
            DWORD other_end = other_start + spans[other].length;
            if (other_end < other_start ||
                (start < other_end && other_start < end)) {
                return 0;
            }
        }
    }
    return 1;
}

static int startup_bytes_are_zero(
    const BYTE *address, DWORD length)
{
    DWORD index;
    if (!address || length == 0u) {
        return 0;
    }
    for (index = 0u; index < length; ++index) {
        BYTE value = address[index];
        if (value != 0u) {
            /* A single first-failure byte distinguishes initialization from
             * unknown nonzero fields without dumping game memory. This is
             * observation only: the original zero-state refusal is unchanged.
             * Region: 1=surface/provider, 2=definition, 3=generator. */
            g_report.startup_nonzero_manager_region =
                address == g_manager ? 1u :
                address == g_definition_manager ? 2u :
                address == g_generator_manager ? 3u : 0u;
            g_report.startup_nonzero_manager_size = length;
            g_report.startup_nonzero_manager_offset = index;
            g_report.startup_nonzero_manager_byte = (DWORD)value;
            return 0;
        }
    }
    return 1;
}

static int startup_exact_call_contract_is_structurally_valid(
    const struct GuStartupExactCallContract *contract,
    DWORD image_size)
{
    DWORD index;
    DWORD previous_return = 0u;
    if (!contract || image_size == 0u ||
        contract->executable_identity_id == 0u ||
        contract->build_profile_id == 0u ||
        contract->preferred_image_base == 0u ||
        contract->interval_count !=
            GU_STARTUP_EXACT_GUARD_INTERVALS ||
        !contract->intervals ||
        contract->return_count == 0u ||
        contract->return_count >
            GU_STARTUP_MAX_EXACT_RETURN_RVAS ||
        !contract->return_rvas ||
        !contract->target_id || !contract->specimen_sha256 ||
        !contract->guard_interval_contract_sha256 ||
        !contract->return_rva_catalog_sha256 ||
        !contract->full_startup_authority_sha256 ||
        contract->target_id[0] == '\0' ||
        contract->specimen_sha256[0] == '\0' ||
        contract->guard_interval_contract_sha256[0] == '\0' ||
        contract->return_rva_catalog_sha256[0] == '\0' ||
        contract->full_startup_authority_sha256[0] == '\0') {
        return 0;
    }
    for (index = 0u; index < contract->interval_count; ++index) {
        const struct GuStartupGuardInterval *interval =
            &contract->intervals[index];
        DWORD guard_last;
        DWORD validation_last;
        DWORD relocation_index;
        DWORD previous_relocation = 0u;
        if (interval->role != index + 1u ||
            interval->guard_first_rva == 0u ||
            interval->guard_length == 0u ||
            interval->guard_first_rva >= image_size ||
            interval->guard_length >
                image_size - interval->guard_first_rva ||
            interval->validation_length == 0u ||
            interval->validation_first_rva >= image_size ||
            interval->validation_length >
                image_size - interval->validation_first_rva ||
            !interval->preferred_bytes ||
            (!interval->relocation_offsets &&
             interval->relocation_count != 0u)) {
            return 0;
        }
        guard_last = interval->guard_first_rva +
            interval->guard_length;
        validation_last = interval->validation_first_rva +
            interval->validation_length;
        if (guard_last < interval->guard_first_rva ||
            validation_last < interval->validation_first_rva ||
            interval->validation_first_rva >
                interval->guard_first_rva ||
            validation_last < guard_last ||
            interval->guard_first_rva -
                interval->validation_first_rva <
                    GU_STARTUP_GUARD_HALO_LENGTH ||
            validation_last - guard_last <
                GU_STARTUP_GUARD_HALO_LENGTH) {
            return 0;
        }
        for (relocation_index = 0u;
             relocation_index < interval->relocation_count;
             ++relocation_index) {
            DWORD offset =
                interval->relocation_offsets[relocation_index];
            if (offset > interval->validation_length - 4u ||
                (relocation_index != 0u &&
                 offset < previous_relocation + 4u)) {
                return 0;
            }
            previous_relocation = offset;
        }
    }
    for (index = 0u; index < contract->return_count; ++index) {
        DWORD return_rva = contract->return_rvas[index];
        if (return_rva == 0u || return_rva >= image_size ||
            (index != 0u && return_rva <= previous_return)) {
            return 0;
        }
        previous_return = return_rva;
    }
    return 1;
}

static const struct GuStartupGuardInterval *
startup_guard_interval_by_role(
    const struct GuStartupExactCallContract *contract,
    DWORD role)
{
    if (!contract || !contract->intervals || role == 0u ||
        role > contract->interval_count ||
        contract->intervals[role - 1u].role != role) {
        return NULL;
    }
    return &contract->intervals[role - 1u];
}

static int startup_guard_interval_contains_rva_range(
    const struct GuStartupExactCallContract *contract,
    DWORD role, DWORD first_rva, DWORD last_rva)
{
    const struct GuStartupGuardInterval *interval =
        startup_guard_interval_by_role(contract, role);
    DWORD interval_last;
    if (!interval || last_rva <= first_rva) {
        return 0;
    }
    interval_last = interval->guard_first_rva +
        interval->guard_length;
    return interval_last >= interval->guard_first_rva &&
        first_rva >= interval->guard_first_rva &&
        last_rva <= interval_last;
}

static int startup_exact_return_contract_contains_rva(
    const struct GuStartupExactCallContract *contract,
    DWORD return_rva)
{
    DWORD low = 0u;
    DWORD high;
    if (!contract || !contract->return_rvas) {
        return 0;
    }
    high = contract->return_count;
    while (low < high) {
        DWORD middle = low + (high - low) / 2u;
        if (contract->return_rvas[middle] == return_rva) {
            return 1;
        }
        if (contract->return_rvas[middle] < return_rva) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }
    return 0;
}

static int startup_guard_intervals_match_preimages(
    const struct GuStartupExactCallContract *contract,
    BYTE *image_base, DWORD image_size,
    DWORD *mismatch_address, DWORD *verified_bytes,
    DWORD *verified_relocations)
{
    DWORD interval_index;
    DWORD loaded_base;
    DWORD relocation_delta;
    if (!image_base || !mismatch_address || !verified_bytes ||
        !verified_relocations ||
        !startup_exact_call_contract_is_structurally_valid(
            contract, image_size)) {
        return 0;
    }
    *mismatch_address = 0u;
    *verified_bytes = 0u;
    *verified_relocations = 0u;
    loaded_base = (DWORD)(ULONG_PTR)image_base;
    relocation_delta = loaded_base - contract->preferred_image_base;
    for (interval_index = 0u;
         interval_index < contract->interval_count;
         ++interval_index) {
        const struct GuStartupGuardInterval *interval =
            &contract->intervals[interval_index];
        DWORD byte_index = 0u;
        DWORD relocation_index = 0u;
        while (byte_index < interval->validation_length) {
            if (relocation_index < interval->relocation_count &&
                byte_index ==
                    interval->relocation_offsets[relocation_index]) {
                DWORD component;
                DWORD preferred_value =
                    (DWORD)interval->preferred_bytes[byte_index] |
                    ((DWORD)interval->preferred_bytes[byte_index + 1u]
                        << 8u) |
                    ((DWORD)interval->preferred_bytes[byte_index + 2u]
                        << 16u) |
                    ((DWORD)interval->preferred_bytes[byte_index + 3u]
                        << 24u);
                DWORD expected_value =
                    preferred_value + relocation_delta;
                for (component = 0u; component < 4u; ++component) {
                    BYTE expected = (BYTE)(
                        expected_value >> (component * 8u));
                    if (image_base[
                            interval->validation_first_rva +
                            byte_index + component] != expected) {
                        *mismatch_address = loaded_base +
                            interval->validation_first_rva +
                            byte_index + component;
                        return 0;
                    }
                    ++*verified_bytes;
                }
                ++*verified_relocations;
                ++relocation_index;
                byte_index += 4u;
                continue;
            }
            if (image_base[interval->validation_first_rva +
                    byte_index] !=
                    interval->preferred_bytes[byte_index]) {
                *mismatch_address = loaded_base +
                    interval->validation_first_rva + byte_index;
                return 0;
            }
            ++*verified_bytes;
            ++byte_index;
        }
        if (relocation_index != interval->relocation_count) {
            return 0;
        }
    }
    return 1;
}

static int startup_quiescence_has_live_tid(
    struct GuStartupQuiescence *guard, DWORD thread_id)
{
    DWORD index;
    for (index = 0u; index < guard->count; ++index) {
        if (guard->thread_ids[index] != thread_id ||
            !guard->handles[index]) {
            continue;
        }
        DWORD wait_result = WaitForSingleObject(
            guard->handles[index], 0u);
        if (wait_result == WAIT_TIMEOUT) {
            return 1;
        }
        if (wait_result != WAIT_OBJECT_0) {
            return -1;
        }
        CloseHandle(guard->handles[index]);
        guard->handles[index] = NULL;
        guard->thread_ids[index] = 0u;
        guard->live[index] = 0u;
    }
    return 0;
}

static int startup_quiescence_terminate_now(DWORD status)
{
    /* Production-safe behavior: never terminate the host process.
     * The caller receives a failure and must keep the transaction stopped. */
    g_report.status = status;
    return 0;
}

static int startup_quiescence_resume_all(
    struct GuStartupQuiescence *guard)
{
    DWORD index;
    DWORD failures = 0u;
    if (!guard) {
        return 0;
    }
    index = guard->count;
    while (index > 0u) {
        DWORD attempt;
        int released = 0;
        --index;
        if (!guard->handles[index]) {
            continue;
        }
        if (guard->live[index]) {
            for (attempt = 0u;
                 attempt < GU_STARTUP_RESUME_ATTEMPTS;
                 ++attempt) {
                DWORD resume_result =
                    ResumeThread(guard->handles[index]);
                if (resume_result != 0xFFFFFFFFu) {
                    released = 1;
                    break;
                }
                if (WaitForSingleObject(
                        guard->handles[index], 0u) ==
                        WAIT_OBJECT_0) {
                    guard->live[index] = 0u;
                    released = 1;
                    break;
                }
                SwitchToThread();
            }
            if (!released) {
                /* Keep the exact live handle. Closing it here would make the
                 * suspension unrecoverable and could strand a lock owner. */
                ++failures;
                continue;
            }
        }
        CloseHandle(guard->handles[index]);
        guard->handles[index] = NULL;
        guard->live[index] = 0u;
        guard->thread_ids[index] = 0u;
    }
    g_report.startup_quiescence_resume_failures += failures;
    if (failures != 0u) {
        g_report.status = STATUS_STARTUP_QUIESCENCE_RESUME_FAILED;
#ifndef GTAIVPOOL_TEST_NO_FATAL
        /* No logging, loader work, heap work, or cleanup is safe while a
         * possibly lock-owning thread remains stopped. Terminate directly
         * from this fixed-storage path; if the syscall itself cannot finish,
         * retain the attach gate and yield forever rather than run torn. */
        startup_quiescence_terminate_now(
            STATUS_STARTUP_QUIESCENCE_RESUME_FAILED);
#else
        return 0;
#endif
    }
    guard->count = 0u;
    guard->active = 0u;
    InterlockedExchange(
        &g_startup_patch_thread_attach_gate, 0);
    g_report.startup_thread_attach_gate_waits =
        (DWORD)InterlockedCompareExchange(
            &g_startup_thread_attach_gate_waits, 0, 0);
    return 1;
}

static int startup_memory_range_is_readable(
    const BYTE *address, DWORD length)
{
    MEMORY_BASIC_INFORMATION information;
    DWORD start;
    DWORD end;
    DWORD region_start;
    DWORD region_end;
    DWORD base_protection;
    if (!address || length == 0u) {
        return 0;
    }
    start = (DWORD)(ULONG_PTR)address;
    end = start + length;
    if (end < start ||
        VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u) {
        return 0;
    }
    base_protection = information.Protect & 0xFFu;
    if (base_protection != PAGE_READONLY &&
        base_protection != PAGE_READWRITE &&
        base_protection != PAGE_WRITECOPY &&
        base_protection != PAGE_EXECUTE_READ &&
        base_protection != PAGE_EXECUTE_READWRITE &&
        base_protection != PAGE_EXECUTE_WRITECOPY) {
        return 0;
    }
    region_start = (DWORD)(ULONG_PTR)information.BaseAddress;
    region_end = region_start + (DWORD)information.RegionSize;
    return region_end >= region_start &&
        start >= region_start && end <= region_end;
}

static int startup_address_is_in_unsafe_code(
    DWORD address, BYTE *image_base,
    const struct GuStartupExactCallContract *call_contract)
{
    DWORD index;
    DWORD base;
    if (!image_base || !call_contract || !call_contract->intervals) {
        return 0;
    }
    base = (DWORD)(ULONG_PTR)image_base;
    for (index = 0u; index < call_contract->interval_count; ++index) {
        const struct GuStartupGuardInterval *interval =
            &call_contract->intervals[index];
        DWORD first = base + interval->guard_first_rva;
        DWORD last = first + interval->guard_length;
        if (first >= base && last >= first &&
            address >= first && address < last) {
            return 1;
        }
    }
    return 0;
}

static int startup_range_overlaps_unsafe_code(
    DWORD first, DWORD last, BYTE *image_base,
    const struct GuStartupExactCallContract *call_contract)
{
    DWORD index;
    DWORD base;
    if (!image_base || !call_contract || !call_contract->intervals ||
        last <= first) {
        return 0;
    }
    base = (DWORD)(ULONG_PTR)image_base;
    for (index = 0u; index < call_contract->interval_count; ++index) {
        const struct GuStartupGuardInterval *interval =
            &call_contract->intervals[index];
        DWORD guard_first = base + interval->guard_first_rva;
        DWORD guard_last = guard_first + interval->guard_length;
        if (guard_first >= base && guard_last >= guard_first &&
            first < guard_last && guard_first < last) {
            return 1;
        }
    }
    return 0;
}

static int startup_main_image_range_is_executable(
    BYTE *image_base, DWORD image_size,
    DWORD first, DWORD last)
{
    PIMAGE_DOS_HEADER dos_header;
    PIMAGE_NT_HEADERS32 nt_headers;
    PIMAGE_SECTION_HEADER section;
    DWORD base;
    DWORD index;
    if (!image_base || image_size == 0u || last <= first) {
        return 0;
    }
    base = (DWORD)(ULONG_PTR)image_base;
    if (first < base || last < base ||
        first - base >= image_size ||
        last - base > image_size ||
        !startup_memory_range_is_readable(
            (const BYTE *)(ULONG_PTR)first, last - first)) {
        return 0;
    }
    dos_header = (PIMAGE_DOS_HEADER)image_base;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE ||
        dos_header->e_lfanew <= 0 ||
        (DWORD)dos_header->e_lfanew >
            image_size - sizeof(IMAGE_NT_HEADERS32)) {
        return 0;
    }
    nt_headers = (PIMAGE_NT_HEADERS32)(
        image_base + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE ||
        nt_headers->FileHeader.NumberOfSections == 0u ||
        nt_headers->FileHeader.NumberOfSections > 96u) {
        return 0;
    }
    section = IMAGE_FIRST_SECTION(nt_headers);
    for (index = 0u;
         index < nt_headers->FileHeader.NumberOfSections;
         ++index, ++section) {
        DWORD span;
        DWORD section_first;
        DWORD section_last;
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0u ||
            section->VirtualAddress >= image_size) {
            continue;
        }
        span = section->Misc.VirtualSize;
        if (span == 0u) {
            span = section->SizeOfRawData;
        }
        if (span > image_size - section->VirtualAddress) {
            span = image_size - section->VirtualAddress;
        }
        section_first = base + section->VirtualAddress;
        section_last = section_first + span;
        if (section_first >= base && section_last >= section_first &&
            first >= section_first && last <= section_last) {
            return 1;
        }
    }
    return 0;
}

static int startup_is_legacy_call_prefix(BYTE value)
{
    return value == 0x26u || value == 0x2Eu ||
        value == 0x36u || value == 0x3Eu ||
        value == 0x64u || value == 0x65u ||
        value == 0x66u || value == 0x67u ||
        value == 0xF2u || value == 0xF3u;
}

static int startup_ff_call_operand_ends_at(
    const BYTE *cursor, const BYTE *end,
    int address_size_16, int *is_far)
{
    BYTE modrm;
    DWORD mod;
    DWORD reg;
    DWORD rm;
    if (!cursor || !end || cursor >= end || !is_far) {
        return 0;
    }
    modrm = *cursor++;
    mod = modrm >> 6u;
    reg = (modrm >> 3u) & 7u;
    rm = modrm & 7u;
    if (reg != 2u && reg != 3u) {
        return 0;
    }
    *is_far = reg == 3u;
    if (*is_far && mod == 3u) {
        return 0;
    }
    if (address_size_16) {
        if (mod == 0u && rm == 6u) {
            cursor += 2u;
        } else if (mod == 1u) {
            cursor += 1u;
        } else if (mod == 2u) {
            cursor += 2u;
        }
    } else {
        if (mod != 3u && rm == 4u) {
            BYTE sib;
            if (cursor >= end) {
                return 0;
            }
            sib = *cursor++;
            if (mod == 0u && (sib & 7u) == 5u) {
                cursor += 4u;
            }
        } else if (mod == 0u && rm == 5u) {
            cursor += 4u;
        }
        if (mod == 1u) {
            cursor += 1u;
        } else if (mod == 2u) {
            cursor += 4u;
        }
    }
    return cursor == end;
}

static int startup_actual_call_ending_at_is_unsafe(
    DWORD return_address, BYTE *image_base, DWORD image_size,
    const struct GuStartupExactCallContract *call_contract,
    DWORD *call_source_out)
{
    DWORD instruction_length;
    if (!image_base || !call_contract || !call_source_out ||
        return_address < 2u) {
        return 0;
    }
    *call_source_out = 0u;
    for (instruction_length = 2u;
         instruction_length <= 15u &&
         instruction_length <= return_address;
         ++instruction_length) {
        DWORD first = return_address - instruction_length;
        const BYTE *cursor;
        const BYTE *end =
            (const BYTE *)(ULONG_PTR)return_address;
        int operand_size_16 = 0;
        int address_size_16 = 0;
        BYTE opcode;
        DWORD direct_target = 0u;
        int is_direct = 0;
        int is_call = 0;
        if (!startup_main_image_range_is_executable(
                image_base, image_size,
                first, return_address)) {
            continue;
        }
        cursor = (const BYTE *)(ULONG_PTR)first;
        while (cursor < end &&
               startup_is_legacy_call_prefix(*cursor)) {
            if (*cursor == 0x66u) {
                operand_size_16 = 1;
            } else if (*cursor == 0x67u) {
                address_size_16 = 1;
            }
            ++cursor;
        }
        if (cursor >= end) {
            continue;
        }
        opcode = *cursor++;
        if (opcode == 0xE8u) {
            DWORD remaining = (DWORD)(end - cursor);
            if (operand_size_16 && remaining == 2u) {
                WORD displacement = (WORD)(
                    (WORD)cursor[0] | ((WORD)cursor[1] << 8u));
                direct_target = (DWORD)(WORD)(
                    (WORD)return_address + (SHORT)displacement);
                is_call = 1;
                is_direct = 1;
            } else if (!operand_size_16 && remaining == 4u) {
                DWORD displacement =
                    (DWORD)cursor[0] |
                    ((DWORD)cursor[1] << 8u) |
                    ((DWORD)cursor[2] << 16u) |
                    ((DWORD)cursor[3] << 24u);
                direct_target = return_address + displacement;
                is_call = 1;
                is_direct = 1;
            }
        } else if (opcode == 0x9Au) {
            DWORD remaining = (DWORD)(end - cursor);
            if (operand_size_16 && remaining == 4u) {
                direct_target = (DWORD)(
                    (WORD)cursor[0] | ((WORD)cursor[1] << 8u));
                is_call = 1;
                is_direct = 1;
            } else if (!operand_size_16 && remaining == 6u) {
                direct_target =
                    (DWORD)cursor[0] |
                    ((DWORD)cursor[1] << 8u) |
                    ((DWORD)cursor[2] << 16u) |
                    ((DWORD)cursor[3] << 24u);
                is_call = 1;
                is_direct = 1;
            }
        } else if (opcode == 0xFFu) {
            int is_far = 0;
            if (startup_ff_call_operand_ends_at(
                    cursor, end, address_size_16, &is_far)) {
                is_call = 1;
            }
        }
        if (!is_call) {
            continue;
        }
        if (startup_range_overlaps_unsafe_code(
                first, return_address,
                image_base, call_contract) ||
            (is_direct && startup_address_is_in_unsafe_code(
                direct_target, image_base, call_contract))) {
            *call_source_out = first;
            return 1;
        }
    }
    return 0;
}

static int startup_quiescence_stack_has_call_return(
    HANDLE thread, const CONTEXT *context,
    GuNtQueryInformationThreadFn query_thread,
    BYTE *image_base, DWORD image_size,
    const struct GuStartupExactCallContract *call_contract,
    DWORD *unsafe_return_out)
{
    struct GuThreadBasicInformation32 basic;
    BYTE *stack_base;
    BYTE *stack_limit;
    DWORD cursor;
    DWORD end;
    DWORD returned = 0u;
    if (!thread || !context || !query_thread || !image_base ||
        image_size == 0u || !call_contract || !unsafe_return_out) {
        return -1;
    }
    *unsafe_return_out = 0u;
    ZeroMemory(&basic, sizeof(basic));
    if (query_thread(
            thread, 0u, &basic, sizeof(basic), &returned) < 0 ||
        !basic.teb_base ||
        !startup_memory_range_is_readable(
            basic.teb_base + 4u, 8u)) {
        return -1;
    }
    stack_base = *(BYTE **)(void *)(basic.teb_base + 4u);
    stack_limit = *(BYTE **)(void *)(basic.teb_base + 8u);
    if (!stack_base || !stack_limit ||
        stack_limit >= stack_base ||
        (DWORD)(ULONG_PTR)context->Esp <
            (DWORD)(ULONG_PTR)stack_limit ||
        (DWORD)(ULONG_PTR)context->Esp >=
            (DWORD)(ULONG_PTR)stack_base ||
        (DWORD)(stack_base - stack_limit) > 64u * 1024u * 1024u) {
        return -1;
    }
    cursor = (DWORD)context->Esp;
    end = (DWORD)(ULONG_PTR)stack_base;
    if (cursor >= end) {
        return -1;
    }
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information;
        DWORD region_start;
        DWORD region_end;
        DWORD scan_end;
        DWORD base_protection;
        if (VirtualQuery(
                (const void *)(ULONG_PTR)cursor,
                &information, sizeof(information)) !=
                sizeof(information) ||
            information.State != MEM_COMMIT ||
            (information.Protect &
                (PAGE_GUARD | PAGE_NOACCESS)) != 0u) {
            return -1;
        }
        base_protection = information.Protect & 0xFFu;
        if (base_protection != PAGE_READONLY &&
            base_protection != PAGE_READWRITE &&
            base_protection != PAGE_WRITECOPY &&
            base_protection != PAGE_EXECUTE_READ &&
            base_protection != PAGE_EXECUTE_READWRITE &&
            base_protection != PAGE_EXECUTE_WRITECOPY) {
            return -1;
        }
        region_start =
            (DWORD)(ULONG_PTR)information.BaseAddress;
        region_end = region_start +
            (DWORD)information.RegionSize;
        if (region_end < region_start ||
            cursor < region_start || cursor >= region_end) {
            return -1;
        }
        scan_end = region_end < end ? region_end : end;
        if (scan_end <= cursor ||
            scan_end - cursor < 4u) {
            return -1;
        }
        while (cursor <= scan_end - 4u) {
            DWORD value = *(const DWORD *)(ULONG_PTR)cursor;
            DWORD image = (DWORD)(ULONG_PTR)image_base;
            DWORD image_end = image + image_size;
            cursor += 4u;
            if (image_end < image || value < image ||
                value >= image_end) {
                continue;
            }
            if (startup_exact_return_contract_contains_rva(
                    call_contract, value - image)) {
                *unsafe_return_out = value;
                return 1;
            }
            {
                DWORD call_source = 0u;
                if (startup_actual_call_ending_at_is_unsafe(
                        value, image_base, image_size,
                        call_contract, &call_source)) {
                    *unsafe_return_out = value;
                    return 1;
                }
            }
        }
        if (cursor != scan_end && scan_end != end) {
            return -1;
        }
    }
    return 0;
}

static int startup_quiescence_begin(
    struct GuStartupQuiescence *guard,
    BYTE *image_base, const struct BuildProfile *profile,
    const struct GuStartupExactCallContract *call_contract,
    const struct GuStartupPatchSpan *spans, DWORD span_count)
{
    HMODULE ntdll;
    GuNtQueryInformationThreadFn query_thread;
    GuNtGetNextThreadFn get_next_thread;
    DWORD process_id = GetCurrentProcessId();
    DWORD current_thread_id = GetCurrentThreadId();
    DWORD stable = 0u;
    DWORD pass;
    DWORD index;
    DWORD interval_mismatch = 0u;
    DWORD interval_bytes_verified = 0u;
    DWORD interval_relocations_verified = 0u;
    const struct GuStartupGuardInterval *definition_interval;
    const struct GuStartupGuardInterval *plant_interval;

    if (!guard || !image_base || !profile || !call_contract || !spans ||
        span_count == 0u ||
        span_count > GU_STARTUP_MAX_PATCH_SPANS) {
        return 0;
    }
    ZeroMemory(guard, sizeof(*guard));
    g_report.startup_quiescence_attempted = 1u;
    g_report.startup_call_contract_identity_id =
        call_contract->executable_identity_id;
    g_report.startup_call_contract_record_count =
        call_contract->return_count;
    g_report.startup_guard_interval_count =
        call_contract->interval_count;
    definition_interval = startup_guard_interval_by_role(
        call_contract, GU_STARTUP_GUARD_DEFINITION);
    plant_interval = startup_guard_interval_by_role(
        call_contract, GU_STARTUP_GUARD_PLANT_SETTER);
    if (!startup_exact_call_contract_is_structurally_valid(
            call_contract, profile->image_size) ||
        !definition_interval || !plant_interval) {
        g_report.startup_quiescence_failure_kind = 18u;
        return 0;
    }
    g_report.startup_definition_guard_first_rva =
        definition_interval->guard_first_rva;
    g_report.startup_definition_guard_last_rva =
        definition_interval->guard_first_rva +
            definition_interval->guard_length;
    g_report.startup_plant_setter_guard_first_rva =
        plant_interval->guard_first_rva;
    g_report.startup_plant_setter_guard_last_rva =
        plant_interval->guard_first_rva +
            plant_interval->guard_length;
    ntdll = GetModuleHandleA("ntdll.dll");
    query_thread = ntdll ?
        (GuNtQueryInformationThreadFn)GetProcAddress(
            ntdll, "NtQueryInformationThread") : NULL;
    get_next_thread = ntdll ?
        (GuNtGetNextThreadFn)GetProcAddress(
            ntdll, "NtGetNextThread") : NULL;
    if (!query_thread || !get_next_thread) {
        g_report.startup_quiescence_failure_kind = 1u;
        g_report.startup_quiescence_last_error = GetLastError();
        return 0;
    }

    InterlockedExchange(&g_startup_thread_attach_gate_waits, 0);
    InterlockedExchange(
        &g_startup_patch_thread_attach_gate, 1);
    guard->active = 1u;
    for (pass = 0u;
         pass < GU_STARTUP_MAX_SNAPSHOT_PASSES &&
         stable < GU_STARTUP_STABLE_SNAPSHOTS;
         ++pass) {
        HANDLE previous = NULL;
        int previous_adopted = 0;
        DWORD new_threads = 0u;
        ++g_report.startup_quiescence_snapshots;
        for (;;) {
            HANDLE thread = NULL;
            struct GuThreadBasicInformation32 basic;
            DWORD returned = 0u;
            LONG next_status;
            DWORD suspend_result;
            int existing_status;
            next_status = get_next_thread(
                GetCurrentProcess(), previous,
                THREAD_SUSPEND_RESUME |
                    THREAD_GET_CONTEXT |
                    THREAD_QUERY_INFORMATION |
                    SYNCHRONIZE,
                0u, 0u, &thread);
            if (previous && !previous_adopted) {
                CloseHandle(previous);
            }
            previous = NULL;
            previous_adopted = 0;
            if (next_status < 0) {
                if ((DWORD)next_status !=
                        GU_STATUS_NO_MORE_ENTRIES) {
                    g_report.startup_quiescence_failure_kind = 2u;
                    g_report.startup_quiescence_last_error =
                        (DWORD)next_status;
                    startup_quiescence_resume_all(guard);
                    return 0;
                }
                break;
            }
            if (!thread) {
                g_report.startup_quiescence_failure_kind = 3u;
                startup_quiescence_resume_all(guard);
                return 0;
            }
            previous = thread;
            ZeroMemory(&basic, sizeof(basic));
            if (query_thread(
                    thread, 0u, &basic, sizeof(basic),
                    &returned) < 0 ||
                basic.client_process != process_id ||
                basic.client_thread == 0u) {
                g_report.startup_quiescence_failure_kind = 4u;
                startup_quiescence_resume_all(guard);
                if (previous && !previous_adopted) {
                    CloseHandle(previous);
                }
                return 0;
            }
            if (basic.client_thread == current_thread_id) {
                continue;
            }
            existing_status = startup_quiescence_has_live_tid(
                guard, basic.client_thread);
            if (existing_status < 0) {
                g_report.startup_quiescence_failure_kind = 5u;
                g_report.startup_quiescence_last_error =
                    GetLastError();
                if (previous && !previous_adopted) {
                    CloseHandle(previous);
                    previous = NULL;
                }
                startup_quiescence_resume_all(guard);
                return 0;
            }
            if (existing_status > 0) {
                continue;
            }
            if (guard->count >= GU_STARTUP_MAX_THREADS) {
                g_report.startup_quiescence_failure_kind = 6u;
                if (previous && !previous_adopted) {
                    CloseHandle(previous);
                    previous = NULL;
                }
                startup_quiescence_resume_all(guard);
                return 0;
            }
            suspend_result = SuspendThread(thread);
            if (suspend_result == 0xFFFFFFFFu) {
                DWORD wait_result;
                DWORD suspend_error = GetLastError();
                wait_result = WaitForSingleObject(thread, 0u);
                if (wait_result == WAIT_OBJECT_0) {
                    continue;
                }
                g_report.startup_quiescence_failure_kind = 7u;
                g_report.startup_quiescence_last_error =
                    wait_result == WAIT_FAILED ?
                        GetLastError() : suspend_error;
                if (previous && !previous_adopted) {
                    CloseHandle(previous);
                    previous = NULL;
                }
                startup_quiescence_resume_all(guard);
                return 0;
            }
            guard->handles[guard->count] = thread;
            guard->thread_ids[guard->count] =
                basic.client_thread;
            guard->live[guard->count] = 1u;
            previous_adopted = 1;
            ++guard->count;
            ++new_threads;
            ++g_report.startup_quiescence_threads_suspended;
        }
        if (new_threads == 0u) {
            ++stable;
        } else {
            stable = 0u;
        }
    }
    if (stable < GU_STARTUP_STABLE_SNAPSHOTS) {
        g_report.startup_quiescence_failure_kind = 8u;
        startup_quiescence_resume_all(guard);
        return 0;
    }

    if (!startup_guard_intervals_match_preimages(
            call_contract, image_base, profile->image_size,
            &interval_mismatch, &interval_bytes_verified,
            &interval_relocations_verified)) {
        g_report.startup_call_contract_mismatch_address =
            interval_mismatch;
        g_report.startup_quiescence_unsafe_eip =
            interval_mismatch;
        g_report.startup_quiescence_failure_kind = 19u;
        startup_quiescence_resume_all(guard);
        return 0;
    }
    g_report.startup_guard_interval_bytes_verified =
        interval_bytes_verified;
    g_report.startup_guard_interval_relocations_verified =
        interval_relocations_verified;
    g_report.startup_guard_interval_validation_passes = 1u;
    g_report.startup_call_contract_preimages_verified =
        call_contract->return_count;

    for (index = 0u; index < guard->count; ++index) {
        CONTEXT context;
        DWORD span_index;
        DWORD unsafe_stack_return = 0u;
        DWORD wait_result;
        int stack_result;
        DWORD eip;
        if (!guard->handles[index] || !guard->live[index]) {
            continue;
        }
        wait_result = WaitForSingleObject(
            guard->handles[index], 0u);
        if (wait_result == WAIT_OBJECT_0) {
            guard->live[index] = 0u;
            continue;
        }
        if (wait_result != WAIT_TIMEOUT) {
            g_report.startup_quiescence_failure_kind = 5u;
            g_report.startup_quiescence_last_error =
                GetLastError();
            startup_quiescence_resume_all(guard);
            return 0;
        }
        ZeroMemory(&context, sizeof(context));
        context.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(
                guard->handles[index], &context)) {
            g_report.startup_quiescence_failure_kind = 9u;
            g_report.startup_quiescence_last_error = GetLastError();
            startup_quiescence_resume_all(guard);
            return 0;
        }
        eip = context.Eip;
        if (startup_address_is_in_unsafe_code(
                eip, image_base, call_contract)) {
            g_report.startup_quiescence_unsafe_eip = eip;
            g_report.startup_quiescence_failure_kind = 10u;
            startup_quiescence_resume_all(guard);
            return 0;
        }
        for (span_index = 0u;
             span_index < span_count; ++span_index) {
            DWORD start = (DWORD)(ULONG_PTR)spans[span_index].address;
            DWORD end = start + spans[span_index].length;
            if (end < start || (eip >= start && eip < end)) {
                g_report.startup_quiescence_unsafe_eip = eip;
                g_report.startup_quiescence_failure_kind = 11u;
                startup_quiescence_resume_all(guard);
                return 0;
            }
        }
        stack_result = startup_quiescence_stack_has_call_return(
            guard->handles[index], &context, query_thread,
            image_base, profile->image_size, call_contract,
            &unsafe_stack_return);
        if (stack_result != 0) {
            g_report.startup_quiescence_unsafe_stack_return =
                stack_result > 0 ? unsafe_stack_return :
                    0xFFFFFFFFu;
            g_report.startup_quiescence_failure_kind =
                stack_result > 0 ? 12u : 13u;
            startup_quiescence_resume_all(guard);
            return 0;
        }
    }
    return 1;
}

static int startup_quiescence_end(
    struct GuStartupQuiescence *guard)
{
    int ok = startup_quiescence_resume_all(guard);
    if (ok) {
        g_report.startup_quiescence_completed = 1u;
    }
    return ok;
}

static int initialize_minidump_writer(void)
{
    HMODULE pinned = NULL;
    char dbghelp_path[MAX_PATH];
    DWORD system_length;
    if (g_mini_dump_write_dump) {
        return 1;
    }
    g_dbghelp_module = GetModuleHandleA("dbghelp.dll");
    if (!g_dbghelp_module) {
        system_length = GetSystemDirectoryA(dbghelp_path, MAX_PATH);
        if (system_length == 0u || system_length >= MAX_PATH - 12u) {
            return 0;
        }
        if (dbghelp_path[system_length - 1u] != '\\') {
            dbghelp_path[system_length++] = '\\';
        }
        copy_string(
            dbghelp_path + system_length,
            MAX_PATH - system_length, "dbghelp.dll");
        g_dbghelp_module = LoadLibraryA(dbghelp_path);
    }
    if (!g_dbghelp_module) {
        return 0;
    }
    (void)GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_PIN, "dbghelp.dll", &pinned);
    g_mini_dump_write_dump =
        (MiniDumpWriteDumpFunction)GetProcAddress(
            g_dbghelp_module, "MiniDumpWriteDump");
    return g_mini_dump_write_dump != NULL;
}

static const BYTE g_patch8_target_pattern[] = {
    0xC7, 0x41, 0x18, 0x00, 0x02, 0x00, 0x00
};

static const BYTE g_ce_target_pattern[] = {
    0xC7, 0x41, 0x10, 0x00, 0x00, 0xA0, 0x41,
    0xC7, 0x41, 0x14, 0x00, 0x00, 0x34, 0x42,
    0xC7, 0x41, 0x18, 0x00, 0x02, 0x00, 0x00,
    0xC7, 0x41, 0x1C, 0x00, 0x00, 0xA0, 0x42,
    0xC7, 0x41, 0x20, 0x00, 0x20, 0xFD, 0x44
};

static const BYTE g_patch8_distance_init_pattern[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
    0xF3, 0x0F, 0x10, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x59, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0x83, 0xEC, 0x14,
    0x53, 0x56, 0x8B, 0xF1,
    0xF3, 0x0F, 0x11, 0x46, 0x14,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x46, 0x1C,
    0xF3, 0x0F, 0x10, 0x46, 0x14,
    0x0F, 0x28, 0xC8,
    0xF3, 0x0F, 0x59, 0xC8,
    0x57,
    0xF3, 0x0F, 0x11, 0x4E, 0x20
};

static const BYTE g_patch8_distance_update_pattern[] = {
    0xF3, 0x0F, 0x10, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x59, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x41, 0x14,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x41, 0x1C,
    0xF3, 0x0F, 0x10, 0x41, 0x14,
    0x0F, 0x28, 0xC8,
    0xF3, 0x0F, 0x59, 0xC8,
    0xF3, 0x0F, 0x11, 0x49, 0x20,
    0xC3
};

static const BYTE g_ce_distance_init_pattern[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0,
    0xF3, 0x0F, 0x10, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x59, 0x05, 0, 0, 0, 0,
    0x83, 0xEC, 0x14,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0x53, 0x56, 0x8B, 0xF1, 0x57,
    0xF3, 0x0F, 0x11, 0x46, 0x14,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x46, 0x1C,
    0xF3, 0x0F, 0x10, 0x46, 0x14,
    0xF3, 0x0F, 0x59, 0xC0,
    0xF3, 0x0F, 0x11, 0x46, 0x20
};

static const BYTE g_ce_distance_update_pattern[] = {
    0xF3, 0x0F, 0x10, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x59, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x41, 0x14,
    0xF3, 0x0F, 0x58, 0x05, 0, 0, 0, 0,
    0xF3, 0x0F, 0x11, 0x41, 0x1C,
    0xF3, 0x0F, 0x10, 0x41, 0x14,
    0xF3, 0x0F, 0x59, 0xC0,
    0xF3, 0x0F, 0x11, 0x41, 0x20,
    0xC3
};

static DWORD read_u32(const BYTE *address)
{
    return ((DWORD)address[0]) |
           ((DWORD)address[1] << 8) |
           ((DWORD)address[2] << 16) |
           ((DWORD)address[3] << 24);
}

static WORD read_u16(const BYTE *address)
{
    return (WORD)(((WORD)address[0]) | ((WORD)address[1] << 8));
}

static float read_float(const BYTE *address)
{
    union {
        DWORD bits;
        float value;
    } converted;
    converted.bits = read_u32(address);
    return converted.value;
}

/*
 * Exact stock positive output-model keys. These are generated with GTA IV's
 * registry hash algorithm from active stock procedural.dat vegetation model
 * identities plus the exact stock object.dat grassplant/grasshouse records.
 * The PLANT callback compares only the proven descriptor+0x18 output key.
 * Unknown keys stay at 1x; no substring or model-global classification is
 * performed in the hot path.
 */
struct PlantPositiveOutputKey {
    DWORD key;
    DWORD class_bit;
};

static const struct PlantPositiveOutputKey g_plant_positive_output_keys[] = {
    {0x0274A8A2u, DENSITY_CLASS_MASK_VEGETATION},
    {0x02AA451Au, DENSITY_CLASS_MASK_VEGETATION},
    {0x078F4E26u, DENSITY_CLASS_MASK_VEGETATION},
    {0x07ACDE81u, DENSITY_CLASS_MASK_VEGETATION},
    {0x1BFE839Cu, DENSITY_CLASS_MASK_VEGETATION},
    {0x22E5469Du, DENSITY_CLASS_MASK_VEGETATION},
    {0x235CEB3Au, DENSITY_CLASS_MASK_VEGETATION},
    {0x2AA7F90Eu, DENSITY_CLASS_MASK_VEGETATION},
    {0x2D229BA0u, DENSITY_CLASS_MASK_VEGETATION},
    {0x4DF49CBFu, DENSITY_CLASS_MASK_VEGETATION},
    {0x4E611D94u, DENSITY_CLASS_MASK_VEGETATION},
    {0x4F06EBECu, DENSITY_CLASS_MASK_VEGETATION},
    {0x65C36EBAu, DENSITY_CLASS_MASK_VEGETATION},
    {0x6C8809FBu, DENSITY_CLASS_MASK_GRASS},
    {0x706B2A4Cu, DENSITY_CLASS_MASK_VEGETATION},
    {0x75B6585Du, DENSITY_CLASS_MASK_GRASS},
    {0x807022C4u, DENSITY_CLASS_MASK_VEGETATION},
    {0x85824AC8u, DENSITY_CLASS_MASK_VEGETATION},
    {0x9BD55927u, DENSITY_CLASS_MASK_VEGETATION},
    {0xA45F6D41u, DENSITY_CLASS_MASK_VEGETATION},
    {0xAA24140Fu, DENSITY_CLASS_MASK_VEGETATION},
    {0xB0D86281u, DENSITY_CLASS_MASK_VEGETATION},
    {0xB82894D3u, DENSITY_CLASS_MASK_VEGETATION},
    {0xB86DB09Eu, DENSITY_CLASS_MASK_VEGETATION},
    {0xB89794EFu, DENSITY_CLASS_MASK_VEGETATION},
    {0xBDDD3B81u, DENSITY_CLASS_MASK_VEGETATION},
    {0xC6513062u, DENSITY_CLASS_MASK_VEGETATION},
    {0xC6703094u, DENSITY_CLASS_MASK_VEGETATION},
    {0xC99337A8u, DENSITY_CLASS_MASK_VEGETATION},
    {0xCD052480u, DENSITY_CLASS_MASK_VEGETATION},
    {0xCE869DE1u, DENSITY_CLASS_MASK_VEGETATION},
    {0xCF94DEF0u, DENSITY_CLASS_MASK_VEGETATION},
    {0xD61B4FF6u, DENSITY_CLASS_MASK_VEGETATION},
    {0xDC45395Eu, DENSITY_CLASS_MASK_VEGETATION},
    {0xDC7BF734u, DENSITY_CLASS_MASK_VEGETATION},
    {0xE1325AD1u, DENSITY_CLASS_MASK_VEGETATION},
    {0xE13F0244u, DENSITY_CLASS_MASK_VEGETATION},
    {0xED6AFF57u, DENSITY_CLASS_MASK_VEGETATION},
    {0xF0EBA19Du, DENSITY_CLASS_MASK_VEGETATION},
    {0xF5848F92u, DENSITY_CLASS_MASK_VEGETATION},
    {0xFF6EA29Cu, DENSITY_CLASS_MASK_VEGETATION}
};

static DWORD plant_output_key_class_bit(DWORD key)
{
    DWORD first = 0u;
    DWORD last = (DWORD)(sizeof(g_plant_positive_output_keys) /
                         sizeof(g_plant_positive_output_keys[0]));
    while (first < last) {
        DWORD middle = first + (last - first) / 2u;
        DWORD candidate = g_plant_positive_output_keys[middle].key;
        if (candidate == key) {
            return g_plant_positive_output_keys[middle].class_bit;
        }
        if (candidate < key) {
            first = middle + 1u;
        } else {
            last = middle;
        }
    }
    return 0u;
}

static DWORD __stdcall plant_visible_density_selector(
    const struct GuPlantSelectionContext *context,
    void *user_context)
{
    DWORD neutral_bits = 0x3F800000u;
    DWORD multiplier_bits;
    float multiplier;
    if (!context || !user_context || context->geometry_type != 0x0Cu ||
        !context->source_entity || !context->source_model_info ||
        !context->descriptor || context->source_model_index >= 31000u ||
        context->source_model_hash == 0u ||
        (plant_output_key_class_bit(
             context->output_model_registry_key) &
         g_density_class_mask) == 0u) {
        InterlockedIncrement(&g_plant_selector_neutral_calls);
        return neutral_bits;
    }
    multiplier_bits = *(const volatile DWORD *)user_context;
    multiplier = *(const float *)(const void *)&multiplier_bits;
    if ((multiplier_bits & 0x7F800000u) == 0x7F800000u ||
        multiplier < MIN_CORRECTED_PLANT_DENSITY_MULTIPLIER ||
        multiplier > MAX_CORRECTED_PLANT_DENSITY_MULTIPLIER) {
        InterlockedIncrement(&g_plant_selector_neutral_calls);
        return neutral_bits;
    }
    InterlockedExchange(
        &g_plant_selector_last_output_key,
        (LONG)context->output_model_registry_key);
    InterlockedExchange(
        &g_plant_selector_last_source_hash,
        (LONG)context->source_model_hash);
    InterlockedExchange(
        &g_plant_selector_last_submesh,
        (LONG)context->submesh_index);
    InterlockedIncrement(&g_plant_selector_positive_calls);
    return multiplier_bits;
}

#ifdef GTAIVPOOL_TEST_EXPORTS
static void write_float(BYTE *address, float value)
{
    union {
        DWORD bits;
        float value;
    } converted;
    converted.value = value;
    address[0] = (BYTE)(converted.bits & 0xFFu);
    address[1] = (BYTE)((converted.bits >> 8) & 0xFFu);
    address[2] = (BYTE)((converted.bits >> 16) & 0xFFu);
    address[3] = (BYTE)((converted.bits >> 24) & 0xFFu);
}

#endif

static void write_float_atomic(BYTE *address, float value)
{
    union {
        DWORD bits;
        float value;
    } converted;
    converted.value = value;
    InterlockedExchange((volatile LONG *)address, (LONG)converted.bits);
}

static int positive_finite_float(float value)
{
    union {
        DWORD bits;
        float value;
    } converted;
    converted.value = value;
    return value > 0.0f &&
        (converted.bits & 0x7F800000u) != 0x7F800000u;
}

static int nonnegative_finite_float(float value)
{
    union {
        DWORD bits;
        float value;
    } converted;
    converted.value = value;
    return value >= 0.0f &&
        (converted.bits & 0x7F800000u) != 0x7F800000u;
}

static float positive_square_root(float value)
{
    float estimate = value >= 1.0f ? value : 1.0f;
    DWORD iteration;
    for (iteration = 0; iteration < 12u; ++iteration) {
        estimate = 0.5f * (estimate + value / estimate);
    }
    return estimate;
}

static int parse_u32_strict(const char *text, DWORD *value)
{
    DWORD parsed = 0;
    DWORD index = 0;

    if (!text || !value || text[0] == '\0') {
        return 0;
    }

    while (text[index] != '\0') {
        DWORD digit;
        if (text[index] < '0' || text[index] > '9') {
            return 0;
        }
        digit = (DWORD)(text[index] - '0');
        if (parsed > (0xFFFFFFFFu - digit) / 10u) {
            return 0;
        }
        parsed = parsed * 10u + digit;
        ++index;
    }

    *value = parsed;
    return 1;
}

static int parse_decimal_float_strict(
    const char *text, float *value)
{
    DWORD whole = 0u;
    DWORD fraction = 0u;
    DWORD divisor = 1u;
    DWORD index = 0u;
    DWORD fraction_digits = 0u;
    int saw_decimal = 0;
    int saw_digit = 0;
    float parsed;

    if (!text || !value || text[0] == '\0') {
        return 0;
    }
    while (text[index] != '\0') {
        char character = text[index++];
        if (character == '.') {
            if (saw_decimal) {
                return 0;
            }
            saw_decimal = 1;
            continue;
        }
        if (character < '0' || character > '9') {
            return 0;
        }
        saw_digit = 1;
        if (!saw_decimal) {
            DWORD digit = (DWORD)(character - '0');
            if (whole > (1000000u - digit) / 10u) {
                return 0;
            }
            whole = whole * 10u + digit;
        } else {
            if (fraction_digits >= 7u) {
                return 0;
            }
            fraction =
                fraction * 10u + (DWORD)(character - '0');
            divisor *= 10u;
            ++fraction_digits;
        }
    }
    if (!saw_digit || (saw_decimal && fraction_digits == 0u)) {
        return 0;
    }
    parsed = (float)whole + (float)fraction / (float)divisor;
    if (!positive_finite_float(parsed)) {
        return 0;
    }
    *value = parsed;
    return 1;
}

static void copy_config_text(
    char *destination, DWORD capacity, const char *source)
{
    DWORD index = 0u;
    if (!destination || capacity == 0u) {
        return;
    }
    while (source && source[index] != '\0' && index + 1u < capacity) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void set_config_error(
    const char *key, const char *value, DWORD kind,
    DWORD minimum, DWORD maximum)
{
    if (g_config_error_kind != CONFIG_ERROR_NONE) {
        return;
    }
    copy_config_text(
        g_config_error_key, sizeof(g_config_error_key), key);
    copy_config_text(
        g_config_error_value, sizeof(g_config_error_value), value);
    g_config_error_kind = kind;
    g_config_error_min = minimum;
    g_config_error_max = maximum;
}

static const char *known_config_float_text(float value)
{
    DWORD bits = read_u32((const BYTE *)&value);
    switch (bits) {
    case 0x3DCCCCCDu:
        return "0.1";
    case 0x3F000000u:
        return "0.5";
    case 0x3F800000u:
        return "1.0";
    case 0x3FC00000u:
        return "1.5";
    case 0x40000000u:
        return "2.0";
    case 0x40200000u:
        return "2.5";
    case 0x40400000u:
        return "3.0";
    case 0x40600000u:
        return "3.5";
    case 0x40800000u:
        return "4.0";
    default:
        return "(unsupported bound)";
    }
}

static void set_config_float_error(
    const char *key, const char *value, DWORD kind,
    float minimum, float maximum)
{
    if (g_config_error_kind != CONFIG_ERROR_NONE) {
        return;
    }
    copy_config_text(
        g_config_error_key, sizeof(g_config_error_key), key);
    copy_config_text(
        g_config_error_value, sizeof(g_config_error_value), value);
    copy_config_text(
        g_config_error_min_text, sizeof(g_config_error_min_text),
        known_config_float_text(minimum));
    copy_config_text(
        g_config_error_max_text, sizeof(g_config_error_max_text),
        known_config_float_text(maximum));
    g_config_error_kind = kind;
    g_config_error_float_range = 1u;
}

static int read_ini_u32_strict(
    const char *key, DWORD minimum, DWORD maximum, DWORD *value)
{
    char text[64];
    DWORD parsed;
    DWORD length = GetPrivateProfileStringA(
        "ProceduralPool", key, NULL,
        text, sizeof(text), g_ini_path);

    if (length == 0u) {
        set_config_error(
            key, "(missing)", CONFIG_ERROR_MISSING,
            minimum, maximum);
        return 0;
    }
    if (length >= sizeof(text) - 1u ||
        !parse_u32_strict(text, &parsed)) {
        set_config_error(
            key, text, CONFIG_ERROR_NOT_WHOLE_NUMBER,
            minimum, maximum);
        return 0;
    }
    if (parsed < minimum || parsed > maximum) {
        set_config_error(
            key, text, CONFIG_ERROR_OUT_OF_RANGE,
            minimum, maximum);
        return 0;
    }
    *value = parsed;
    return 1;
}

static int read_ini_u32_strict_default(
    const char *key, const char *default_text,
    DWORD minimum, DWORD maximum, DWORD *value)
{
    char text[64];
    DWORD parsed;
    DWORD length = GetPrivateProfileStringA(
        "ProceduralPool", key, default_text,
        text, sizeof(text), g_ini_path);

    if (length == 0u || length >= sizeof(text) - 1u ||
        !parse_u32_strict(text, &parsed)) {
        set_config_error(
            key, length == 0u ? "(missing)" : text,
            length == 0u ? CONFIG_ERROR_MISSING :
                CONFIG_ERROR_NOT_WHOLE_NUMBER,
            minimum, maximum);
        return 0;
    }
    if (parsed < minimum || parsed > maximum) {
        set_config_error(
            key, text, CONFIG_ERROR_OUT_OF_RANGE,
            minimum, maximum);
        return 0;
    }
    *value = parsed;
    return 1;
}

static int read_ini_float_strict_default(
    const char *key, const char *default_text,
    float minimum, float maximum, float *value)
{
    char text[64];
    float parsed;
    DWORD length = GetPrivateProfileStringA(
        "ProceduralPool", key, default_text,
        text, sizeof(text), g_ini_path);

    if (length == 0u || length >= sizeof(text) - 1u ||
        !parse_decimal_float_strict(text, &parsed)) {
        set_config_float_error(
            key, length == 0u ? "(missing)" : text,
            length == 0u ? CONFIG_ERROR_MISSING :
                CONFIG_ERROR_NOT_DECIMAL_FLOAT,
            minimum, maximum);
        return 0;
    }
    if (parsed < minimum || parsed > maximum) {
        set_config_float_error(
            key, text, CONFIG_ERROR_OUT_OF_RANGE,
            minimum, maximum);
        return 0;
    }
    *value = parsed;
    return 1;
}

static int read_ini_rebased_density(
    const char *key, float *configured, float *effective)
{
    float value;
    if (!configured || !effective ||
        !read_ini_float_strict_default(key, "1.0",
            MIN_PUBLIC_DENSITY_MULTIPLIER,
            MAX_PUBLIC_DENSITY_MULTIPLIER, &value)) {
        return 0;
    }
    *configured = value;
    *effective = value * PUBLIC_DENSITY_BASELINE_SCALE;
    return 1;
}

static int read_ini_u32_or_auto_default(
    const char *key, DWORD minimum, DWORD maximum,
    DWORD *value, DWORD *automatic)
{
    char text[64];
    DWORD parsed;
    DWORD length = GetPrivateProfileStringA(
        "ProceduralPool", key, "auto",
        text, sizeof(text), g_ini_path);

    if (length == 0u || length >= sizeof(text) - 1u) {
        set_config_error(
            key, length == 0u ? "(missing)" : text,
            length == 0u ? CONFIG_ERROR_MISSING :
                CONFIG_ERROR_NOT_WHOLE_NUMBER,
            minimum, maximum);
        return 0;
    }
    if (lstrcmpiA(text, "auto") == 0) {
        *automatic = 1u;
        return 1;
    }
    if (!parse_u32_strict(text, &parsed)) {
        set_config_error(
            key, text, CONFIG_ERROR_NOT_WHOLE_NUMBER,
            minimum, maximum);
        return 0;
    }
    if (parsed < minimum || parsed > maximum) {
        set_config_error(
            key, text, CONFIG_ERROR_OUT_OF_RANGE,
            minimum, maximum);
        return 0;
    }
    *value = parsed;
    *automatic = 0u;
    return 1;
}

/*
 * Treat only the exact IEEE-754 single-precision value 1.0f as neutral.
 * GRASS-BLD-0050 permits combined behavior axes; this raw-bit counter is
 * retained for unambiguous logging and the combined-axis acceptance matrix.
 */
static DWORD behavior_axis_non_neutral_count(
    DWORD distance_multiplier_bits,
    DWORD plant_density_multiplier_bits,
    DWORD procobj_density_multiplier_bits)
{
    const DWORD neutral_bits = 0x3F800000u;
    DWORD non_neutral_count = 0u;
    if (distance_multiplier_bits != neutral_bits) {
        ++non_neutral_count;
    }
    if (plant_density_multiplier_bits != neutral_bits) {
        ++non_neutral_count;
    }
    if (procobj_density_multiplier_bits != neutral_bits) {
        ++non_neutral_count;
    }
    return non_neutral_count;
}

static int resolve_automatic_pool_profile(
    float distance_multiplier,
    float plant_density_multiplier,
    float procobj_density_multiplier,
    DWORD *surface_capacity,
    DWORD *rendered_capacity,
    DWORD *provider_capacity)
{
    if (!surface_capacity || !rendered_capacity ||
        !provider_capacity ||
        distance_multiplier < MIN_CORRECTED_DISTANCE_MULTIPLIER ||
        distance_multiplier > MAX_CORRECTED_DISTANCE_MULTIPLIER ||
        plant_density_multiplier < MIN_CORRECTED_PLANT_DENSITY_MULTIPLIER ||
        plant_density_multiplier > MAX_CORRECTED_PLANT_DENSITY_MULTIPLIER ||
        procobj_density_multiplier < MIN_PROCOBJ_DENSITY_MULTIPLIER ||
        procobj_density_multiplier > MAX_PROCOBJ_DENSITY_MULTIPLIER) {
        return 0;
    }

    /* Automatic selection is deliberately bounded.  Distance/density never
     * promotes the runtime into the former 32K/40K allocation regime. */
    *surface_capacity = DEFAULT_CAPACITY;
    *rendered_capacity = DEFAULT_RENDERED_OBJECT_CAPACITY;
    *provider_capacity = DEFAULT_PROVIDER_CAPACITY;
    return 1;
}

static DWORD float_to_bits(float value)
{
    return read_u32((const BYTE *)&value);
}

static void write_u32(BYTE *address, DWORD value)
{
    address[0] = (BYTE)(value & 0xFFu);
    address[1] = (BYTE)((value >> 8) & 0xFFu);
    address[2] = (BYTE)((value >> 16) & 0xFFu);
    address[3] = (BYTE)((value >> 24) & 0xFFu);
}

static void write_u16(BYTE *address, WORD value)
{
    address[0] = (BYTE)(value & 0xFFu);
    address[1] = (BYTE)((value >> 8) & 0xFFu);
}

static void copy_bytes(BYTE *destination, const BYTE *source, DWORD length)
{
    DWORD index;
    for (index = 0; index < length; ++index) {
        destination[index] = source[index];
    }
}

static int bytes_are_equal(
    const BYTE *left, const BYTE *right, DWORD length)
{
    DWORD index;
    for (index = 0; index < length; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

/*
 * GTAIV.exe calls 0x00C09D10 as a thiscall-style allocator with the
 * procedural-object generator manager in ECX. Tiny C Compiler does not
 * implement thiscall on x86, so this short, audited assembly bridge preserves
 * ECX, invokes a normal C helper, and returns its EAX value to the game.
 *
 * The second bridge calls GTA IV's original intrusive-list pop routine with
 * its list object in ECX. It tail-jumps so the game's RET returns directly to
 * the C caller, which then performs normal cdecl stack cleanup.
 */
void generator_pop_trampoline(void);
void *generator_builtin_pop_call(void *list);
void *generator_pop_hook_c(BYTE *manager);

#if defined(_MSC_VER) && !defined(__clang__)
__declspec(naked) void generator_pop_trampoline(void)
{
    __asm {
        push ecx
        call generator_pop_hook_c
        add esp, 4
        ret
    }
}

__declspec(naked) void *generator_builtin_pop_call(void *list)
{
    __asm {
        mov ecx, [esp + 4]
        mov eax, g_generator_builtin_pop
        jmp eax
    }
}
#elif defined(__TINYC__)
__asm__(
    ".text\n"
    ".globl generator_pop_trampoline\n"
    "generator_pop_trampoline:\n"
    "push %ecx\n"
    "call generator_pop_hook_c\n"
    "add $4,%esp\n"
    "ret\n"

    ".globl generator_builtin_pop_call\n"
    "generator_builtin_pop_call:\n"
    "mov 4(%esp),%ecx\n"
    "mov g_generator_builtin_pop,%eax\n"
    "jmp *%eax\n"
);
#else
#error "This source requires either MSVC x86 inline assembly or TinyCC for the trampoline layer."
#endif

#ifdef GTAIVPOOL_TEST_NO_FATAL
void *generator_fixture_pop(void *list);
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(naked) void *generator_fixture_pop(void *list)
{
    __asm {
        mov eax, [ecx + 4]
        test eax, eax
        jz fixture_null
        cmp eax, [ecx + 8]
        jne fixture_middle
        dec dword ptr [ecx]
        mov dword ptr [ecx + 4], 0
        mov dword ptr [ecx + 8], 0
        ret
fixture_middle:
        mov edx, [eax]
        mov [ecx + 4], edx
        test edx, edx
        jz fixture_after_next
        mov dword ptr [edx + 4], 0
fixture_after_next:
        dec dword ptr [ecx]
        ret
fixture_null:
        xor eax, eax
        ret
    }
}
#elif defined(__TINYC__)
__asm__(
    ".text\n"
    ".globl generator_fixture_pop\n"
    "generator_fixture_pop:\n"
    "mov 4(%ecx),%eax\n"
    "test %eax,%eax\n"
    "jz 2f\n"
    "cmp 8(%ecx),%eax\n"
    "jne 1f\n"
    "decl (%ecx)\n"
    "movl $0,4(%ecx)\n"
    "movl $0,8(%ecx)\n"
    "ret\n"
    "1:\n"
    "mov (%eax),%edx\n"
    "mov %edx,4(%ecx)\n"
    "test %edx,%edx\n"
    "jz 3f\n"
    "movl $0,4(%edx)\n"
    "3:\n"
    "decl (%ecx)\n"
    "ret\n"
    "2:\n"
    "xor %eax,%eax\n"
    "ret\n"
);
#endif
#endif

void *generator_pop_hook_c(BYTE *manager)
{
    void *record;
    LONG issued;

    if (!manager || manager != g_generator_manager ||
        !g_generator_builtin_pop) {
        InterlockedExchange(
            &g_generator_extra_exhaustion_observed, 1);
        return NULL;
    }

    record = generator_builtin_pop_call(
        manager + GENERATOR_FREE_LIST_OFFSET);
    if (record) {
        return record;
    }

    InterlockedIncrement(&g_generator_fallback_calls);
    for (;;) {
        issued = InterlockedCompareExchange(
            &g_generator_extra_issued, 0, 0);
        if (issued < 0 ||
            (DWORD)issued >= g_generator_extra_record_count) {
            InterlockedExchange(
                &g_generator_extra_exhaustion_observed, 1);
            return NULL;
        }
        if (InterlockedCompareExchange(
                &g_generator_extra_issued,
                issued + 1, issued) == issued) {
            break;
        }
    }

    record = g_generator_extra_records +
        (DWORD)issued * GENERATOR_RECORD_SIZE;
    /*
     * VirtualAlloc already zeroes the block. Clear a record again on first
     * issue so this hook has exactly the initialized field state of each
     * built-in 0x18-byte record at 0x00C09D30.
     */
    ZeroMemory(record, GENERATOR_RECORD_SIZE);
    return record;
}

static void update_long_high_watermark(
    volatile LONG *high_watermark, DWORD value)
{
    LONG observed;

    if (value > 0x7FFFFFFFu) {
        value = 0x7FFFFFFFu;
    }
    for (;;) {
        observed = InterlockedCompareExchange(
            high_watermark, 0, 0);
        if ((DWORD)observed >= value) {
            return;
        }
        if (InterlockedCompareExchange(
                high_watermark, (LONG)value,
                observed) == observed) {
            return;
        }
    }
}

static int strings_are_equal(const char *left, const char *right)
{
    if (!left || !right) {
        return 0;
    }
    while (*left && *right) {
        if (*left != *right) {
            return 0;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static void GU_FASTCALL provider_post_rebuild_hook(
    BYTE *manager, DWORD ignored_edx);

static int initialize_provider_rebuild_stub(void)
{
    static const BYTE replay_epilogue[] = {
        0x5F, 0x5E, 0xB0, 0x01, 0x5B,
        0x8B, 0xE5, 0x5D, 0xC3
    };
    BYTE *stub;
    DWORD cursor = 0u;
    DWORD relative;
    DWORD old_protection = 0u;

    if (g_provider_rebuild_stub) {
        return 0;
    }
    stub = (BYTE *)VirtualAlloc(
        NULL, 64u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!stub) {
        return 0;
    }
    stub[cursor++] = 0x9Cu; /* pushfd */
    stub[cursor++] = 0x50u; /* push eax */
    stub[cursor++] = 0x51u; /* push ecx */
    stub[cursor++] = 0x52u; /* push edx */
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = 0xCEu; /* mov ecx,esi */
    stub[cursor++] = 0x31u;
    stub[cursor++] = 0xD2u; /* xor edx,edx */
    stub[cursor++] = 0xE8u; /* call rel32 */
    relative = (DWORD)(const void *)provider_post_rebuild_hook -
        (DWORD)(stub + cursor + 4u);
    write_u32(stub + cursor, relative);
    cursor += 4u;
    stub[cursor++] = 0x5Au; /* pop edx */
    stub[cursor++] = 0x59u; /* pop ecx */
    stub[cursor++] = 0x58u; /* pop eax */
    stub[cursor++] = 0x9Du; /* popfd */
    copy_bytes(
        stub + cursor, replay_epilogue,
        sizeof(replay_epilogue));
    cursor += sizeof(replay_epilogue);
    if (!VirtualProtect(
            stub, 64u, PAGE_EXECUTE_READ,
            &old_protection) ||
        !FlushInstructionCache(
            GetCurrentProcess(), stub, cursor)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    g_provider_rebuild_stub = stub;
    return 1;
}

static void release_provider_rebuild_stub(void)
{
    if (g_provider_rebuild_stub) {
        VirtualFree(
            g_provider_rebuild_stub, 0u, MEM_RELEASE);
        g_provider_rebuild_stub = NULL;
    }
}

static void configure_provider_patch(
    struct ProviderPatch *patch, BYTE *address,
    const BYTE *expected, DWORD length)
{
    ZeroMemory(patch, sizeof(*patch));
    patch->address = address;
    patch->length = length;
    copy_bytes(patch->original, expected, length);
    copy_bytes(patch->replacement, expected, length);
}

static void configure_provider_patches_ce(
    BYTE *image_base, struct ProviderPatch *patches)
{
    BYTE expected_0[] = {0xB9, 0, 0, 0, 0};
    BYTE expected_1[] = {0xB9, 0, 0, 0, 0};
    BYTE expected_2[] = {0, 0, 0, 0};
    BYTE expected_3[] = {0, 0, 0, 0};
    static const BYTE expected_4[] = {
        0x8D, 0x04, 0x40, 0x53, 0xC1, 0xE0, 0x05,
        0x8D, 0x77, 0xD0, 0x03, 0xF0,
        0x55, 0x56, 0x8B, 0xCF
    };
    static const BYTE expected_5[] = {
        0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x05,
        0x03, 0xF0, 0x8D, 0x4E, 0xD0
    };
    static const BYTE expected_6[] =
        {0x8D, 0x46, 0xD0};
    static const BYTE expected_7[] = {
        0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x05,
        0x83, 0xC0, 0xD0, 0x03, 0xC1
    };
    static const BYTE expected_8[] = {
        0x0F, 0xB7, 0xC6, 0x8D, 0x4F, 0xD0,
        0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x05,
        0x03, 0xC8
    };
    static const BYTE expected_9[] = {
        0x8D, 0x04, 0x76, 0xC1, 0xE0, 0x05,
        0x8D, 0x4F, 0xD0, 0x03, 0xC8
    };
    DWORD shadow_base = (DWORD)g_provider_storage;
    DWORD record_index_base = (DWORD)g_provider_records -
        PROCEDURAL_RECORD_SIZE;
    DWORD next_index_base = (DWORD)g_provider_records -
        (PROCEDURAL_RECORD_SIZE - SOURCE_NEXT_OFFSET);

    write_u32(
        expected_0 + 1u,
        (DWORD)(image_base + PROVIDER_MANAGER_POINTER_RVA));
    write_u32(
        expected_1 + 1u,
        (DWORD)(image_base + PROVIDER_MANAGER_POINTER_RVA));
    write_u32(
        expected_2,
        (DWORD)(image_base + PROVIDER_NEXT_INDEX_POINTER_RVA));
    write_u32(
        expected_3,
        (DWORD)(image_base + PROVIDER_RECORD_INDEX_POINTER_RVA));

    configure_provider_patch(
        &patches[0], image_base + g_build_profile->provider_sites[0],
        expected_0, sizeof(expected_0));
    configure_provider_patch(
        &patches[1], image_base + g_build_profile->provider_sites[1],
        expected_1, sizeof(expected_1));
    configure_provider_patch(
        &patches[2], image_base + g_build_profile->provider_sites[2],
        expected_2, sizeof(expected_2));
    configure_provider_patch(
        &patches[3], image_base + g_build_profile->provider_sites[3],
        expected_3, sizeof(expected_3));
    configure_provider_patch(
        &patches[4], image_base + g_build_profile->provider_sites[4],
        expected_4, sizeof(expected_4));
    configure_provider_patch(
        &patches[5], image_base + g_build_profile->provider_sites[5],
        expected_5, sizeof(expected_5));
    configure_provider_patch(
        &patches[6], image_base + g_build_profile->provider_sites[6],
        expected_6, sizeof(expected_6));
    configure_provider_patch(
        &patches[7], image_base + g_build_profile->provider_sites[7],
        expected_7, sizeof(expected_7));
    configure_provider_patch(
        &patches[8], image_base + g_build_profile->provider_sites[8],
        expected_8, sizeof(expected_8));
    configure_provider_patch(
        &patches[9], image_base + g_build_profile->provider_sites[9],
        expected_9, sizeof(expected_9));

    write_u32(patches[0].replacement + 1u, shadow_base);
    write_u32(patches[1].replacement + 1u, shadow_base);
    write_u32(patches[2].replacement, next_index_base);
    write_u32(patches[3].replacement, record_index_base);

    patches[4].replacement[0] = 0x6B;
    patches[4].replacement[1] = 0xC0;
    patches[4].replacement[2] = 0x60;
    patches[4].replacement[3] = 0x05;
    write_u32(patches[4].replacement + 4u, record_index_base);
    patches[4].replacement[8] = 0x53;
    patches[4].replacement[9] = 0x55;
    patches[4].replacement[10] = 0x8B;
    patches[4].replacement[11] = 0xF0;
    patches[4].replacement[12] = 0x56;
    patches[4].replacement[13] = 0x8B;
    patches[4].replacement[14] = 0xCF;
    patches[4].replacement[15] = 0x90;

    patches[5].replacement[0] = 0x6B;
    patches[5].replacement[1] = 0xC0;
    patches[5].replacement[2] = 0x60;
    patches[5].replacement[3] = 0x05;
    write_u32(patches[5].replacement + 4u, record_index_base);
    patches[5].replacement[8] = 0x8B;
    patches[5].replacement[9] = 0xC8;
    patches[5].replacement[10] = 0x90;

    patches[6].replacement[0] = 0x90;
    patches[6].replacement[1] = 0x90;
    patches[6].replacement[2] = 0x90;

    patches[7].replacement[0] = 0x6B;
    patches[7].replacement[1] = 0xC0;
    patches[7].replacement[2] = 0x60;
    patches[7].replacement[3] = 0x05;
    write_u32(patches[7].replacement + 4u, record_index_base);
    patches[7].replacement[8] = 0x90;
    patches[7].replacement[9] = 0x90;
    patches[7].replacement[10] = 0x90;

    patches[8].replacement[0] = 0x0F;
    patches[8].replacement[1] = 0xB7;
    patches[8].replacement[2] = 0xC6;
    patches[8].replacement[3] = 0x6B;
    patches[8].replacement[4] = 0xC0;
    patches[8].replacement[5] = 0x60;
    patches[8].replacement[6] = 0x05;
    write_u32(patches[8].replacement + 7u, record_index_base);
    patches[8].replacement[11] = 0x8B;
    patches[8].replacement[12] = 0xC8;
    patches[8].replacement[13] = 0x90;

    patches[9].replacement[0] = 0x6B;
    patches[9].replacement[1] = 0xC6;
    patches[9].replacement[2] = 0x60;
    patches[9].replacement[3] = 0x05;
    write_u32(patches[9].replacement + 4u, record_index_base);
    patches[9].replacement[8] = 0x8B;
    patches[9].replacement[9] = 0xC8;
    patches[9].replacement[10] = 0x90;
}

static void configure_provider_patches_patch8(
    BYTE *image_base, struct ProviderPatch *patches)
{
    static const BYTE expected_entity_lookup[] = {
        0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x05,
        0x39, 0x54, 0x08, 0x14, 0x8D, 0x44, 0x08, 0xD0
    };
    BYTE expected_manager_0[] = {0xB9, 0, 0, 0, 0};
    BYTE expected_manager_1[] = {0xB9, 0, 0, 0, 0};
    BYTE expected_next[] = {0, 0, 0, 0};
    BYTE expected_record[] = {0, 0, 0, 0};
    static const BYTE expected_record_lookup[] = {
        0x8D, 0x04, 0x40, 0xC1, 0xE0, 0x05,
        0x8D, 0x74, 0x08, 0xD0
    };
    static const BYTE expected_traversal_0[] = {
        0x0F, 0xB7, 0xC7, 0x8D, 0x04, 0x40,
        0xC1, 0xE0, 0x05, 0x0F, 0xB7, 0x7C,
        0x30, 0x28, 0x8D, 0x4C, 0x30, 0xD0
    };
    static const BYTE expected_traversal_1[] = {
        0x8D, 0x04, 0x7F, 0xC1, 0xE0, 0x05,
        0x66, 0x39, 0x6C, 0x30, 0x26, 0x0F,
        0xB7, 0x7C, 0x30, 0x28, 0x8D, 0x4C,
        0x30, 0xD0
    };
    static const BYTE expected_allocation[] = {
        0x8D, 0x04, 0x40, 0x53, 0xC1, 0xE0, 0x05,
        0x8D, 0x74, 0x38, 0xD0, 0x55, 0x56, 0x8B, 0xCF
    };
    DWORD shadow_base = (DWORD)g_provider_storage;
    DWORD record_index_base = (DWORD)g_provider_records -
        PROCEDURAL_RECORD_SIZE;
    DWORD next_index_base = (DWORD)g_provider_records -
        (PROCEDURAL_RECORD_SIZE - SOURCE_NEXT_OFFSET);

    write_u32(
        expected_manager_0 + 1u,
        (DWORD)(image_base + PROVIDER_MANAGER_POINTER_RVA));
    write_u32(
        expected_manager_1 + 1u,
        (DWORD)(image_base + PROVIDER_MANAGER_POINTER_RVA));
    write_u32(
        expected_next,
        (DWORD)(image_base + PROVIDER_NEXT_INDEX_POINTER_RVA));
    write_u32(
        expected_record,
        (DWORD)(image_base + PROVIDER_RECORD_INDEX_POINTER_RVA));

    configure_provider_patch(
        &patches[0], image_base + g_build_profile->provider_sites[0],
        expected_entity_lookup, sizeof(expected_entity_lookup));
    configure_provider_patch(
        &patches[1], image_base + g_build_profile->provider_sites[1],
        expected_manager_0, sizeof(expected_manager_0));
    configure_provider_patch(
        &patches[2], image_base + g_build_profile->provider_sites[2],
        expected_manager_1, sizeof(expected_manager_1));
    configure_provider_patch(
        &patches[3], image_base + g_build_profile->provider_sites[3],
        expected_next, sizeof(expected_next));
    configure_provider_patch(
        &patches[4], image_base + g_build_profile->provider_sites[4],
        expected_record, sizeof(expected_record));
    configure_provider_patch(
        &patches[5], image_base + g_build_profile->provider_sites[5],
        expected_record_lookup, sizeof(expected_record_lookup));
    configure_provider_patch(
        &patches[6], image_base + g_build_profile->provider_sites[6],
        expected_traversal_0, sizeof(expected_traversal_0));
    configure_provider_patch(
        &patches[7], image_base + g_build_profile->provider_sites[7],
        expected_traversal_1, sizeof(expected_traversal_1));
    configure_provider_patch(
        &patches[8], image_base + g_build_profile->provider_sites[8],
        expected_allocation, sizeof(expected_allocation));

    patches[0].replacement[0] = 0x6B;
    patches[0].replacement[1] = 0xC0;
    patches[0].replacement[2] = 0x60;
    patches[0].replacement[3] = 0x05;
    write_u32(patches[0].replacement + 4u, record_index_base);
    patches[0].replacement[8] = 0x39;
    patches[0].replacement[9] = 0x50;
    patches[0].replacement[10] = 0x44;
    patches[0].replacement[11] = 0x90;
    patches[0].replacement[12] = 0x90;
    patches[0].replacement[13] = 0x90;

    write_u32(patches[1].replacement + 1u, shadow_base);
    write_u32(patches[2].replacement + 1u, shadow_base);
    write_u32(patches[3].replacement, next_index_base);
    write_u32(patches[4].replacement, record_index_base);

    patches[5].replacement[0] = 0x6B;
    patches[5].replacement[1] = 0xC0;
    patches[5].replacement[2] = 0x60;
    patches[5].replacement[3] = 0x05;
    write_u32(patches[5].replacement + 4u, record_index_base);
    patches[5].replacement[8] = 0x8B;
    patches[5].replacement[9] = 0xF0;

    patches[6].replacement[0] = 0x0F;
    patches[6].replacement[1] = 0xB7;
    patches[6].replacement[2] = 0xC7;
    patches[6].replacement[3] = 0x6B;
    patches[6].replacement[4] = 0xC0;
    patches[6].replacement[5] = 0x60;
    patches[6].replacement[6] = 0x05;
    write_u32(patches[6].replacement + 7u, record_index_base);
    patches[6].replacement[11] = 0x0F;
    patches[6].replacement[12] = 0xB7;
    patches[6].replacement[13] = 0x78;
    patches[6].replacement[14] = 0x58;
    patches[6].replacement[15] = 0x8B;
    patches[6].replacement[16] = 0xC8;
    patches[6].replacement[17] = 0x90;

    patches[7].replacement[0] = 0x6B;
    patches[7].replacement[1] = 0xC7;
    patches[7].replacement[2] = 0x60;
    patches[7].replacement[3] = 0x05;
    write_u32(patches[7].replacement + 4u, record_index_base);
    patches[7].replacement[8] = 0x66;
    patches[7].replacement[9] = 0x39;
    patches[7].replacement[10] = 0x68;
    patches[7].replacement[11] = 0x56;
    patches[7].replacement[12] = 0x0F;
    patches[7].replacement[13] = 0xB7;
    patches[7].replacement[14] = 0x78;
    patches[7].replacement[15] = 0x58;
    patches[7].replacement[16] = 0x8B;
    patches[7].replacement[17] = 0xC8;
    patches[7].replacement[18] = 0x90;
    patches[7].replacement[19] = 0x90;

    patches[8].replacement[0] = 0x6B;
    patches[8].replacement[1] = 0xC0;
    patches[8].replacement[2] = 0x60;
    patches[8].replacement[3] = 0x05;
    write_u32(patches[8].replacement + 4u, record_index_base);
    patches[8].replacement[8] = 0x53;
    patches[8].replacement[9] = 0x8B;
    patches[8].replacement[10] = 0xF0;
    patches[8].replacement[11] = 0x55;
    patches[8].replacement[12] = 0x56;
    patches[8].replacement[13] = 0x8B;
    patches[8].replacement[14] = 0xCF;
}

static void configure_provider_patches(
    BYTE *image_base, struct ProviderPatch *patches)
{
    static const BYTE expected_ce_head_publish[] = {
        0xC7, 0x06, 0x01, 0x00, 0x00, 0x00
    };
    static const BYTE expected_patch_head_publish[] = {
        0x66, 0xC7, 0x06, 0x01, 0x00
    };
    static const BYTE expected_rebuild_epilogue[] = {
        0x5F, 0x5E, 0xB0, 0x01, 0x5B,
        0x8B, 0xE5, 0x5D, 0xC3
    };
    DWORD head_patch_index;
    DWORD epilogue_patch_index;
    DWORD relative;

    ZeroMemory(
        patches,
        sizeof(struct ProviderPatch) * PROVIDER_PATCH_CAPACITY);
    if (g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_CE) {
        configure_provider_patches_ce(image_base, patches);
    } else if (g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_PATCH8) {
        configure_provider_patches_patch8(image_base, patches);
    }

    head_patch_index = PROVIDER_RELOCATION_PATCH_COUNT;
    if (g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_CE) {
        configure_provider_patch(
            &patches[head_patch_index],
            image_base + PROVIDER_HEAD_QUARANTINE_RVA,
            expected_ce_head_publish,
            sizeof(expected_ce_head_publish));
    } else if (g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_PATCH8) {
        configure_provider_patch(
            &patches[head_patch_index],
            image_base + PROVIDER_HEAD_QUARANTINE_RVA,
            expected_patch_head_publish,
            sizeof(expected_patch_head_publish));
    }
    patches[head_patch_index].replacement[
        g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_CE ? 2u : 3u] = 0x00u;

    epilogue_patch_index = head_patch_index + 1u;
    configure_provider_patch(
        &patches[epilogue_patch_index],
        image_base + PROVIDER_REBUILD_EPILOGUE_RVA,
        expected_rebuild_epilogue,
        sizeof(expected_rebuild_epilogue));
    patches[epilogue_patch_index].replacement[0] = 0xE9u;
    relative = (DWORD)g_provider_rebuild_stub -
        (DWORD)(patches[epilogue_patch_index].address + 5u);
    write_u32(
        patches[epilogue_patch_index].replacement + 1u,
        relative);
    patches[epilogue_patch_index].replacement[5] = 0x90u;
    patches[epilogue_patch_index].replacement[6] = 0x90u;
    patches[epilogue_patch_index].replacement[7] = 0x90u;
    patches[epilogue_patch_index].replacement[8] = 0x90u;
}

static int validate_provider_patches(
    struct ProviderPatch *patches)
{
    DWORD index;
    DWORD verified = 0;

    g_report.provider_patch_sites_verified = 0u;
    g_report.provider_patch_site_mismatches = 0u;
    g_report.provider_first_mismatch_site = 0u;
    g_report.provider_first_mismatch_byte = 0u;

    for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
        DWORD byte_index;
        copy_bytes(
            patches[index].observed_at_validation,
            patches[index].address,
            patches[index].length);
        patches[index].original_match = bytes_are_equal(
            patches[index].observed_at_validation,
            patches[index].original,
            patches[index].length);
        if (patches[index].original_match) {
            ++verified;
            continue;
        }

        ++g_report.provider_patch_site_mismatches;
        if (g_report.provider_first_mismatch_site == 0u) {
            g_report.provider_first_mismatch_site = index + 1u;
            for (byte_index = 0;
                 byte_index < patches[index].length;
                 ++byte_index) {
                if (patches[index].observed_at_validation[byte_index] !=
                    patches[index].original[byte_index]) {
                    g_report.provider_first_mismatch_byte = byte_index;
                    break;
                }
            }
        }
    }

    g_report.provider_patch_sites_verified = verified;
    return verified == PROVIDER_PATCH_COUNT;
}

static int readable_address_range(const BYTE *address, DWORD size);

static int initialize_provider_storage(DWORD capacity)
{
    DWORD index;
    DWORD bytes = MANAGER_SOURCE_RECORDS_OFFSET +
        capacity * PROCEDURAL_RECORD_SIZE;
    BYTE *storage = (BYTE *)VirtualAlloc(
        NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!storage) {
        return 0;
    }

    ZeroMemory(storage, bytes);
    g_provider_storage = storage;
    g_provider_records = storage + MANAGER_SOURCE_RECORDS_OFFSET;
    g_provider_capacity = capacity;

    for (index = 0; index < capacity; ++index) {
        BYTE *record =
            g_provider_records + index * PROCEDURAL_RECORD_SIZE;
        WORD next = (WORD)(
            index + 1u == capacity ? 0u : index + 2u);
        WORD previous = (WORD)(index == 0u ? 0u : index);
        write_u16(record + SOURCE_NEXT_OFFSET, next);
        write_u16(record + SOURCE_NEXT_OFFSET + 2u, previous);
    }

    for (index = 0; index < capacity; ++index) {
        BYTE *record =
            g_provider_records + index * PROCEDURAL_RECORD_SIZE;
        WORD expected_next = (WORD)(
            index + 1u == capacity ? 0u : index + 2u);
        WORD expected_previous =
            (WORD)(index == 0u ? 0u : index);
        if (read_u16(record + SOURCE_NEXT_OFFSET) !=
                expected_next ||
            read_u16(record + SOURCE_NEXT_OFFSET + 2u) !=
                expected_previous) {
            return 0;
        }
    }

    g_report.provider_storage = (DWORD)g_provider_storage;
    g_report.provider_records = (DWORD)g_provider_records;
    g_report.provider_storage_bytes = bytes;
    g_report.provider_free_list_verified = 1u;
    return 1;
}

static void GU_FASTCALL provider_post_rebuild_hook(
    BYTE *manager, DWORD ignored_edx)
{
    DWORD index;
    DWORD heads_before_quarantine = 0u;
    int clean = 1;
    int manager_valid;
    (void)ignored_edx;

    if (!g_provider_storage ||
        g_provider_capacity <= VANILLA_PROVIDER_CAPACITY) {
        return;
    }
    manager_valid = manager && manager == g_manager &&
        readable_address_range(
            manager,
            MANAGER_SOURCE_ACTIVE_HEAD_OFFSET + sizeof(WORD));
    if (manager_valid) {
        heads_before_quarantine = read_u32(manager);
        InterlockedExchange((volatile LONG *)manager, 0);
    }
    if (!manager_valid || !g_provider_records ||
        g_provider_capacity > 0xFFFFu ||
        !readable_address_range(
            g_provider_records,
            g_provider_capacity * PROCEDURAL_RECORD_SIZE) ||
        (heads_before_quarantine >> 16) != 0u) {
        clean = 0;
    }

    if (clean) {
        for (index = 0u; index < g_provider_capacity; ++index) {
            BYTE *record = g_provider_records +
                index * PROCEDURAL_RECORD_SIZE;
            if (read_u32(record + PROVIDER_OWNER_POINTER_OFFSET) != 0u ||
                read_u32(record + PROVIDER_BITMAP_POINTER_OFFSET) != 0u ||
                read_u32(record + PROVIDER_AUXILIARY_POINTER_OFFSET) != 0u ||
                read_u16(record + PROVIDER_RESOURCE_FLAGS_OFFSET) != 0u) {
                clean = 0;
                break;
            }
        }
    }

    if (!clean) {
        if (manager_valid) {
            InterlockedExchange((volatile LONG *)manager, 0);
        }
        InterlockedIncrement(&g_provider_rebuild_failures);
        return;
    }

    for (index = 0u; index < g_provider_capacity; ++index) {
        BYTE *record = g_provider_records +
            index * PROCEDURAL_RECORD_SIZE;
        WORD next = (WORD)(
            index + 1u == g_provider_capacity ? 0u : index + 2u);
        WORD previous = (WORD)(index == 0u ? 0u : index);
        write_u16(record + SOURCE_NEXT_OFFSET, next);
        write_u16(record + SOURCE_NEXT_OFFSET + 2u, previous);
    }
    /* InterlockedExchange below is the release barrier for rebuilt links. */
    InterlockedExchange((volatile LONG *)manager, 1);

    if (read_u16(manager + MANAGER_SOURCE_FREE_HEAD_OFFSET) != 1u ||
        read_u16(manager + MANAGER_SOURCE_ACTIVE_HEAD_OFFSET) != 0u) {
        clean = 0;
    }
    for (index = 0u; clean && index < g_provider_capacity; ++index) {
        BYTE *record = g_provider_records +
            index * PROCEDURAL_RECORD_SIZE;
        WORD expected_next = (WORD)(
            index + 1u == g_provider_capacity ? 0u : index + 2u);
        WORD expected_previous = (WORD)(index == 0u ? 0u : index);
        if (read_u16(record + SOURCE_NEXT_OFFSET) != expected_next ||
            read_u16(record + SOURCE_NEXT_OFFSET + 2u) !=
                expected_previous) {
            clean = 0;
        }
    }
    if (!clean) {
        InterlockedExchange((volatile LONG *)manager, 0);
        InterlockedIncrement(&g_provider_rebuild_failures);
        return;
    }
    InterlockedIncrement(&g_provider_rebuilds_verified);
}

static BOOL restore_patch_page_protection_once(
    BYTE *span_start, DWORD span, DWORD old_protection,
    DWORD *unused_protection)
{
#ifdef GTAIVPOOL_TEST_NO_FATAL
    LONG failures;
    for (;;) {
        failures = InterlockedCompareExchange(
            &g_fixture_patch_protection_restore_failures, 0, 0);
        if (failures <= 0) {
            break;
        }
        if (InterlockedCompareExchange(
                &g_fixture_patch_protection_restore_failures,
                failures - 1, failures) == failures) {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
    }
#endif
    return VirtualProtect(
        span_start, span, old_protection, unused_protection);
}

static int restore_patch_page_protection_retry(
    BYTE *span_start, DWORD span, DWORD old_protection)
{
    DWORD attempt;
    DWORD unused_protection = 0u;
    for (attempt = 0u;
         attempt < PATCH_PROTECTION_RESTORE_ATTEMPTS;
         ++attempt) {
        if (restore_patch_page_protection_once(
                span_start, span, old_protection,
                &unused_protection)) {
            return 1;
        }
    }
    return 0;
}

static int flush_patch_span_retry(BYTE *span_start, DWORD span)
{
    DWORD attempt;
    for (attempt = 0u;
         attempt < PATCH_PROTECTION_RESTORE_ATTEMPTS;
         ++attempt) {
        if (FlushInstructionCache(
                GetCurrentProcess(), span_start, span)) {
            return 1;
        }
    }
    return 0;
}

static int restore_provider_patches_writable(
    const struct ProviderPatch *patches)
{
    DWORD index;
    int restored = 1;
    for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
        copy_bytes(
            patches[index].address,
            patches[index].original,
            patches[index].length);
    }
    for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
        if (!bytes_are_equal(
                patches[index].address,
                patches[index].original,
                patches[index].length)) {
            restored = 0;
        }
    }
    return restored;
}

static int rollback_provider_patch_transaction_writable(
    const struct ProviderPatch *patches,
    BYTE *span_start, DWORD span, DWORD old_protection)
{
    int bytes_restored = restore_provider_patches_writable(patches);
    int cache_flushed = flush_patch_span_retry(span_start, span);
    int protection_restored = restore_patch_page_protection_retry(
        span_start, span, old_protection);
    if (!bytes_restored || !cache_flushed || !protection_restored) {
        return 0;
    }
    g_report.provider_patch_sites_applied = 0u;
    InterlockedExchange(&g_provider_patch_transaction_dirty, 0);
    return 1;
}

static int apply_provider_patches(
    struct ProviderPatch *patches)
{
    BYTE *span_start = patches[0].address;
    BYTE *span_end = patches[0].address + patches[0].length;
    DWORD span;
    DWORD old_protection = 0;
    DWORD unused_protection = 0;
    DWORD index;

    for (index = 1u; index < PROVIDER_PATCH_COUNT; ++index) {
        BYTE *patch_end =
            patches[index].address + patches[index].length;
        if (patches[index].address < span_start) {
            span_start = patches[index].address;
        }
        if (patch_end > span_end) {
            span_end = patch_end;
        }
    }
    span = (DWORD)(span_end - span_start);

    if (!VirtualProtect(
            span_start, span, PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        g_report.status = STATUS_PROVIDER_VIRTUAL_PROTECT_FAILED;
        g_report.last_error = GetLastError();
        return 0;
    }
    g_report.provider_old_protection = old_protection;
    g_report.provider_patch_sites_applied = PROVIDER_PATCH_COUNT;
    InterlockedExchange(&g_provider_patch_transaction_dirty, 1);

    for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
        copy_bytes(
            patches[index].address,
            patches[index].replacement,
            patches[index].length);
    }
    for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
        if (!bytes_are_equal(
                patches[index].address,
                patches[index].replacement,
                patches[index].length)) {
            int restored = rollback_provider_patch_transaction_writable(
                patches, span_start, span, old_protection);
            g_report.status = restored ?
                STATUS_PROVIDER_WRITE_VERIFY_FAILED :
                STATUS_PROVIDER_ROLLBACK_FAILED;
            return 0;
        }
    }

    g_report.provider_flush_result =
        (DWORD)FlushInstructionCache(
            GetCurrentProcess(), span_start, span);
    if (!g_report.provider_flush_result) {
        DWORD flush_error = GetLastError();
        int restored = rollback_provider_patch_transaction_writable(
            patches, span_start, span, old_protection);
        g_report.last_error = flush_error;
        g_report.status = restored ?
            STATUS_PROVIDER_FLUSH_FAILED_REVERTED :
            STATUS_PROVIDER_ROLLBACK_FAILED;
        return 0;
    }

    g_report.provider_protection_restore_result =
        (DWORD)restore_patch_page_protection_once(
            span_start, span, old_protection,
            &unused_protection);
    if (!g_report.provider_protection_restore_result) {
        DWORD protection_error = GetLastError();
        int restored = rollback_provider_patch_transaction_writable(
            patches, span_start, span, old_protection);
        g_report.status = restored ?
            STATUS_PROVIDER_PROTECTION_RESTORE_FAILED :
            STATUS_PROVIDER_ROLLBACK_FAILED;
        g_report.last_error = protection_error;
        return 0;
    }

    InterlockedExchange(&g_provider_patch_transaction_dirty, 0);
    return 1;
}

static void configure_generator_patch(
    struct GeneratorPatch *patch, BYTE *address,
    const BYTE *expected, DWORD length)
{
    ZeroMemory(patch, sizeof(*patch));
    patch->address = address;
    patch->length = length;
    copy_bytes(patch->original, expected, length);
    copy_bytes(patch->replacement, expected, length);
}

static void configure_generator_patches(
    BYTE *image_base, DWORD capacity,
    struct GeneratorPatch *patches)
{
    BYTE expected_special[] = {
        0x81, 0x3D, 0, 0, 0, 0,
        0x00, 0x02, 0x00, 0x00
    };
    BYTE expected_render[10];
    BYTE expected_allocator[11];
    static const BYTE expected_staging_alloc[] = {
        0x68, 0x00, 0x40, 0x00, 0x00
    };
    static const BYTE expected_staging_cap[] = {
        0xB8, 0x00, 0x02, 0x00, 0x00
    };
    DWORD render_length;
    DWORD render_capacity_offset;
    DWORD relative;
    DWORD staging_bytes = capacity * GENERATOR_STAGING_RECORD_SIZE;

    write_u32(
        expected_special + 2u,
        (DWORD)(image_base + VERIFIED_GENERATOR_MANAGER_RVA));
    if (g_build_profile->code_variant == BUILD_CODE_CE) {
        expected_render[0] = 0x81;
        expected_render[1] = 0xF9;
        write_u32(expected_render + 2u, VANILLA_RENDERED_OBJECT_CAPACITY);
        render_length = 6u;
        render_capacity_offset = 2u;
    } else {
        expected_render[0] = 0x81;
        expected_render[1] = 0x3D;
        write_u32(
            expected_render + 2u,
            (DWORD)(image_base + VERIFIED_GENERATOR_MANAGER_RVA));
        write_u32(
            expected_render + 6u,
            VANILLA_RENDERED_OBJECT_CAPACITY);
        render_length = 10u;
        render_capacity_offset = 6u;
    }
    copy_bytes(
        expected_allocator,
        g_build_profile->generator_allocator_original,
        sizeof(expected_allocator));

    configure_generator_patch(
        &patches[0],
        image_base + VERIFIED_GENERATOR_SPECIAL_CAP_RVA,
        expected_special, sizeof(expected_special));
    configure_generator_patch(
        &patches[1],
        image_base + VERIFIED_GENERATOR_RENDER_CAP_RVA,
        expected_render, render_length);
    configure_generator_patch(
        &patches[2],
        image_base + VERIFIED_GENERATOR_ALLOCATOR_RVA,
        expected_allocator, sizeof(expected_allocator));
    configure_generator_patch(
        &patches[3],
        image_base + VERIFIED_GENERATOR_STAGING_ALLOC_RVA,
        expected_staging_alloc, sizeof(expected_staging_alloc));
    configure_generator_patch(
        &patches[4],
        image_base + VERIFIED_GENERATOR_STAGING_CAP_RVA,
        expected_staging_cap, sizeof(expected_staging_cap));

    write_u32(patches[0].replacement + 6u, capacity);
    write_u32(
        patches[1].replacement + render_capacity_offset,
        capacity);

    relative =
        (DWORD)generator_pop_trampoline -
        (DWORD)(patches[2].address + 5u);
    patches[2].replacement[0] = 0xE9;
    write_u32(patches[2].replacement + 1u, relative);
    patches[2].replacement[5] = 0x90;
    patches[2].replacement[6] = 0x90;
    patches[2].replacement[7] = 0x90;
    patches[2].replacement[8] = 0x90;
    patches[2].replacement[9] = 0x90;
    patches[2].replacement[10] = 0x90;

    write_u32(patches[3].replacement + 1u, staging_bytes);
    write_u32(patches[4].replacement + 1u, capacity);
}

static int validate_generator_patches(
    struct GeneratorPatch *patches)
{
    DWORD index;
    DWORD verified = 0u;

    g_report.generator_patch_sites_verified = 0u;
    g_report.generator_patch_site_mismatches = 0u;
    g_report.generator_first_mismatch_site = 0u;
    g_report.generator_first_mismatch_byte = 0u;

    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        DWORD byte_index;
        copy_bytes(
            patches[index].observed_at_validation,
            patches[index].address,
            patches[index].length);
        patches[index].original_match = bytes_are_equal(
            patches[index].observed_at_validation,
            patches[index].original,
            patches[index].length);
        if (patches[index].original_match) {
            ++verified;
            continue;
        }

        ++g_report.generator_patch_site_mismatches;
        if (g_report.generator_first_mismatch_site == 0u) {
            g_report.generator_first_mismatch_site = index + 1u;
            for (byte_index = 0u;
                 byte_index < patches[index].length;
                 ++byte_index) {
                if (patches[index].observed_at_validation[byte_index] !=
                    patches[index].original[byte_index]) {
                    g_report.generator_first_mismatch_byte =
                        byte_index;
                    break;
                }
            }
        }
    }

    g_report.generator_patch_sites_verified = verified;
    return verified == GENERATOR_PATCH_COUNT;
}

static int initialize_generator_storage(DWORD capacity)
{
    DWORD extra_count;
    DWORD bytes;
    BYTE *storage;

    if (capacity <= VANILLA_RENDERED_OBJECT_CAPACITY) {
        g_generator_extra_records = NULL;
        g_generator_extra_record_count = 0u;
        g_generator_render_capacity =
            VANILLA_RENDERED_OBJECT_CAPACITY;
        return 1;
    }

    extra_count =
        capacity - VANILLA_RENDERED_OBJECT_CAPACITY;
    bytes = extra_count * GENERATOR_RECORD_SIZE;
    storage = (BYTE *)VirtualAlloc(
        NULL, bytes,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!storage) {
        return 0;
    }

    ZeroMemory(storage, bytes);
    g_generator_extra_records = storage;
    g_generator_extra_record_count = extra_count;
    g_generator_render_capacity = capacity;
    InterlockedExchange(&g_generator_extra_issued, 0);
    InterlockedExchange(
        &g_generator_extra_exhaustion_observed, 0);
    InterlockedExchange(&g_generator_fallback_calls, 0);
    InterlockedExchange(&g_plant_selector_positive_calls, 0);
    InterlockedExchange(&g_plant_selector_neutral_calls, 0);
    InterlockedExchange(&g_plant_selector_last_output_key, 0);
    InterlockedExchange(&g_plant_selector_last_source_hash, 0);
    InterlockedExchange(&g_plant_selector_last_submesh, 0);

    g_report.generator_extra_storage = (DWORD)storage;
    g_report.generator_extra_record_count = extra_count;
    g_report.generator_extra_storage_bytes = bytes;
    return 1;
}

static int restore_generator_patches_writable(
    const struct GeneratorPatch *patches)
{
    DWORD index;
    int restored = 1;

    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        copy_bytes(
            patches[index].address,
            patches[index].original,
            patches[index].length);
    }
    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        if (!bytes_are_equal(
                patches[index].address,
                patches[index].original,
                patches[index].length)) {
            restored = 0;
        }
    }
    return restored;
}

static int rollback_generator_patch_transaction_writable(
    const struct GeneratorPatch *patches,
    BYTE *span_start, DWORD span, DWORD old_protection)
{
    int bytes_restored = restore_generator_patches_writable(patches);
    int cache_flushed = flush_patch_span_retry(span_start, span);
    int protection_restored = restore_patch_page_protection_retry(
        span_start, span, old_protection);
    if (!bytes_restored || !cache_flushed || !protection_restored) {
        return 0;
    }
    g_report.generator_patch_sites_applied = 0u;
    InterlockedExchange(&g_generator_patch_transaction_dirty, 0);
    return 1;
}

static int apply_generator_patches(
    struct GeneratorPatch *patches)
{
    BYTE *span_start = patches[0].address;
    BYTE *span_end = patches[0].address + patches[0].length;
    DWORD span;
    DWORD old_protection = 0u;
    DWORD unused_protection = 0u;
    DWORD index;

    for (index = 1u; index < GENERATOR_PATCH_COUNT; ++index) {
        BYTE *patch_end = patches[index].address + patches[index].length;
        if (patches[index].address < span_start) {
            span_start = patches[index].address;
        }
        if (patch_end > span_end) {
            span_end = patch_end;
        }
    }
    span = (DWORD)(span_end - span_start);

    if (!VirtualProtect(
            span_start, span, PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        g_report.status =
            STATUS_GENERATOR_VIRTUAL_PROTECT_FAILED;
        g_report.last_error = GetLastError();
        return 0;
    }
    g_report.generator_old_protection = old_protection;
    g_report.generator_patch_sites_applied = GENERATOR_PATCH_COUNT;
    InterlockedExchange(&g_generator_patch_transaction_dirty, 1);

    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        copy_bytes(
            patches[index].address,
            patches[index].replacement,
            patches[index].length);
    }
    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        if (!bytes_are_equal(
                patches[index].address,
                patches[index].replacement,
                patches[index].length)) {
            int restored = rollback_generator_patch_transaction_writable(
                patches, span_start, span, old_protection);
            g_report.status = restored ?
                STATUS_GENERATOR_WRITE_VERIFY_FAILED :
                STATUS_GENERATOR_ROLLBACK_FAILED;
            return 0;
        }
    }

    g_report.generator_flush_result =
        (DWORD)FlushInstructionCache(
            GetCurrentProcess(), span_start, span);
    if (!g_report.generator_flush_result) {
        DWORD flush_error = GetLastError();
        int restored = rollback_generator_patch_transaction_writable(
            patches, span_start, span, old_protection);
        g_report.last_error = flush_error;
        g_report.status = restored ?
            STATUS_GENERATOR_FLUSH_FAILED_REVERTED :
            STATUS_GENERATOR_ROLLBACK_FAILED;
        return 0;
    }

    g_report.generator_protection_restore_result =
        (DWORD)restore_patch_page_protection_once(
            span_start, span, old_protection,
            &unused_protection);
    if (!g_report.generator_protection_restore_result) {
        DWORD protection_error = GetLastError();
        int restored = rollback_generator_patch_transaction_writable(
            patches, span_start, span, old_protection);
        g_report.status = restored ?
            STATUS_GENERATOR_PROTECTION_RESTORE_FAILED :
            STATUS_GENERATOR_ROLLBACK_FAILED;
        g_report.last_error = protection_error;
        return 0;
    }

    InterlockedExchange(&g_generator_patch_transaction_dirty, 0);
    return 1;
}

static DWORD string_length(const char *text)
{
    DWORD length = 0;
    if (!text) {
        return 0;
    }
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static void copy_string(char *destination, DWORD capacity, const char *source)
{
    DWORD index = 0;
    if (!destination || capacity == 0) {
        return;
    }
    if (source) {
        while (index + 1 < capacity && source[index] != '\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = '\0';
}

static void text_buffer_append(
    char *destination, DWORD capacity, const char *source)
{
    DWORD index;
    if (!destination || capacity == 0u) {
        return;
    }
    index = string_length(destination);
    while (source && *source && index + 1u < capacity) {
        destination[index++] = *source++;
    }
    destination[index] = '\0';
}

static void text_buffer_append_u32(
    char *destination, DWORD capacity, DWORD value)
{
    char digits[16];
    DWORD count = 0u;
    char one[2];
    if (value == 0u) {
        text_buffer_append(destination, capacity, "0");
        return;
    }
    while (value != 0u && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    one[1] = '\0';
    while (count != 0u) {
        one[0] = digits[--count];
        text_buffer_append(destination, capacity, one);
    }
}

static void text_buffer_append_hex32(
    char *destination, DWORD capacity, DWORD value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[11];
    DWORD index;
    text[0] = '0';
    text[1] = 'x';
    for (index = 0u; index < 8u; ++index) {
        DWORD shift = (7u - index) * 4u;
        text[index + 2u] = digits[(value >> shift) & 0xFu];
    }
    text[10] = '\0';
    text_buffer_append(destination, capacity, text);
}

static void make_sibling_path(const char *module_path, const char *filename,
                              char *destination, DWORD capacity)
{
    DWORD length;
    DWORD cut;
    DWORD index;

    if (!destination || capacity == 0) {
        return;
    }

    copy_string(destination, capacity, module_path);
    length = string_length(destination);
    cut = length;

    while (cut > 0) {
        char value = destination[cut - 1];
        if (value == '\\' || value == '/') {
            break;
        }
        --cut;
    }

    if (cut == 0) {
        destination[0] = '\0';
    } else {
        destination[cut] = '\0';
    }

    index = string_length(destination);
    while (index + 1 < capacity && filename && *filename) {
        destination[index++] = *filename++;
    }
    destination[index] = '\0';
}

static int target_bytes_match(const BYTE *candidate)
{
    const BYTE *pattern =
        g_build_profile->code_variant == BUILD_CODE_CE ?
        g_ce_target_pattern : g_patch8_target_pattern;
    DWORD index;
    for (index = 0; index < TARGET_PATTERN_LENGTH; ++index) {
        if (index >= TARGET_IMMEDIATE_OFFSET &&
            index < TARGET_IMMEDIATE_OFFSET + 4u) {
            continue;
        }
        if (candidate[index] != pattern[index]) {
            return 0;
        }
    }
    return 1;
}

static int offset_in_pointer(DWORD index, DWORD pointer_offset)
{
    return index >= pointer_offset && index < pointer_offset + 4u;
}

static int distance_init_bytes_match(const BYTE *candidate)
{
    const BYTE *pattern =
        g_build_profile->code_variant == BUILD_CODE_CE ?
        g_ce_distance_init_pattern :
        g_patch8_distance_init_pattern;
    DWORD index;
    for (index = 0; index < DISTANCE_INIT_PATTERN_LENGTH; ++index) {
        if (offset_in_pointer(index, DIST_INIT_BASE_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_INIT_DETAIL_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_INIT_NEAR_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_INIT_FAR_POINTER_OFFSET)) {
            continue;
        }
        if (candidate[index] != pattern[index]) {
            return 0;
        }
    }
    return 1;
}

static int distance_update_bytes_match(const BYTE *candidate)
{
    const BYTE *pattern =
        g_build_profile->code_variant == BUILD_CODE_CE ?
        g_ce_distance_update_pattern :
        g_patch8_distance_update_pattern;
    DWORD index;
    for (index = 0; index < DISTANCE_UPDATE_PATTERN_LENGTH; ++index) {
        if (offset_in_pointer(index, DIST_UPDATE_BASE_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_UPDATE_DETAIL_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_UPDATE_NEAR_POINTER_OFFSET) ||
            offset_in_pointer(index, DIST_UPDATE_FAR_POINTER_OFFSET)) {
            continue;
        }
        if (candidate[index] != pattern[index]) {
            return 0;
        }
    }
    return 1;
}

static int image_pointer_has_bits(const BYTE *image_base, DWORD image_size,
                                  DWORD pointer, DWORD expected_bits)
{
    DWORD start = (DWORD)image_base;
    DWORD end = start + image_size;
    if (end < start ||
        pointer < start ||
        pointer > end - 4u) {
        return 0;
    }
    return read_u32((const BYTE *)pointer) == expected_bits;
}

static int distance_sources_are_valid(const BYTE *image_base, DWORD image_size,
                                      const BYTE *distance_init,
                                      const BYTE *distance_update)
{
    DWORD init_base = read_u32(
        distance_init + DIST_INIT_BASE_POINTER_OFFSET);
    DWORD init_detail = read_u32(
        distance_init + DIST_INIT_DETAIL_POINTER_OFFSET);
    DWORD init_near = read_u32(
        distance_init + DIST_INIT_NEAR_POINTER_OFFSET);
    DWORD init_far = read_u32(
        distance_init + DIST_INIT_FAR_POINTER_OFFSET);
    DWORD update_base = read_u32(
        distance_update + DIST_UPDATE_BASE_POINTER_OFFSET);
    DWORD update_detail = read_u32(
        distance_update + DIST_UPDATE_DETAIL_POINTER_OFFSET);
    DWORD update_near = read_u32(
        distance_update + DIST_UPDATE_NEAR_POINTER_OFFSET);
    DWORD update_far = read_u32(
        distance_update + DIST_UPDATE_FAR_POINTER_OFFSET);

    if (distance_update == distance_init) {
        return 0;
    }

    if (init_base != update_base ||
        init_detail != update_detail ||
        init_near != update_near ||
        init_far != update_far) {
        return 0;
    }

    if (!image_pointer_has_bits(
            image_base, image_size, init_detail, FLOAT_BITS_HALF) ||
        !image_pointer_has_bits(
            image_base, image_size, init_near, FLOAT_BITS_20) ||
        !image_pointer_has_bits(
            image_base, image_size, init_far, FLOAT_BITS_40)) {
        return 0;
    }

    {
        DWORD start = (DWORD)image_base;
        DWORD end = start + image_size;
        return end >= start &&
               init_base >= start &&
               init_base <= end - 4u;
    }
}

static int wrapper_bytes_match(const BYTE *candidate, const BYTE *image_base)
{
#ifdef GTAIVPOOL_TEST_NO_FATAL
    const BYTE *wrapper;
    DWORD index;
    if ((DWORD)(candidate - image_base) < TARGET_WRAPPER_DISTANCE) {
        return 0;
    }
    wrapper = candidate - TARGET_WRAPPER_DISTANCE;
    if (wrapper[0] != 0xB9 || wrapper[5] != 0xE9 ||
        wrapper[6] != 0x06 || wrapper[7] != 0x00 ||
        wrapper[8] != 0x00 || wrapper[9] != 0x00) {
        return 0;
    }
    for (index = 10u; index < 16u; ++index) {
        if (wrapper[index] != 0xCC) {
            return 0;
        }
    }
    return 1;
#else
    if ((VERIFIED_TARGET_RVA != 0u &&
         (DWORD)(candidate - image_base) != VERIFIED_TARGET_RVA) ||
        (DWORD)(candidate - image_base) < 3u) {
        return 0;
    }
    if (g_build_profile->code_variant == BUILD_CODE_CE) {
        const BYTE *wrapper = candidate - TARGET_WRAPPER_DISTANCE;
        DWORD index;
        if (wrapper[0] != 0xB9 || wrapper[5] != 0xE9 ||
            wrapper[6] != 0x06 || wrapper[7] != 0x00 ||
            wrapper[8] != 0x00 || wrapper[9] != 0x00) {
            return 0;
        }
        for (index = 10u; index < TARGET_WRAPPER_DISTANCE; ++index) {
            if (wrapper[index] != 0xCC) {
                return 0;
            }
        }
        return 1;
    }
    return candidate[-3] == 0x0F &&
        candidate[-2] == 0x57 &&
        candidate[-1] == 0xC0 &&
        candidate[7] == 0x89 &&
        candidate[8] == 0x81 &&
        candidate[9] == 0x30 &&
        candidate[10] == 0x0F &&
        candidate[11] == 0x00 &&
        candidate[12] == 0x00;
#endif
}

static int readable_address_range(const BYTE *address, DWORD size);

static int image_rva_range_is_valid(
    DWORD image_size, DWORD rva, DWORD length)
{
    return length != 0u && rva < image_size &&
        length <= image_size - rva;
}

static int image_rva_range_is_readable(
    BYTE *image_base, DWORD image_size, DWORD rva, DWORD length)
{
    return image_rva_range_is_valid(image_size, rva, length) &&
        readable_address_range(image_base + rva, length);
}

static const struct BuildProfile *find_build_profile_by_id(DWORD id)
{
    DWORD index;
    for (index = 0u; index < g_build_profile_count; ++index) {
        if (g_build_profiles[index].id == id) {
            return &g_build_profiles[index];
        }
    }
    return NULL;
}

static const struct ExecutableIdentity *select_executable_identity(
    const char *sha256, DWORD file_size)
{
    DWORD index;
    const struct ExecutableIdentity *match = NULL;
    for (index = 0u; index < g_executable_identity_count; ++index) {
        const struct ExecutableIdentity *candidate =
            &g_executable_identities[index];
        if (candidate->file_size == file_size &&
            strings_are_equal(candidate->sha256, sha256)) {
            if (match) {
                return NULL;
            }
            match = candidate;
        }
    }
    return match;
}

#ifndef GTAIVPOOL_TEST_NO_FATAL
/*
 * User-authorized unknown-EXE probing is deliberately much stricter than a
 * short signature scan.  A candidate family must match every generated
 * startup guard interval (including relocation-normalized bytes and halos),
 * its main profile metadata, exact call-return authority, and its universal
 * plant-setter contract.  Multiple exact identities may represent the same
 * independently verified layout family; different matching profile IDs are
 * ambiguous and are never patched.
 */
static const struct ExecutableIdentity *
select_compatibility_probe_identity(
    BYTE *image_base, DWORD image_size, DWORD pe_timestamp,
    DWORD preferred_image_base, DWORD *identity_matches,
    DWORD *family_matches)
{
    const struct ExecutableIdentity *selected = NULL;
    DWORD matched_profile_ids[16];
    DWORD matched_profile_count = 0u;
    DWORD identity_index;

    if (!image_base || image_size == 0u ||
        preferred_image_base == 0u || !identity_matches ||
        !family_matches) {
        return NULL;
    }
    *identity_matches = 0u;
    *family_matches = 0u;
    ZeroMemory(matched_profile_ids, sizeof(matched_profile_ids));

    for (identity_index = 0u;
         identity_index < g_executable_identity_count;
         ++identity_index) {
        const struct ExecutableIdentity *candidate =
            &g_executable_identities[identity_index];
        const struct BuildProfile *profile =
            find_build_profile_by_id(candidate->build_profile_id);
        const struct GuStartupExactCallContract *call_contract =
            startup_find_exact_call_contract(candidate->id);
        const struct GuUniversalExecutableProfile *universal_profile =
            gu_universal_profile_by_identity(candidate->id);
        DWORD mismatch_address = 0u;
        DWORD verified_bytes = 0u;
        DWORD verified_relocations = 0u;
        DWORD profile_index;
        int profile_seen = 0;

        if (!profile || !call_contract || !universal_profile ||
            profile->pe_timestamp != pe_timestamp ||
            profile->image_size != image_size ||
            call_contract->executable_identity_id != candidate->id ||
            call_contract->build_profile_id !=
                candidate->build_profile_id ||
            call_contract->preferred_image_base != preferred_image_base ||
            !strings_are_equal(
                call_contract->specimen_sha256, candidate->sha256) ||
            lstrcmpA(universal_profile->sha256, candidate->sha256) != 0 ||
            !startup_exact_call_contract_is_structurally_valid(
                call_contract, image_size) ||
            !startup_guard_interval_contains_rva_range(
                call_contract, GU_STARTUP_GUARD_PROCEDURAL,
                profile->procedural_code_first_rva,
                profile->procedural_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                call_contract, GU_STARTUP_GUARD_GENERATOR,
                profile->generator_code_first_rva,
                profile->generator_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                call_contract, GU_STARTUP_GUARD_SOURCE,
                profile->source_code_first_rva,
                profile->source_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                call_contract, GU_STARTUP_GUARD_DEFINITION,
                profile->definition_call_rva,
                profile->definition_call_rva +
                    VERIFIED_DEFINITION_CALL_LENGTH) ||
            !startup_guard_interval_contains_rva_range(
                call_contract, GU_STARTUP_GUARD_PLANT_SETTER,
                universal_profile->plant_distance_setter.rva,
                universal_profile->plant_distance_setter.rva +
                    universal_profile->plant_distance_setter.length) ||
            !startup_exact_return_contract_contains_rva(
                call_contract, profile->definition_call_rva + 10u) ||
            !startup_exact_return_contract_contains_rva(
                call_contract, profile->definition_call_rva + 15u) ||
            !startup_guard_intervals_match_preimages(
                call_contract, image_base, image_size,
                &mismatch_address, &verified_bytes,
                &verified_relocations) ||
            verified_bytes == 0u) {
            continue;
        }

        ++*identity_matches;
        for (profile_index = 0u;
             profile_index < matched_profile_count;
             ++profile_index) {
            if (matched_profile_ids[profile_index] ==
                    candidate->build_profile_id) {
                profile_seen = 1;
                break;
            }
        }
        if (!profile_seen) {
            if (matched_profile_count >=
                    sizeof(matched_profile_ids) /
                        sizeof(matched_profile_ids[0])) {
                return NULL;
            }
            matched_profile_ids[matched_profile_count++] =
                candidate->build_profile_id;
            if (!selected) {
                selected = candidate;
            }
        }
    }

    *family_matches = matched_profile_count;
    return matched_profile_count == 1u ? selected : NULL;
}
#endif

static int generator_expansion_supported_for_profile(DWORD profile_id)
{
    return find_build_profile_by_id(profile_id) != NULL;
}

static int definition_startup_call_matches(
    BYTE *image_base, DWORD image_size)
{
    BYTE expected[VERIFIED_DEFINITION_CALL_LENGTH];

    expected[0] = 0xB9;
    ZeroMemory(expected + 1u, 4u);
    copy_bytes(
        expected + 5u,
        g_build_profile->definition_call_tail,
        sizeof(g_build_profile->definition_call_tail));

    if (!image_rva_range_is_readable(
            image_base, image_size, VERIFIED_DEFINITION_CALL_RVA,
            sizeof(expected))) {
        return 0;
    }
    write_u32(
        expected + 1u,
        (DWORD)(image_base + VERIFIED_DEFINITION_MANAGER_RVA));
    return bytes_are_equal(
        image_base + VERIFIED_DEFINITION_CALL_RVA,
        expected, sizeof(expected));
}

static const char *status_text(DWORD status)
{
    switch (status) {
    case STATUS_PATCH_APPLIED:
        return "PATCH_APPLIED";
    case STATUS_CODE_ALREADY_PATCHED:
        return "CODE_ALREADY_PATCHED_BEFORE_INITIALIZATION";
    case STATUS_ALREADY_ACTIVE:
        return "REQUESTED_CAPACITY_ALREADY_ACTIVE";
    case STATUS_NO_CHANGE_REQUESTED:
        return "NO_CHANGE_REQUESTED";
    case STATUS_BAD_MAIN_MODULE:
        return "BAD_MAIN_MODULE";
    case STATUS_BAD_PE_IMAGE:
        return "BAD_PE_IMAGE";
    case STATUS_UNSUPPORTED_MACHINE:
        return "UNSUPPORTED_MACHINE";
    case STATUS_INVALID_CONFIG:
        return "INVALID_CONFIG";
    case STATUS_SIGNATURE_NOT_FOUND:
        return "SIGNATURE_NOT_FOUND";
    case STATUS_SIGNATURE_AMBIGUOUS:
        return "SIGNATURE_AMBIGUOUS";
    case STATUS_BAD_MANAGER_REFERENCE:
        return "BAD_MANAGER_REFERENCE";
    case STATUS_TOO_LATE:
        return "TOO_LATE_MANAGER_ALREADY_INITIALIZED";
    case STATUS_UNEXPECTED_CAPACITY:
        return "UNEXPECTED_EXISTING_CAPACITY";
    case STATUS_VIRTUAL_PROTECT_FAILED:
        return "VIRTUAL_PROTECT_FAILED";
    case STATUS_WRITE_VERIFY_FAILED:
        return "WRITE_VERIFY_FAILED";
    case STATUS_FLUSH_FAILED_REVERTED:
        return "FLUSH_FAILED_PATCH_REVERTED";
    case STATUS_PROTECTION_RESTORE_FAILED:
        return "PROTECTION_RESTORE_FAILED_PATCH_REVERTED";
    case STATUS_DISTANCE_INIT_SIGNATURE_NOT_FOUND:
        return "DISTANCE_INIT_SIGNATURE_NOT_FOUND";
    case STATUS_DISTANCE_INIT_SIGNATURE_AMBIGUOUS:
        return "DISTANCE_INIT_SIGNATURE_AMBIGUOUS";
    case STATUS_DISTANCE_UPDATE_SIGNATURE_NOT_FOUND:
        return "DISTANCE_UPDATE_SIGNATURE_NOT_FOUND";
    case STATUS_DISTANCE_UPDATE_SIGNATURE_AMBIGUOUS:
        return "DISTANCE_UPDATE_SIGNATURE_AMBIGUOUS";
    case STATUS_DISTANCE_SOURCE_VALIDATION_FAILED:
        return "DISTANCE_SOURCE_VALIDATION_FAILED";
    case STATUS_DISTANCE_VIRTUAL_PROTECT_FAILED:
        return "DISTANCE_VIRTUAL_PROTECT_FAILED_CAPACITY_REVERTED";
    case STATUS_DISTANCE_WRITE_VERIFY_FAILED:
        return "DISTANCE_WRITE_VERIFY_FAILED_PATCHES_REVERTED";
    case STATUS_DISTANCE_FLUSH_FAILED_REVERTED:
        return "DISTANCE_FLUSH_FAILED_PATCHES_REVERTED";
    case STATUS_DISTANCE_PROTECTION_RESTORE_FAILED:
        return "DISTANCE_PROTECTION_RESTORE_FAILED_PATCHES_REVERTED";
    case STATUS_CAPACITY_ROLLBACK_FAILED:
        return "CAPACITY_ROLLBACK_FAILED";
    case STATUS_PROVIDER_UNSUPPORTED_BUILD:
        return "PROVIDER_LAYOUT_MISMATCH";
    case STATUS_PROVIDER_CODE_MISMATCH:
        return "PROVIDER_PATCH_SITE_MISMATCH";
    case STATUS_PROVIDER_ALLOCATION_FAILED:
        return "PROVIDER_STORAGE_ALLOCATION_FAILED";
    case STATUS_PROVIDER_VIRTUAL_PROTECT_FAILED:
        return "PROVIDER_VIRTUAL_PROTECT_FAILED";
    case STATUS_PROVIDER_WRITE_VERIFY_FAILED:
        return "PROVIDER_WRITE_VERIFY_FAILED_PATCHES_REVERTED";
    case STATUS_PROVIDER_FLUSH_FAILED_REVERTED:
        return "PROVIDER_FLUSH_FAILED_PATCHES_REVERTED";
    case STATUS_PROVIDER_PROTECTION_RESTORE_FAILED:
        return "PROVIDER_PROTECTION_RESTORE_FAILED_PATCHES_REVERTED";
    case STATUS_PROVIDER_ROLLBACK_FAILED:
        return "PROVIDER_OR_PRIOR_PATCH_ROLLBACK_FAILED";
    case STATUS_DEFINITION_UNSUPPORTED_BUILD:
        return "DEFINITION_MANAGER_CODE_MISMATCH";
    case STATUS_PROVIDER_INITIALIZATION_VERIFY_FAILED:
        return "PROVIDER_FREE_LIST_INITIALIZATION_VERIFY_FAILED";
    case STATUS_GENERATOR_UNSUPPORTED_BUILD:
        return "GENERATOR_LAYOUT_MISMATCH";
    case STATUS_GENERATOR_CODE_MISMATCH:
        return "GENERATOR_PATCH_SITE_MISMATCH";
    case STATUS_GENERATOR_ALLOCATION_FAILED:
        return "GENERATOR_EXTRA_RECORD_ALLOCATION_FAILED";
    case STATUS_GENERATOR_VIRTUAL_PROTECT_FAILED:
        return "GENERATOR_VIRTUAL_PROTECT_FAILED";
    case STATUS_GENERATOR_WRITE_VERIFY_FAILED:
        return "GENERATOR_WRITE_VERIFY_FAILED_PATCHES_REVERTED";
    case STATUS_GENERATOR_FLUSH_FAILED_REVERTED:
        return "GENERATOR_FLUSH_FAILED_PATCHES_REVERTED";
    case STATUS_GENERATOR_PROTECTION_RESTORE_FAILED:
        return "GENERATOR_PROTECTION_RESTORE_FAILED_PATCHES_REVERTED";
    case STATUS_GENERATOR_ROLLBACK_FAILED:
        return "GENERATOR_OR_PRIOR_PATCH_ROLLBACK_FAILED";
    case STATUS_REQUIRED_LAYOUT_OUT_OF_RANGE:
        return "REQUIRED_LAYOUT_OUTSIDE_EXE_IMAGE";
    case STATUS_UNSUPPORTED_BUILD_PROFILE:
        return "UNSUPPORTED_BUILD_PROFILE";
    case STATUS_EXECUTABLE_HASH_READ_FAILED:
        return "EXECUTABLE_SHA256_READ_FAILED";
    case STATUS_UNREGISTERED_EXECUTABLE_HASH:
        return "UNREGISTERED_EXECUTABLE_SHA256";
    case STATUS_EXECUTABLE_IDENTITY_MISMATCH:
        return "EXECUTABLE_HASH_PROFILE_INTEGRITY_MISMATCH";
    case STATUS_GENERATOR_INITIALIZED_BEFORE_PATCH:
        return "GENERATOR_INITIALIZED_BEFORE_STAGING_PATCH";
    case STATUS_UNIVERSAL_BEHAVIOR_PROFILE_MISSING:
        return "UNIVERSAL_BEHAVIOR_EXACT_PROFILE_MISSING";
    case STATUS_UNIVERSAL_BEHAVIOR_INSTALL_FAILED:
        return "UNIVERSAL_BEHAVIOR_TRANSACTION_INSTALL_FAILED";
    case STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED:
        return "UNIVERSAL_BEHAVIOR_OR_PRIOR_PATCH_ROLLBACK_FAILED";
    case STATUS_BEHAVIOR_AXIS_ISOLATION_REFUSED:
        return "BEHAVIOR_AXIS_ISOLATION_REFUSED_MULTIPLE_NON_NEUTRAL_AXES";
    case STATUS_ASI_IDENTITY_READ_FAILED:
        return "ASI_SHA256_OR_IDENTITY_LOCK_FAILED";
    case STATUS_IDENTITY_REVALIDATION_FAILED:
        return "ASI_OR_EXECUTABLE_IDENTITY_CHANGED_BEFORE_FIRST_WRITE";
    case STATUS_STARTUP_WORKER_PIN_FAILED:
        return "STARTUP_WORKER_MODULE_PIN_FAILED";
    case STATUS_INI_IDENTITY_READ_FAILED:
        return "INI_SHA256_OR_IDENTITY_LOCK_FAILED";
    case STATUS_DEFINITION_LOAD_ALREADY_STARTED:
        return "DEFINITION_LOAD_ALREADY_STARTED_BEFORE_HOOK";
    case STATUS_DEFINITION_CALLBACK_BEFORE_ACTIVATION:
        return "DEFINITION_CALLBACK_RAN_BEFORE_TRANSACTION_ACTIVATION";
    case STATUS_DEFINITION_CALLBACK_MISSED:
        return "DEFINITION_LOAD_COMPLETED_WITHOUT_INSTALLED_CALLBACK";
    case STATUS_STARTUP_QUIESCENCE_FAILED:
        return "STARTUP_THREAD_QUIESCENCE_FAILED_BEFORE_WRITE";
    case STATUS_STARTUP_QUIESCENCE_IN_FLIGHT:
        return "STARTUP_THREAD_IN_OR_RETURNING_TO_PATCHED_CODE";
    case STATUS_STARTUP_QUIESCENCE_RESUME_FAILED:
        return "STARTUP_THREAD_RESUME_FAILED_PROCESS_TERMINATED";
    case STATUS_COMPATIBILITY_PROBE_DECLINED:
        return "UNRECOGNIZED_EXE_PROBE_DECLINED_MOD_DISABLED";
    case STATUS_COMPATIBILITY_PROBE_NO_UNIQUE_FAMILY:
        return "UNRECOGNIZED_EXE_NO_UNIQUE_COMPLETE_PROFILE_MOD_DISABLED";
    default:
        return "NOT_RUN_OR_UNKNOWN";
    }
}

static const char *definition_status_text(DWORD status)
{
    switch (status) {
    case DEFINITION_SCALING_WAITING:
        return "WAITING_FOR_PROCEDURAL_DAT";
    case DEFINITION_SCALING_APPLIED:
        return "DEFINITION_SCALING_APPLIED";
    case DEFINITION_SCALING_NO_CHANGE:
        return "DEFINITION_SCALING_NO_CHANGE";
    case DEFINITION_SCALING_ROLLED_BACK_SAFETY:
        return "DEFINITION_SCALING_ROLLED_BACK_BY_SAFETY_GOVERNOR";
    case DEFINITION_SCALING_BAD_MANAGER:
        return "DEFINITION_MANAGER_INVALID";
    case DEFINITION_SCALING_BAD_COUNTS:
        return "DEFINITION_COUNTS_INVALID";
    case DEFINITION_SCALING_BAD_FLOAT:
        return "DEFINITION_FLOAT_INVALID";
    case DEFINITION_SCALING_WRITE_VERIFY_FAILED:
        return "DEFINITION_WRITE_VERIFY_FAILED";
    case DEFINITION_SCALING_ROLLBACK_FAILED:
        return "DEFINITION_SAFETY_ROLLBACK_FAILED";
    case DEFINITION_SCALING_FINGERPRINT_REFUSED:
        return "DEFINITION_STOCK_CATALOG_FINGERPRINT_REFUSED";
    case DEFINITION_SCALING_CALLBACK_BEFORE_ACTIVATION:
        return "DEFINITION_CALLBACK_BEFORE_TRANSACTION_ACTIVATION";
    case DEFINITION_SCALING_CALLBACK_MISSED:
        return "DEFINITION_LOAD_COMPLETED_WITHOUT_CALLBACK";
    default:
        return "DEFINITION_STATUS_UNKNOWN";
    }
}

static const char *density_catalog_text(DWORD selection)
{
    switch (selection) {
    case DENSITY_CATALOG_STOCK_BASE:
        return "STOCK_BASE_179";
    case DENSITY_CATALOG_STOCK_EXPANDED:
        return "STOCK_EXPANDED_209";
    default:
        return "NONE_REFUSED";
    }
}

static const char *density_class_text(BYTE density_class)
{
    switch (density_class) {
    case DENSITY_CLASS_GRASS:
        return "grass";
    case DENSITY_CLASS_VEGETATION:
        return "vegetation";
    case DENSITY_CLASS_CLUTTER:
        return "clutter";
    case DENSITY_CLASS_OTHER:
        return "other";
    default:
        return "unknown";
    }
}

static const char *model_distance_status_text(DWORD status)
{
    switch (status) {
    case MODEL_DISTANCE_WAITING:
        return "WAITING_FOR_PROCEDURAL_DAT";
    case MODEL_DISTANCE_APPLIED:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_APPLIED";
    case MODEL_DISTANCE_NO_CHANGE:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_NO_CHANGE";
    case MODEL_DISTANCE_ROLLED_BACK_SAFETY:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_ROLLED_BACK_BY_SAFETY_GOVERNOR";
    case MODEL_DISTANCE_BAD_DEFINITION_MANAGER:
        return "MODEL_DISTANCE_DEFINITION_MANAGER_INVALID";
    case MODEL_DISTANCE_BAD_DEFINITION_COUNT:
        return "MODEL_DISTANCE_DEFINITION_COUNT_INVALID";
    case MODEL_DISTANCE_BAD_MODEL_TABLE:
        return "MODEL_INFO_TABLE_INVALID";
    case MODEL_DISTANCE_BAD_MODEL_INDEX:
        return "PROCEDURAL_MODEL_INDEX_INVALID";
    case MODEL_DISTANCE_BAD_MODEL_POINTER:
        return "PROCEDURAL_MODEL_POINTER_INVALID_OR_READ_ONLY";
    case MODEL_DISTANCE_BAD_DRAW_DISTANCE:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_INVALID";
    case MODEL_DISTANCE_WRITE_VERIFY_FAILED:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_WRITE_VERIFY_FAILED";
    case MODEL_DISTANCE_ROLLBACK_FAILED:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_ROLLBACK_FAILED";
    case MODEL_DISTANCE_BAD_QUERY_RADIUS:
        return "PROCEDURAL_QUERY_RADIUS_INVALID";
    case MODEL_DISTANCE_SAFETY_ROLLBACK_FAILED:
        return "PROCEDURAL_MODEL_DRAW_DISTANCE_SAFETY_ROLLBACK_FAILED";
    case MODEL_DISTANCE_RETIRED_GLOBAL_MUTATION:
        return "MODEL_GLOBAL_DRAW_DISTANCE_MUTATION_RETIRED";
    default:
        return "MODEL_DISTANCE_STATUS_UNKNOWN";
    }
}

static void log_append(const char *text)
{
    while (text && *text && g_log_length + 1 < sizeof(g_log_buffer)) {
        g_log_buffer[g_log_length++] = *text++;
    }
    g_log_buffer[g_log_length] = '\0';
}

static void log_append_u32(DWORD value)
{
    char digits[16];
    DWORD count = 0;
    if (value == 0) {
        log_append("0");
        return;
    }
    while (value != 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count > 0) {
        char pair[2];
        pair[0] = digits[--count];
        pair[1] = '\0';
        log_append(pair);
    }
}

static void log_append_float_3(float value)
{
    DWORD whole;
    DWORD fraction;

    if (value < 0.0f) {
        log_append("-");
        value = -value;
    }
    if (value > 1000000.0f) {
        log_append("(out-of-range)");
        return;
    }

    whole = (DWORD)value;
    fraction = (DWORD)((value - (float)whole) * 1000.0f + 0.5f);
    if (fraction >= 1000u) {
        ++whole;
        fraction = 0;
    }

    log_append_u32(whole);
    log_append(".");
    if (fraction < 100u) log_append("0");
    if (fraction < 10u) log_append("0");
    log_append_u32(fraction);
}

static void log_append_hex32(DWORD value)
{
    static const char hex[] = "0123456789ABCDEF";
    char output[11];
    int index;
    output[0] = '0';
    output[1] = 'x';
    for (index = 0; index < 8; ++index) {
        output[2 + index] = hex[(value >> ((7 - index) * 4)) & 0xFu];
    }
    output[10] = '\0';
    log_append(output);
}

static void log_append_hex_bytes(const BYTE *bytes, DWORD length)
{
    static const char hex[] = "0123456789ABCDEF";
    DWORD index;
    for (index = 0; index < length; ++index) {
        char output[4];
        output[0] = hex[(bytes[index] >> 4) & 0xFu];
        output[1] = hex[bytes[index] & 0xFu];
        output[2] = index + 1u == length ? '\0' : ' ';
        output[3] = '\0';
        log_append(output);
    }
}

static void log_key_text(const char *key, const char *value)
{
    log_append(key);
    log_append(": ");
    log_append(value ? value : "");
    log_append("\r\n");
}

static void log_key_u32(const char *key, DWORD value)
{
    log_append(key);
    log_append(": ");
    log_append_u32(value);
    log_append("\r\n");
}

static void log_key_hex(const char *key, DWORD value)
{
    log_append(key);
    log_append(": ");
    log_append_hex32(value);
    log_append("\r\n");
}

static void log_key_float(const char *key, float value)
{
    log_append(key);
    log_append(": ");
    log_append_float_3(value);
    log_append("\r\n");
}

static void log_loaded_module_probe(const char *name)
{
    HMODULE module = GetModuleHandleA(name);
    char path[MAX_PATH];

    log_append("Module ");
    log_append(name);
    log_append(": ");
    if (!module) {
        log_append("NOT LOADED WHEN THIS ASI RAN\r\n");
        return;
    }

    path[0] = '\0';
    GetModuleFileNameA(module, path, MAX_PATH);
    log_append("LOADED base=");
    log_append_hex32((DWORD)module);
    log_append(" path=");
    log_append(path);
    log_append("\r\n");
}

static void log_known_module_load_order(void)
{
    log_append("\r\n[MODULES PRESENT WHEN PATCH VALIDATION RAN]\r\n");
    log_loaded_module_probe("GTAIV.EFLC.FusionFix.asi");
    log_loaded_module_probe("aCompleteEditionHook.asi");
    log_loaded_module_probe("AdvancedHookInit.asi");
    log_loaded_module_probe("ScriptHookDotNet.asi");
    log_loaded_module_probe("Liberty's Legacy.asi");
    log_loaded_module_probe("LibertyLoadout.asi");
    log_loaded_module_probe("VolumetricLights.asi");
    log_loaded_module_probe("WeaponAnimations.asi");
    log_loaded_module_probe("FirstPerson.asi");
}

static void build_log(void)
{
    SYSTEMTIME utc;
    struct GuSourcePrepareDiagnostic source_prepare;
    DWORD index;
    g_log_length = 0;
    g_log_buffer[0] = '\0';
    GetSystemTime(&utc);
    ZeroMemory(&source_prepare, sizeof(source_prepare));
    gu_source_bank_get_prepare_diagnostic(&source_prepare);

    log_append("GTAIV Grass & Procedural Props Fix By : iammrmikeman\r\n");
    log_append(PLUGIN_NAME " " PLUGIN_VERSION "\r\n");
    log_append("UTC: ");
    log_append_u32((DWORD)utc.wYear);
    log_append("-");
    if (utc.wMonth < 10) log_append("0");
    log_append_u32((DWORD)utc.wMonth);
    log_append("-");
    if (utc.wDay < 10) log_append("0");
    log_append_u32((DWORD)utc.wDay);
    log_append(" ");
    if (utc.wHour < 10) log_append("0");
    log_append_u32((DWORD)utc.wHour);
    log_append(":");
    if (utc.wMinute < 10) log_append("0");
    log_append_u32((DWORD)utc.wMinute);
    log_append(":");
    if (utc.wSecond < 10) log_append("0");
    log_append_u32((DWORD)utc.wSecond);
    log_append("\r\n\r\n");

    log_key_text("ASI path", g_self_path);
    log_key_u32(
        "Startup worker running outside DllMain",
        g_report.startup_worker_started);
    log_key_u32("Loader initialization callbacks",
        (DWORD)InterlockedCompareExchange(&g_startup_loader_callback_calls, 0, 0));
    log_key_u32("Loader initialization callbacks safely deferred",
        (DWORD)InterlockedCompareExchange(&g_startup_loader_callback_deferred, 0, 0));
    log_key_u32("Loader initialization wait timeouts",
        (DWORD)InterlockedCompareExchange(&g_startup_loader_callback_timeouts, 0, 0));
    log_key_text(
        "ASI SHA-256",
        g_report.asi_hash_valid ?
            g_self_sha256 : "UNAVAILABLE");
    log_key_u32("ASI file size", g_report.asi_file_size);
    log_key_u32(
        "ASI retained read identity lock held",
        g_report.asi_identity_lock_held);
    log_key_hex(
        "ASI file identity volume serial",
        g_self_file_identity.snapshot.dwVolumeSerialNumber);
    log_key_hex(
        "ASI file identity index high",
        g_self_file_identity.snapshot.nFileIndexHigh);
    log_key_hex(
        "ASI file identity index low",
        g_self_file_identity.snapshot.nFileIndexLow);
    log_key_hex(
        "ASI file identity last-write high",
        g_self_file_identity.snapshot.ftLastWriteTime.dwHighDateTime);
    log_key_hex(
        "ASI file identity last-write low",
        g_self_file_identity.snapshot.ftLastWriteTime.dwLowDateTime);
    log_key_text("Main module", g_main_path);
    log_key_text(
        "Main executable SHA-256",
        g_report.executable_hash_valid ?
            g_main_sha256 : "UNAVAILABLE");
    log_key_u32(
        "Main executable file size",
        g_report.executable_file_size);
    log_key_u32(
        "Main executable retained read identity lock held",
        g_report.executable_identity_lock_held);
    log_key_hex(
        "Main executable file identity volume serial",
        g_main_file_identity.snapshot.dwVolumeSerialNumber);
    log_key_hex(
        "Main executable file identity index high",
        g_main_file_identity.snapshot.nFileIndexHigh);
    log_key_hex(
        "Main executable file identity index low",
        g_main_file_identity.snapshot.nFileIndexLow);
    log_key_hex(
        "Main executable file identity last-write high",
        g_main_file_identity.snapshot.ftLastWriteTime.dwHighDateTime);
    log_key_hex(
        "Main executable file identity last-write low",
        g_main_file_identity.snapshot.ftLastWriteTime.dwLowDateTime);
    log_key_u32(
        "ASI, INI, and executable revalidated before first image write",
        g_report.identity_revalidation_passed);
    log_key_u32(
        "Startup thread quiescence attempted",
        g_report.startup_quiescence_attempted);
    log_key_u32(
        "Startup native thread enumeration passes",
        g_report.startup_quiescence_snapshots);
    log_key_u32(
        "Startup thread suspensions owned",
        g_report.startup_quiescence_threads_suspended);
    log_key_u32(
        "Startup exact-call executable-identity ID",
        g_report.startup_call_contract_identity_id);
    log_key_u32(
        "Startup exact return RVAs selected",
        g_report.startup_call_contract_record_count);
    log_key_u32(
        "Startup exact return RVAs sealed by interval authority",
        g_report.startup_call_contract_preimages_verified);
    log_key_hex(
        "Startup guarded-interval mismatch address",
        g_report.startup_call_contract_mismatch_address);
    log_key_u32(
        "Startup guarded intervals selected",
        g_report.startup_guard_interval_count);
    log_key_u32(
        "Startup guarded-interval stopped-world validation passes",
        g_report.startup_guard_interval_validation_passes);
    log_key_u32(
        "Startup guarded-interval bytes verified per pass",
        g_report.startup_guard_interval_bytes_verified);
    log_key_u32(
        "Startup guarded-interval HIGHLOW relocations verified per pass",
        g_report.startup_guard_interval_relocations_verified);
    log_key_hex(
        "Startup definition guard first RVA",
        g_report.startup_definition_guard_first_rva);
    log_key_hex(
        "Startup definition guard last RVA exclusive",
        g_report.startup_definition_guard_last_rva);
    log_key_hex(
        "Startup plant-distance setter guard first RVA",
        g_report.startup_plant_setter_guard_first_rva);
    log_key_hex(
        "Startup plant-distance setter guard last RVA exclusive",
        g_report.startup_plant_setter_guard_last_rva);
    log_key_hex(
        "Startup unsafe EIP or preimage address",
        g_report.startup_quiescence_unsafe_eip);
    log_key_hex(
        "Startup unsafe stack return",
        g_report.startup_quiescence_unsafe_stack_return);
    log_key_u32(
        "Startup quiescence failure kind",
        g_report.startup_quiescence_failure_kind);
    log_key_u32(
        "Startup nonzero manager region (1=surface/provider,2=definition,3=generator)",
        g_report.startup_nonzero_manager_region);
    log_key_u32("Startup nonzero manager checked bytes",
        g_report.startup_nonzero_manager_size);
    log_key_hex("Startup nonzero manager first byte offset",
        g_report.startup_nonzero_manager_offset);
    log_key_hex("Startup nonzero manager first byte value",
        g_report.startup_nonzero_manager_byte);
    log_key_hex(
        "Startup quiescence last error/status",
        g_report.startup_quiescence_last_error);
    log_key_u32(
        "Startup thread resume failures",
        g_report.startup_quiescence_resume_failures);
    log_key_u32(
        "Startup quiescence completed and released",
        g_report.startup_quiescence_completed);
    log_key_u32(
        "Thread-attach gate waits",
        g_report.startup_thread_attach_gate_waits);
    log_key_u32(
        "Unrecognized-EXE compatibility probe prompted",
        g_report.compatibility_probe_prompted);
    log_key_u32(
        "Unrecognized-EXE compatibility probe authorized",
        g_report.compatibility_probe_authorized);
    log_key_u32(
        "Compatibility-probe matching exact identities",
        g_report.compatibility_probe_identity_matches);
    log_key_u32(
        "Compatibility-probe matching profile families",
        g_report.compatibility_probe_family_matches);
    log_key_u32(
        "Compatibility-probe donor identity ID",
        g_report.compatibility_probe_selected_identity_id);
    log_key_u32(
        "Compatibility-probe selected profile ID",
        g_report.compatibility_probe_selected_profile_id);
    log_key_u32(
        "Compatibility-probe safe game continuation",
        g_report.compatibility_probe_safe_continue);
    log_key_u32(
        "Selected executable-identity ID",
        g_report.executable_identity_id);
    log_key_text(
        "Selected executable identity",
        g_report.executable_identity_id != 0u &&
                g_executable_identity ?
            g_executable_identity->evidence_label :
            "NONE - exact SHA-256 is not registered");
    log_key_text(
        "Compatibility-probe donor identity",
        g_report.compatibility_probe_selected_identity_id != 0u &&
                g_executable_identity ?
            g_executable_identity->evidence_label : "NONE");
    log_key_text("INI path", g_ini_path);
    log_key_text(
        "INI SHA-256",
        g_report.ini_hash_valid ?
            g_ini_sha256 : "UNAVAILABLE");
    log_key_u32("INI file size", g_report.ini_file_size);
    log_key_u32(
        "INI retained read identity lock held",
        g_report.ini_identity_lock_held);
    log_key_hex(
        "INI file identity volume serial",
        g_ini_file_identity.snapshot.dwVolumeSerialNumber);
    log_key_hex(
        "INI file identity index high",
        g_ini_file_identity.snapshot.nFileIndexHigh);
    log_key_hex(
        "INI file identity index low",
        g_ini_file_identity.snapshot.nFileIndexLow);
    log_key_hex(
        "INI file identity last-write high",
        g_ini_file_identity.snapshot.ftLastWriteTime.dwHighDateTime);
    log_key_hex(
        "INI file identity last-write low",
        g_ini_file_identity.snapshot.ftLastWriteTime.dwLowDateTime);
    log_key_u32("Status code", g_report.status);
    log_key_text("Status", status_text(g_report.status));
    log_key_u32("Selected build-profile ID", g_report.build_profile_id);
    log_key_text(
        "Selected build profile",
        g_report.build_profile_id != 0u ?
            g_build_profile->name :
            "NONE - executable identity is not registered");
    log_key_u32("Requested capacity", g_report.requested_capacity);
    log_key_u32(
        "Capacity selected by auto profile",
        g_report.capacity_auto_selected);
    log_key_u32(
        "Requested rendered-object capacity",
        g_report.requested_rendered_object_capacity);
    log_key_u32(
        "Rendered-object capacity selected by auto profile",
        g_report.rendered_capacity_auto_selected);
    log_key_u32(
        "Effective rendered-object capacity",
        g_report.effective_rendered_object_capacity);
    log_key_u32(
        "Generator expansion enabled for selected profile",
        g_report.generator_expansion_enabled_for_profile);
    log_key_u32(
        "Requested provider capacity",
        g_report.requested_provider_capacity);
    log_key_u32(
        "Provider capacity selected by auto profile",
        g_report.provider_capacity_auto_selected);
    log_key_float(
        "Corrected procedural distance multiplier",
        read_float((BYTE *)&
            g_report.corrected_distance_multiplier_bits));
    log_key_hex(
        "Corrected procedural distance multiplier bits",
        g_report.corrected_distance_multiplier_bits);
    log_key_u32(
        "Retired legacy distance multiplier state",
        g_report.distance_multiplier);
    log_key_u32(
        "Universal behavior exact profile found",
        g_report.universal_behavior_profile_found);
    log_key_u32(
        "Universal behavior transaction installed",
        g_report.universal_behavior_installed);
    log_key_u32(
        "Exact PROCOBJ commit hook installed",
        g_report.universal_behavior_installed);
    log_key_u32(
        "Internal pre-destroy release observer installed",
        g_report.universal_behavior_installed);
    log_key_u32(
        "Whole-function release entry detour installed", 0u);
    log_key_u32(
        "Universal behavior failure stage",
        g_report.universal_behavior_failure_stage);
    log_key_u32(
        "Universal install lifecycle state",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_state, 0, 0));
    log_key_u32(
        "Universal install lifecycle rejections",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_rejections, 0, 0));
    log_key_u32(
        "Universal source prepare substage",
        source_prepare.stage);
    log_key_text(
        "Universal source prepare substage name",
        gu_source_prepare_stage_name(source_prepare.stage));
    log_key_hex(
        "Universal source TLS index",
        source_prepare.tls_index);
    log_key_u32(
        "Universal source prepare Win32 error",
        source_prepare.win32_error);
    log_key_u32(
        "Universal source prepare site kind",
        source_prepare.site_kind);
    log_key_text(
        "Universal source prepare site name",
        gu_source_prepare_site_name(source_prepare.site_kind));
    log_key_u32(
        "Universal source prepare site index",
        source_prepare.site_index);
    log_key_hex(
        "Universal source prepare site RVA",
        source_prepare.site_rva);
    log_key_u32(
        "Universal source prepare site length",
        source_prepare.site_length);
    log_key_u32(
        "Universal source prepare first mismatch byte",
        source_prepare.first_mismatch_offset);
    log_key_hex(
        "Universal source prepare site rebase delta",
        source_prepare.image_rebase_delta);
    log_key_u32(
        "Universal source transaction count before",
        source_prepare.transaction_count_before);
    log_key_u32(
        "Universal source transaction count at failure",
        source_prepare.transaction_count_partial);
    log_key_u32(
        "Universal source transaction count after cleanup",
        source_prepare.transaction_count_after);
    log_key_u32(
        "Universal source transaction committed",
        source_prepare.transaction_committed);
    if (source_prepare.site_length != 0u &&
        source_prepare.site_length <= GU_PATCH_MAX_BYTES) {
        log_append("Universal source expected live bytes: ");
        log_append_hex_bytes(
            source_prepare.expected, source_prepare.site_length);
        log_append("\r\nUniversal source observed live bytes: ");
        log_append_hex_bytes(
            source_prepare.observed, source_prepare.site_length);
        log_append("\r\n");
    }
    log_key_u32(
        "Universal behavior rollback verified",
        g_report.universal_behavior_rollback_verified);
    log_key_float("Configured PlantDensityMultiplier (v1.2 scale)",
        g_configured_plant_density_multiplier);
    log_key_float("Configured ProceduralObjectDensityMultiplier (v1.2 scale)",
        g_configured_procobj_density_multiplier);
    log_key_float("Density baseline scale", PUBLIC_DENSITY_BASELINE_SCALE);
    log_key_float(
        "Corrected PLANT visible-density multiplier",
        read_float((BYTE *)&
            g_report.corrected_plant_density_multiplier_bits));
    log_key_hex(
        "Corrected PLANT visible-density multiplier bits",
        g_report.corrected_plant_density_multiplier_bits);
    log_key_u32(
        "Retired legacy PLANT definition multiplier state",
        g_report.plant_density_multiplier);
    log_key_float(
        "Procedural-object density multiplier",
        read_float((BYTE *)&
            g_report.procobj_density_multiplier_bits));
    log_key_hex(
        "Procedural-object density multiplier bits",
        g_report.procobj_density_multiplier_bits);
    log_key_u32(
        "Non-neutral behavior axes requested",
        behavior_axis_non_neutral_count(
            g_report.corrected_distance_multiplier_bits,
            g_report.corrected_plant_density_multiplier_bits,
            g_report.procobj_density_multiplier_bits));
    log_key_u32(
        "Density class mask",
        g_report.density_class_mask);
    log_append(
        "DensityClassMask bits: grass=1, vegetation=2, clutter=4, "
        "other=8. Default=3 (grass+vegetation).\r\n");
    log_key_u32(
        "PLANT positive output-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_positive_calls, 0, 0));
    log_key_u32(
        "PLANT neutral or unknown output-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_neutral_calls, 0, 0));
    log_key_hex(
        "PLANT last positive output-model key",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_last_output_key, 0, 0));
    log_key_hex(
        "PLANT last positive source-model hash",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_last_source_hash, 0, 0));
    log_key_u32(
        "PLANT last positive submesh",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_last_submesh, 0, 0));
    log_key_u32("Debug log enabled", g_report.debug_log_enabled);
    log_key_u32("Hang watchdog enabled", g_report.hang_watchdog_enabled);
    log_key_u32("Hang detection threshold seconds", g_report.hang_timeout_seconds);
    log_key_u32("Hang message box enabled", g_report.hang_message_box_enabled);
    log_key_u32("Full hang dump opt-in enabled", g_report.full_hang_dump_enabled);
    log_key_hex("Default compact hang dump flags", MINIDUMP_COMPACT_TYPE);
    log_key_u32("Compact diagnostic upload budget bytes", 25u * 1024u * 1024u);
    if (g_report.status == STATUS_INVALID_CONFIG) {
        log_key_text("Invalid INI key", g_config_error_key);
        log_key_text("Invalid INI value", g_config_error_value);
        log_key_u32("INI error kind", g_config_error_kind);
        if (g_config_error_float_range) {
            log_key_text(
                "INI allowed minimum", g_config_error_min_text);
            log_key_text(
                "INI allowed maximum", g_config_error_max_text);
        } else {
            log_key_u32("INI allowed minimum", g_config_error_min);
            log_key_u32("INI allowed maximum", g_config_error_max);
        }
    }
    log_append("\r\n[CUMULATIVE MULTIBUILD PROFILE]\r\n");
    log_append(
        "Selection rule: exact on-disk GTAIV.exe SHA-256 first; PE "
        "timestamp/image size and all instruction bytes are mandatory "
        "secondary checks. Unknown hashes are never guessed.\r\n");
    log_append(
        "Pool values may be literal 'auto' or strict manual integers. Auto keeps "
        "the proven 40,960 surface pool, selects 4,096-32,768 generated wrappers "
        "from distance-squared times density pressure, and keeps GTA IV's vanilla "
        "40-provider storage. DistanceMultiplier coherently scales procedural "
        "manager residency/coverage, the 15-120m source-query radius with a "
        "32,768-pointer mod bank, and only live wrapper-owned PROCOBJ instance "
        "visibility. The shared model-global draw-distance mutation stays retired.\r\n");
    log_key_text(
        "Density safety",
        "PROCOBJ STOCK-FINGERPRINT + CLASS GATE; PLANT EXACT OUTPUT-KEY ALLOWLIST; PLANT 64->128 ATOMIC WORKSPACE");
    log_key_text(
        "Distance safety",
        "EXACT-HASH ALL-OR-NONE MANAGER + SOURCE QUERY + GENERATED-INSTANCE TRANSACTION; MODEL-GLOBAL MUTATION DISABLED");
    log_key_text(
        "Distance persistence correction",
        "BLD0035 FALSE 512-WRAPPER GOVERNOR TRIGGER REMOVED");
    log_append("\r\n");
    log_key_u32("Signature matches", g_report.signature_matches);
    log_key_u32("Distance init signature matches",
                g_report.distance_init_matches);
    log_key_u32("Distance update signature matches",
                g_report.distance_update_matches);
    log_key_hex("Target RVA", g_report.target_rva);
    log_key_hex("Distance init RVA", g_report.distance_init_rva);
    log_key_hex("Distance update RVA", g_report.distance_update_rva);
    log_key_u32("Original instruction capacity", g_report.original_immediate);
    log_key_u32("Final instruction capacity", g_report.final_immediate);
    log_key_hex("Manager RVA", g_report.manager_rva);
    log_key_u32("Manager capacity at load", g_report.manager_capacity);
    log_key_hex("Manager buffer 0 at load", g_report.manager_buffer0);
    log_key_hex("Manager buffer 1 at load", g_report.manager_buffer1);
    log_key_hex("Manager buffer 2 at load", g_report.manager_buffer2);
    log_key_u32(
        "Provider free-list head at ASI load",
        g_report.provider_free_head_at_load);
    log_key_u32(
        "Provider active-list head at ASI load",
        g_report.provider_active_head_at_load);
    log_key_hex("PE timestamp", g_report.pe_timestamp);
    log_key_hex("Size of image", g_report.size_of_image);
    log_key_hex("Loaded GTAIV.exe image base", g_report.image_base);
    log_key_hex(
        "Loaded-header preferred image base",
        g_report.loaded_header_preferred_image_base);
    log_key_hex(
        "Preferred GTAIV.exe image base",
        g_report.preferred_image_base);
    log_key_hex(
        "GTAIV.exe rebase delta",
        g_report.image_rebase_delta);
    log_key_hex("Original page protection", g_report.old_protection);
    log_key_u32("FlushInstructionCache result", g_report.flush_result);
    log_key_u32("Protection restore result", g_report.protection_restore_result);
    log_key_u32("Scaled 20-unit terms", g_report.scaled_twenty);
    log_key_u32("Scaled 40-unit padding", g_report.scaled_forty);
    log_key_hex("Scaled 0.5 multiplier bits", g_report.scaled_half_bits);
    log_key_hex("Live Detail Distance source",
                g_report.detail_distance_source);
    log_key_hex("Original 0.5 source",
                g_report.detail_multiplier_source);
    log_key_hex("Original 20-unit source",
                g_report.distance_near_source);
    log_key_hex("Original 40-unit source",
                g_report.distance_far_source);
    log_key_hex("Distance original page protection",
                g_report.distance_old_protection);
    log_key_u32("Distance FlushInstructionCache result",
                g_report.distance_flush_result);
    log_key_u32("Distance protection restore result",
                g_report.distance_protection_restore_result);
    log_key_u32(
        "Provider patch sites verified",
        g_report.provider_patch_sites_verified);
    log_key_u32(
        "Provider patch sites applied",
        g_report.provider_patch_sites_applied);
    log_key_u32(
        "Provider patch site mismatches",
        g_report.provider_patch_site_mismatches);
    log_key_u32(
        "Provider first mismatching site",
        g_report.provider_first_mismatch_site);
    log_key_u32(
        "Provider first mismatching byte offset (zero based)",
        g_report.provider_first_mismatch_byte);
    log_key_hex(
        "Provider storage allocation",
        g_report.provider_storage);
    log_key_hex(
        "Provider record base",
        g_report.provider_records);
    log_key_u32(
        "Provider storage bytes",
        g_report.provider_storage_bytes);
    log_key_u32(
        "Provider free-list initialization verified",
        g_report.provider_free_list_verified);
    log_key_hex(
        "Provider original page protection",
        g_report.provider_old_protection);
    log_key_u32(
        "Provider FlushInstructionCache result",
        g_report.provider_flush_result);
    log_key_u32(
        "Provider protection restore result",
        g_report.provider_protection_restore_result);
    log_key_hex(
        "Generator manager RVA",
        g_report.generator_manager_rva);
    log_key_u32(
        "Generator patch sites verified",
        g_report.generator_patch_sites_verified);
    log_key_u32(
        "Generator patch sites applied",
        g_report.generator_patch_sites_applied);
    log_key_u32(
        "Generator patch site mismatches",
        g_report.generator_patch_site_mismatches);
    log_key_u32(
        "Generator first mismatching site",
        g_report.generator_first_mismatch_site);
    log_key_u32(
        "Generator first mismatching byte offset (zero based)",
        g_report.generator_first_mismatch_byte);
    log_key_hex(
        "Generator external record allocation",
        g_report.generator_extra_storage);
    log_key_u32(
        "Generator extra record count",
        g_report.generator_extra_record_count);
    log_key_u32(
        "Generator external storage bytes",
        g_report.generator_extra_storage_bytes);
    log_key_u32(
        "Generator free-list count at ASI load",
        g_report.generator_original_free_count);
    log_key_u32(
        "Generator rendered count at ASI load",
        g_report.generator_original_active_rendered);
    log_key_u32(
        "Generator staging capacity at ASI load",
        g_report.generator_original_staging_capacity);
    log_key_u32(
        "Generator staging count at ASI load",
        g_report.generator_original_staging_count);
    log_key_hex(
        "Generator original page protection",
        g_report.generator_old_protection);
    log_key_u32(
        "Generator FlushInstructionCache result",
        g_report.generator_flush_result);
    log_key_u32(
        "Generator protection restore result",
        g_report.generator_protection_restore_result);
    log_key_hex(
        "Definition manager RVA",
        g_report.definition_manager_rva);
    log_key_u32(
        "Definition scaling status code",
        g_report.definition_scaling_status);
    log_key_u32(
        "Definition game-thread callback state",
        g_report.definition_callback_state);
    log_key_u32(
        "Definition game-thread callback calls",
        g_report.definition_callback_calls);
    log_key_u32(
        "Definition game-thread callback thread ID",
        g_report.definition_callback_thread_id);
    log_key_text(
        "Definition scaling status",
        definition_status_text(g_report.definition_scaling_status));
    log_key_u32(
        "Procedural-object definitions loaded",
        g_report.procobj_definition_count);
    log_key_u32(
        "Plant definitions loaded",
        g_report.plant_definition_count);
    log_key_u32(
        "Procedural-object definitions scaled",
        g_report.procobj_definitions_scaled);
    log_key_u32(
        "Plant definitions scaled",
        g_report.plant_definitions_scaled);
    log_key_text(
        "PLANT definition-field mutation",
        "DISABLED - authored +0x2C eligibility fields are observed and preserved");
    log_key_text(
        "PLANT visible-count density",
        "EXACT OUTPUT-MODEL KEY GATE; SELECTED CALLS MULTIPLIED IN THE LIVE COUNT PATH; 64-SLOT STORAGE RELOCATED TO THE PROVEN 128 PROJECTOR CEILING");
    log_key_u32(
        "Tuning governor observation-only",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    log_key_u32(
        "Automatic live definition rollback installed",
        0u);
    log_key_u32(
        "Monitor live definition writes",
        0u);
    log_key_text(
        "Monitor hazard action",
        "LATCH_RESTART_REQUIRED_AND_FAIL_CURRENT_TEST");
    log_key_u32("Unhandled-only crash dump handler installed",
                g_report.crash_handler_installed);
    log_key_u32(
        "Targeted procedural first-chance logger installed",
        g_report.targeted_exception_handler_installed);
    log_key_text(
        "Crash capture mode",
        "Unhandled exceptions get crash dumps; the independent window watchdog "
        "writes a hang dump after sustained non-response; targeted first-chance "
        "exceptions are logged without being intercepted");
    log_key_u32(
        "Existing crash dump was present before this launch",
        g_existing_crash_dump_at_start);
    log_key_u32(
        "Crash dump created during startup",
        0u);
    log_key_u32("Telemetry thread started",
                g_report.telemetry_thread_started);
    log_key_u32(
        "Background writers gated until initial log committed",
        1u);
    log_key_text("Crash dump path", g_crash_dump_path);
    log_key_text("Hang dump path", g_hang_dump_path);
    log_key_text("Immutable hang summary path", g_hang_summary_path);
    log_key_u32(
        "Existing hang dump was present before this launch",
        g_existing_hang_dump_at_start);
    log_key_u32(
        "Hang watchdog thread started",
        g_report.hang_watchdog_thread_started);
    log_key_u32("Last Win32 error", g_report.last_error);
    log_known_module_load_order();

    log_append("\r\n[ACTION RESULTS]\r\n");
    log_key_u32(
        "INI parsed and all configured values accepted",
        g_report.status != STATUS_INVALID_CONFIG);
    log_key_u32(
        "Registered executable profile selected",
        g_report.build_profile_id != 0u &&
        g_report.pe_timestamp == VERIFIED_PE_TIMESTAMP &&
        g_report.size_of_image == VERIFIED_IMAGE_SIZE);
    log_key_u32(
        "Definition-manager code verified",
        g_report.definition_code_verified);
    log_key_u32(
        "Compatibility preflight passed",
        g_report.compatibility_checks_passed);
    log_key_u32(
        "Surface-capacity signature unique",
        g_report.signature_matches == 1u);
    log_key_u32(
        "Distance-init signature unique",
        g_report.distance_init_matches == 1u);
    log_key_u32(
        "Distance-update signature unique",
        g_report.distance_update_matches == 1u);
    log_key_u32(
        "Capacity write and read-back succeeded",
        g_report.final_immediate ==
            g_report.requested_capacity);
    log_key_u32(
        "Capacity instruction cache flushed",
        g_report.requested_capacity == VANILLA_CAPACITY ||
        g_report.flush_result != 0u);
    log_key_u32(
        "Capacity page protection restored",
        g_report.requested_capacity == VANILLA_CAPACITY ||
        g_report.protection_restore_result != 0u);
    log_key_u32(
        "Distance kept vanilla or all six operands written and flushed",
        !LEGACY_DISTANCE_OPERAND_PATCH_ENABLED ||
        g_report.distance_multiplier == 1u ||
        g_report.distance_flush_result != 0u);
    log_key_u32(
        "Distance kept vanilla or page protection restored",
        !LEGACY_DISTANCE_OPERAND_PATCH_ENABLED ||
        g_report.distance_multiplier == 1u ||
        g_report.distance_protection_restore_result != 0u);
    log_key_u32(
        "Provider external storage allocated",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_storage != 0u);
    log_key_u32(
        "Provider free-list links initialized and verified",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_free_list_verified != 0u);
    log_key_u32(
        "All configured provider sites matched originals",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_patch_sites_verified ==
            PROVIDER_PATCH_COUNT);
    log_key_u32(
        "All configured provider sites written and verified",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_patch_sites_applied ==
            PROVIDER_PATCH_COUNT);
    log_key_u32(
        "Provider instruction cache flushed",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_flush_result != 0u);
    log_key_u32(
        "Provider page protection restored",
        g_report.requested_provider_capacity ==
            VANILLA_PROVIDER_CAPACITY ||
        g_report.provider_protection_restore_result != 0u);
    log_key_u32(
        "Generator external records allocated",
        g_report.effective_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY ||
        g_report.generator_extra_storage != 0u);
    log_key_u32(
        "All five generator sites matched originals",
        g_report.effective_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY ||
        g_report.generator_patch_sites_verified ==
            GENERATOR_PATCH_COUNT);
    log_key_u32(
        "All five generator sites written and verified",
        g_report.effective_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY ||
        g_report.generator_patch_sites_applied ==
            GENERATOR_PATCH_COUNT);
    log_key_u32(
        "Generator instruction cache flushed",
        g_report.effective_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY ||
        g_report.generator_flush_result != 0u);
    log_key_u32(
        "Generator page protection restored",
        g_report.effective_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY ||
        g_report.generator_protection_restore_result != 0u);
    log_key_u32(
        "Unhandled-only crash handler installed",
        g_report.crash_handler_installed);
    log_key_u32(
        "Targeted procedural exception logger installed",
        g_report.targeted_exception_handler_installed);
    log_key_u32(
        "Definition/telemetry worker started",
        g_report.telemetry_thread_started);
    log_append(
        "Definition scaling is asynchronous; its final per-record result "
        "is appended under [PROCEDURAL.DAT SCALING].\r\n");
    log_key_u32(
        "Every synchronous startup action succeeded",
        g_report.status == STATUS_PATCH_APPLIED &&
        g_report.startup_worker_started != 0u &&
        g_report.asi_identity_lock_held != 0u &&
        g_report.ini_identity_lock_held != 0u &&
        g_report.executable_identity_lock_held != 0u &&
        g_report.identity_revalidation_passed != 0u &&
        g_report.crash_handler_installed != 0u &&
        g_report.targeted_exception_handler_installed != 0u &&
        g_report.telemetry_thread_started != 0u &&
        (!g_report.hang_watchdog_enabled ||
         g_report.hang_watchdog_thread_started != 0u));

    log_append("\r\n[STARTUP PATCH MANIFEST]\r\n");
    log_append("Capacity immediate RVA: ");
    log_append_hex32(
        g_report.target_rva == 0u ? 0u :
        g_report.target_rva + TARGET_IMMEDIATE_OFFSET);
    log_append("\r\nCapacity original value: ");
    log_append_u32(g_report.original_immediate);
    log_append("\r\nCapacity requested value: ");
    log_append_u32(g_report.requested_capacity);
    log_append("\r\nCapacity final read-back: ");
    log_append_u32(g_report.final_immediate);
    log_append("\r\nCapacity write verified: ");
    log_append_u32(
        g_report.status == STATUS_PATCH_APPLIED &&
        g_report.final_immediate == g_report.requested_capacity);
    log_append("\r\n");

    log_append(
        "\r\nDownstream generator expansion code sites:\r\n");
    if (g_generator_manifest_configured) {
        for (index = 0u;
             index < GENERATOR_PATCH_COUNT;
             ++index) {
            const struct GeneratorPatch *patch =
                &g_generator_patch_manifest[index];
            HMODULE main_module = GetModuleHandleA(NULL);
            DWORD rva = main_module ?
                (DWORD)(
                    patch->address -
                    (BYTE *)main_module) : 0u;
            log_append("Generator site ");
            log_append_u32(index + 1u);
            log_append(" RVA=");
            log_append_hex32(rva);
            log_append(" length=");
            log_append_u32(patch->length);
            log_append(
                "\r\n  expected live original "
                "(rebase-aware): ");
            log_append_hex_bytes(
                patch->original, patch->length);
            log_append("\r\n  observed at validation: ");
            log_append_hex_bytes(
                patch->observed_at_validation,
                patch->length);
            log_append("\r\n  replacement: ");
            log_append_hex_bytes(
                patch->replacement, patch->length);
            log_append(
                "\r\n  original matched at validation: ");
            log_append_u32(patch->original_match);
            log_append("\r\n  current live bytes: ");
            log_append_hex_bytes(
                patch->address, patch->length);
            log_append("\r\n  current equals replacement: ");
            log_append_u32(bytes_are_equal(
                patch->address,
                patch->replacement,
                patch->length));
            log_append("\r\n");
        }
        log_append(
            "Generator site 1 raises the special-object eviction "
            "threshold. Site 2 raises the rendered-entity activation "
            "threshold. Site 3 redirects the exhausted built-in "
            "free-list allocator to the ASI trampoline. Sites 4 and 5 "
            "raise the constructor allocation and 16-bit capacity for the "
            "per-update 0x20-byte staging queue.\r\n");
        if (g_report.effective_rendered_object_capacity ==
                VANILLA_RENDERED_OBJECT_CAPACITY) {
            log_append(
                "Selected method keeps all five generator sites at their "
                "verified original bytes; the replacement bytes above are "
                "validation-only and were not written.\r\n");
        }
    } else if (
        g_report.requested_rendered_object_capacity ==
            VANILLA_RENDERED_OBJECT_CAPACITY) {
        log_append(
            "RenderedObjectCapacity=512; downstream generator "
            "expansion not requested.\r\n");
    } else {
        log_append(
            "Generator manifest was not configured; see status "
            "above.\r\n");
    }

    log_append("\r\nLegacy manager-radius distance operands:\r\n");
    if (!LEGACY_DISTANCE_OPERAND_PATCH_ENABLED) {
        log_append(
            "The failed constant-operand experiment is disabled. The exact-hash "
            "universal transaction instead postscales the live manager fields, "
            "synchronizes the PLANT near/far setter, expands the source query, "
            "and adjusts only generated PROCOBJ instances.\r\n");
    } else if (g_report.distance_multiplier == 1u) {
        log_append(
            "DistanceMultiplier=1; both distance functions were identified for "
            "telemetry, but no distance operand was modified.\r\n");
    } else if (g_report.distance_init_rva != 0u &&
        g_report.distance_update_rva != 0u) {
        const DWORD operand_rvas[6] = {
            g_report.distance_init_rva +
                DIST_INIT_DETAIL_POINTER_OFFSET,
            g_report.distance_init_rva +
                DIST_INIT_NEAR_POINTER_OFFSET,
            g_report.distance_init_rva +
                DIST_INIT_FAR_POINTER_OFFSET,
            g_report.distance_update_rva +
                DIST_UPDATE_DETAIL_POINTER_OFFSET,
            g_report.distance_update_rva +
                DIST_UPDATE_NEAR_POINTER_OFFSET,
            g_report.distance_update_rva +
                DIST_UPDATE_FAR_POINTER_OFFSET
        };
        const DWORD original_pointers[6] = {
            g_report.detail_multiplier_source,
            g_report.distance_near_source,
            g_report.distance_far_source,
            g_report.detail_multiplier_source,
            g_report.distance_near_source,
            g_report.distance_far_source
        };
        const DWORD replacement_pointers[6] = {
            (DWORD)&g_scaled_half,
            (DWORD)&g_scaled_twenty,
            (DWORD)&g_scaled_forty,
            (DWORD)&g_scaled_half,
            (DWORD)&g_scaled_twenty,
            (DWORD)&g_scaled_forty
        };
        const float replacement_values[6] = {
            g_scaled_half, g_scaled_twenty, g_scaled_forty,
            g_scaled_half, g_scaled_twenty, g_scaled_forty
        };
        for (index = 0; index < 6u; ++index) {
            log_append("Distance operand ");
            log_append_u32(index + 1u);
            log_append(" code RVA=");
            log_append_hex32(operand_rvas[index]);
            log_append(" original pointer=");
            log_append_hex32(original_pointers[index]);
            log_append(" replacement pointer=");
            log_append_hex32(replacement_pointers[index]);
            log_append(" replacement value=");
            log_append_float_3(replacement_values[index]);
            log_append("\r\n");
        }
        log_append("Distance operand writes verified: ");
        log_append_u32(
            g_report.distance_flush_result != 0u &&
            g_report.distance_protection_restore_result != 0u);
        log_append("\r\n");
    } else {
        log_append("Not available because distance signatures were not "
                   "accepted.\r\n");
    }

    log_append("\r\nProvider relocation code sites:\r\n");
    if (g_provider_manifest_configured) {
        for (index = 0; index < PROVIDER_PATCH_COUNT; ++index) {
            const struct ProviderPatch *patch =
                &g_provider_patch_manifest[index];
            HMODULE main_module = GetModuleHandleA(NULL);
            DWORD rva = main_module ?
                (DWORD)(patch->address - (BYTE *)main_module) : 0u;
            log_append("Provider site ");
            log_append_u32(index + 1u);
            log_append(" RVA=");
            log_append_hex32(rva);
            log_append(" length=");
            log_append_u32(patch->length);
            log_append("\r\n  expected live original (rebase-aware): ");
            log_append_hex_bytes(patch->original, patch->length);
            log_append("\r\n  observed at validation: ");
            log_append_hex_bytes(
                patch->observed_at_validation, patch->length);
            log_append("\r\n  replacement: ");
            log_append_hex_bytes(patch->replacement, patch->length);
            log_append("\r\n  original matched at validation: ");
            log_append_u32(patch->original_match);
            log_append("\r\n  current live bytes: ");
            log_append_hex_bytes(patch->address, patch->length);
            log_append("\r\n  current equals original: ");
            log_append_u32(bytes_are_equal(
                patch->address, patch->original, patch->length));
            log_append("\r\n  current equals replacement: ");
            log_append_u32(bytes_are_equal(
                patch->address, patch->replacement, patch->length));
            log_append("\r\n");
        }
    } else if (g_report.requested_provider_capacity ==
               VANILLA_PROVIDER_CAPACITY) {
        log_append("ProviderCapacity=40; relocation not requested.\r\n");
    } else {
        log_append("Relocation manifest was not configured; see status "
                   "above.\r\n");
    }

    log_append("\r\nVerified static interpretation:\r\n");
    log_append("- procedural.dat material eligibility feeds both PLANT and PROCOBJ.\r\n");
    log_append("- the configurable capacity expands three 0x60-byte surface arrays.\r\n");
    log_append("- a separate downstream generator owns 512 embedded 0x18-byte\r\n");
    log_append("  live-object wrappers and separate 512 rendered/staging limits.\r\n");
    log_append("- this owner-test branch permits RenderedObjectCapacity from 512 to 32,768.\r\n");
    log_append("  Values above 512 use the exact-profile allocator, eviction, activation,\r\n");
    log_append("  and staging patch set; 32,768 is the experimental hard ceiling.\r\n");
    if (g_build_profile->provider_supported) {
        log_append("- auto always selects GTA IV's vanilla ProviderCapacity=40.\r\n");
        log_append("  Manual values above 40 are refused after owner-observed LOD flicker\r\n");
        log_append("  and a resource/LOD-lock hang with no demonstrated use of slot 41+.\r\n");
    } else {
        log_append("- this selected profile keeps the embedded 40-record provider\r\n");
        log_append("  storage unchanged; ProviderCapacity is safely limited to 40.\r\n");
    }
    log_append("- provider absolute operands are validated against the actual loaded\r\n");
    log_append("  GTAIV.exe base, including Windows image rebasing/ASLR.\r\n");
    if (g_build_profile->provider_patch_variant ==
            PROVIDER_PATCH_VARIANT_CE) {
        log_append("- CE provider site 10 preserves ESI as its original record-index\r\n");
        log_append("  source; the replacement encodes imul eax,esi,60h.\r\n");
    } else {
        log_append("- Patch 7/8 provider relocation uses nine fixed-length rewrites\r\n");
        log_append("  covering manager roots, entity lookup, allocation, and traversal.\r\n");
    }
    log_append("- telemetry reports the provider, surface, and downstream rendered\r\n");
    log_append("  generator pools independently after game initialization.\r\n");
    log_append("- telemetry reads GTA IV's material-to-PLANT/PROCOBJ maps and the\r\n");
    log_append("  live provider eligibility bitsets without modifying either one.\r\n");
    log_append("- exhaustion is polled every 100 ms and latched for the full session.\r\n");
    log_append("- the rejected shared-constant distance patch is disabled; exact-hash\r\n");
    log_append("  hooks scale live manager far/query/squared coverage while preserving\r\n");
    log_append("  GTA IV's preload lead. At 1x-4x the authored fade width is retained;\r\n");
    log_append("  experimental 0.5x clamps the near edge to zero and shortens that band.\r\n");
    log_append("- legacy CBaseModelInfo draw-distance scaling is retired because it is\r\n");
    log_append("  model-global and can affect placed/non-procedural objects.\r\n");
    log_append("- source enumeration uses a bounded 32,768-pointer mod bank and a 15-120m\r\n");
    log_append("  query radius; generated PROCOBJ visibility is changed only after exact\r\n");
    log_append("  wrapper/surface ownership publication.\r\n");
    log_append("- after procedural.dat loads, exact field fingerprints select only the\r\n");
    log_append("  generated 179-row or 209-row stock PROCOBJ class catalog. Modified or\r\n");
    log_append("  reordered data fails closed without density writes.\r\n");
    log_append("- selected random definitions scale inverse-square density; selected\r\n");
    log_append("  USEGRID definitions scale spacing and the authored inverse-square. PLANT\r\n");
    log_append("  visible count uses the proven count consumer, exact output-key allowlist,\r\n");
    log_append("  and an atomically redirected 128-record workspace.\r\n");
    log_append("- unhandled exceptions write crash dumps; the independent window\r\n");
    log_append("  watchdog writes a separate hang dump after sustained non-response.\r\n");
    log_append("- a narrowly scoped vectored logger records first-chance faults only\r\n");
    log_append("  when code/fault addresses intersect the patched procedural subsystem;\r\n");
    log_append("  it never creates a dump and ignores unrelated kernel32 exceptions.\r\n");
}

static HANDLE open_log_file(void)
{
    HANDLE file;
    char fallback[MAX_PATH];
    char temp_directory[MAX_PATH];
    DWORD temp_length;

    file = CreateFileA(g_log_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        return file;
    }

    copy_string(fallback, MAX_PATH, PLUGIN_NAME ".log");
    file = CreateFileA(fallback, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        copy_string(g_log_path, MAX_PATH, fallback);
        return file;
    }

    temp_length = GetTempPathA(MAX_PATH, temp_directory);
    if (temp_length > 0 && temp_length < MAX_PATH) {
        DWORD index = string_length(temp_directory);
        const char *name = PLUGIN_NAME ".log";
        while (index + 1 < MAX_PATH && *name) {
            temp_directory[index++] = *name++;
        }
        temp_directory[index] = '\0';
        file = CreateFileA(temp_directory, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            copy_string(g_log_path, MAX_PATH, temp_directory);
            return file;
        }
    }

    return INVALID_HANDLE_VALUE;
}

static void write_log(void)
{
    HANDLE file;
    DWORD written = 0;
    if (!g_debug_log_enabled &&
        g_report.status == STATUS_PATCH_APPLIED) {
        return;
    }
    build_log();
    file = open_log_file();
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    WriteFile(file, g_log_buffer, g_log_length, &written, NULL);
    FlushFileBuffers(file);
    CloseHandle(file);
}

static int readable_address_range(const BYTE *address, DWORD size)
{
    MEMORY_BASIC_INFORMATION information;
    BYTE *last;

    if (!address || size == 0) {
        return 0;
    }
    last = (BYTE *)address + size - 1u;
    if (last < address) {
        return 0;
    }

    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return 0;
    }
    if (VirtualQuery(last, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return 0;
    }
    return 1;
}

static int writable_address_range(BYTE *address, DWORD size)
{
    MEMORY_BASIC_INFORMATION information;
    BYTE *last;
    DWORD writable_mask =
        PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    if (!address || size == 0u) {
        return 0;
    }
    last = address + size - 1u;
    if (last < address) {
        return 0;
    }
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        (information.Protect & writable_mask) == 0u) {
        return 0;
    }
    if (VirtualQuery(last, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        (information.Protect & writable_mask) == 0u) {
        return 0;
    }
    return 1;
}

static void append_current_log_buffer(void)
{
    HANDLE file;
    DWORD written = 0;

    if (!g_debug_log_enabled) {
        return;
    }

    file = CreateFileA(
        g_log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    WriteFile(file, g_log_buffer, g_log_length, &written, NULL);
    FlushFileBuffers(file);
    CloseHandle(file);
}

static void telemetry_begin_block(const char *name)
{
    SYSTEMTIME utc;
    g_log_length = 0;
    g_log_buffer[0] = '\0';
    GetSystemTime(&utc);
    log_append("\r\n[");
    log_append(name);
    log_append("] UTC ");
    log_append_u32((DWORD)utc.wYear);
    log_append("-");
    if (utc.wMonth < 10) log_append("0");
    log_append_u32((DWORD)utc.wMonth);
    log_append("-");
    if (utc.wDay < 10) log_append("0");
    log_append_u32((DWORD)utc.wDay);
    log_append(" ");
    if (utc.wHour < 10) log_append("0");
    log_append_u32((DWORD)utc.wHour);
    log_append(":");
    if (utc.wMinute < 10) log_append("0");
    log_append_u32((DWORD)utc.wMinute);
    log_append(":");
    if (utc.wSecond < 10) log_append("0");
    log_append_u32((DWORD)utc.wSecond);
    log_append("\r\n");
}

static DWORD count_index_list(
    const BYTE *records, DWORD capacity, WORD head, DWORD next_offset,
    DWORD *flag_bit0_records, DWORD *flag_bit1_records,
    DWORD *low3_histogram, int *valid)
{
    DWORD count = 0;
    WORD index = head;

    *valid = 1;
    while (index != 0) {
        const BYTE *record;
        BYTE flags;

        if ((DWORD)index > capacity || count >= capacity) {
            *valid = 0;
            return count;
        }
        record = records + ((DWORD)index - 1u) * PROCEDURAL_RECORD_SIZE;
        flags = record[SURFACE_FLAGS_OFFSET];
        if (flag_bit0_records && (flags & 1u) != 0) {
            ++*flag_bit0_records;
        }
        if (flag_bit1_records && (flags & 2u) != 0) {
            ++*flag_bit1_records;
        }
        if (low3_histogram) {
            ++low3_histogram[flags & 7u];
        }
        ++count;
        index = read_u16(record + next_offset);
    }
    return count;
}

struct TelemetrySnapshot {
    DWORD capacity;
    DWORD source_free;
    DWORD source_active;
    DWORD surface_free;
    DWORD surface_active;
    DWORD surface_flag_bit0_records;
    DWORD surface_flag_bit1_records;
    DWORD surface_flag_bits0_and1_records;
    DWORD surface_low3_flag_histogram[8];
    DWORD category_counts[MANAGER_SURFACE_LIST_COUNT];
    DWORD staging_counts[2][MANAGER_SURFACE_LIST_COUNT];
    DWORD staging_totals[2];
    DWORD staging_index;
    DWORD buffer0;
    DWORD buffer1;
    DWORD buffer2;
    DWORD source_lists_valid;
    DWORD surface_lists_valid;
    DWORD definition_maps_readable;
    DWORD definition_map_indices_valid;
    DWORD definition_procobj_count;
    DWORD definition_plant_count;
    DWORD material_plant_mapped;
    DWORD material_procobj_mapped;
    DWORD material_both_mapped;
    DWORD material_neither_mapped;
    DWORD material_invalid_plant_indices;
    DWORD material_invalid_procobj_indices;
    DWORD provider_bitmaps_valid;
    DWORD provider_records_scanned;
    DWORD provider_declared_bits;
    DWORD provider_plant_eligibility_bits;
    DWORD provider_procobj_eligibility_bits;
    DWORD providers_with_plant;
    DWORD providers_with_procobj;
    DWORD providers_with_both;
    DWORD providers_with_neither;
    DWORD provider_bitmap_read_failures;
    DWORD generator_readable;
    DWORD generator_initialized;
    DWORD generator_invariant_valid;
    DWORD generator_active_rendered;
    DWORD generator_secondary_entities;
    DWORD generator_free_records;
    DWORD generator_free_head;
    DWORD generator_extra_issued;
    DWORD generator_total_records_introduced;
    DWORD generator_active_wrapper_records;
    DWORD generator_fallback_calls;
    DWORD generator_extra_exhaustion_observed;
    DWORD generator_staging_capacity;
    DWORD generator_staging_count;
    DWORD generator_staging_buffer;
    DWORD invariant_valid;
    float detail_distance;
    float fade_close;
    float plant_far;
    float query_radius;
    float plant_far_squared;
    float expected_fade_close;
    float expected_plant_far;
    float expected_query_radius;
    float expected_plant_far_squared;
    float position_x;
    float position_y;
    float position_z;
};

static struct TelemetrySnapshot g_last_telemetry_snapshot;
static volatile LONG g_last_snapshot_state;
static DWORD g_last_surface_high_watermark;
static DWORD g_last_provider_high_watermark;
static DWORD g_last_sample_number;

static void publish_telemetry_snapshot(
    const struct TelemetrySnapshot *snapshot,
    DWORD sample_number,
    DWORD high_surface,
    DWORD high_provider)
{
    InterlockedExchange(&g_last_snapshot_state, 1);
    copy_bytes(
        (BYTE *)&g_last_telemetry_snapshot,
        (const BYTE *)snapshot,
        sizeof(g_last_telemetry_snapshot));
    g_last_sample_number = sample_number;
    g_last_surface_high_watermark = high_surface;
    g_last_provider_high_watermark = high_provider;
    InterlockedExchange(&g_last_snapshot_state, 2);
}

static int read_published_telemetry_snapshot(
    struct TelemetrySnapshot *snapshot,
    DWORD *sample_number,
    DWORD *high_surface,
    DWORD *high_provider)
{
    DWORD attempt;
    for (attempt = 0; attempt < 3u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &g_last_snapshot_state, 0, 0);
        if (before != 2) {
            return 0;
        }
        copy_bytes(
            (BYTE *)snapshot,
            (const BYTE *)&g_last_telemetry_snapshot,
            sizeof(*snapshot));
        *sample_number = g_last_sample_number;
        *high_surface = g_last_surface_high_watermark;
        *high_provider = g_last_provider_high_watermark;
        if (InterlockedCompareExchange(
                &g_last_snapshot_state, 0, 0) == before) {
            return 1;
        }
    }
    return 0;
}

static void capture_definition_map_telemetry(
    struct TelemetrySnapshot *snapshot)
{
    DWORD material;
    DWORD proc_count;
    DWORD plant_count;

    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MATERIAL_MAP_REQUIRED_SIZE)) {
        return;
    }

    proc_count = read_u32(
        g_definition_manager +
        DEFINITION_PROCOBJ_COUNT_OFFSET);
    plant_count = read_u32(
        g_definition_manager +
        DEFINITION_PLANT_COUNT_OFFSET);
    snapshot->definition_procobj_count = proc_count;
    snapshot->definition_plant_count = plant_count;
    snapshot->definition_maps_readable = 1u;
    snapshot->definition_map_indices_valid = 1u;

    if (proc_count > DEFINITION_PROCOBJ_CAPACITY ||
        plant_count > DEFINITION_PLANT_CAPACITY) {
        snapshot->definition_map_indices_valid = 0u;
        return;
    }

    for (material = 0u;
         material < DEFINITION_MATERIAL_COUNT;
         ++material) {
        BYTE *entry =
            g_definition_manager +
            DEFINITION_MATERIAL_MAP_OFFSET +
            material * DEFINITION_MATERIAL_MAP_STRIDE;
        LONG proc_index = (LONG)read_u32(
            entry + DEFINITION_MATERIAL_PROCOBJ_OFFSET);
        LONG plant_index = (LONG)read_u32(
            entry + DEFINITION_MATERIAL_PLANT_OFFSET);
        int has_proc = proc_index > -1;
        int has_plant = plant_index > -1;

        if (has_proc) {
            ++snapshot->material_procobj_mapped;
            if ((DWORD)proc_index >= proc_count) {
                ++snapshot->material_invalid_procobj_indices;
                snapshot->definition_map_indices_valid = 0u;
            }
        }
        if (has_plant) {
            ++snapshot->material_plant_mapped;
            if ((DWORD)plant_index >= plant_count) {
                ++snapshot->material_invalid_plant_indices;
                snapshot->definition_map_indices_valid = 0u;
            }
        }
        if (has_proc && has_plant) {
            ++snapshot->material_both_mapped;
        } else if (!has_proc && !has_plant) {
            ++snapshot->material_neither_mapped;
        }
    }
}

static int read_foreign_u32(const BYTE *address, DWORD *value)
{
    SIZE_T bytes_read = 0;
    DWORD temporary = 0;

    if (!address || !value ||
        !ReadProcessMemory(
            GetCurrentProcess(), address,
            &temporary, sizeof(temporary),
            &bytes_read) ||
        bytes_read != sizeof(temporary)) {
        return 0;
    }
    *value = temporary;
    return 1;
}

static void capture_provider_bitmap_telemetry(
    struct TelemetrySnapshot *snapshot,
    const BYTE *source_records)
{
    WORD index;
    DWORD visited = 0u;

    snapshot->provider_bitmaps_valid = 1u;
    index = read_u16(
        g_manager + MANAGER_SOURCE_ACTIVE_HEAD_OFFSET);
    while (index != 0u) {
        const BYTE *record;
        DWORD bitmap_pointer;
        DWORD dword_count;
        DWORD bit_count;
        DWORD word_index;
        int provider_has_plant = 0;
        int provider_has_proc = 0;

        if ((DWORD)index > g_provider_capacity ||
            visited >= g_provider_capacity) {
            snapshot->provider_bitmaps_valid = 0u;
            return;
        }
        record = source_records +
            ((DWORD)index - 1u) * PROCEDURAL_RECORD_SIZE;
        bitmap_pointer = read_u32(
            record + PROVIDER_BITMAP_POINTER_OFFSET);
        dword_count = (DWORD)read_u16(
            record + PROVIDER_BITMAP_DWORD_COUNT_OFFSET);
        bit_count = (DWORD)read_u16(
            record + PROVIDER_BITMAP_BIT_COUNT_OFFSET);

        ++snapshot->provider_records_scanned;
        snapshot->provider_declared_bits += bit_count;

        if (bit_count > dword_count * 32u ||
            (bit_count != 0u &&
             (bitmap_pointer == 0u || dword_count == 0u))) {
            snapshot->provider_bitmaps_valid = 0u;
            ++snapshot->provider_bitmap_read_failures;
        } else {
            for (word_index = 0u;
                 word_index < dword_count;
                 ++word_index) {
                DWORD bits;
                DWORD bit_in_word;
                DWORD base_bit = word_index * 32u;

                if (!read_foreign_u32(
                        (const BYTE *)bitmap_pointer +
                        word_index * 4u,
                        &bits)) {
                    snapshot->provider_bitmaps_valid = 0u;
                    ++snapshot->provider_bitmap_read_failures;
                    break;
                }
                for (bit_in_word = 0u;
                     bit_in_word < 32u &&
                     base_bit + bit_in_word < bit_count;
                     ++bit_in_word) {
                    if ((bits & (1u << bit_in_word)) == 0u) {
                        continue;
                    }
                    if (((base_bit + bit_in_word) & 1u) == 0u) {
                        ++snapshot->provider_plant_eligibility_bits;
                        provider_has_plant = 1;
                    } else {
                        ++snapshot->provider_procobj_eligibility_bits;
                        provider_has_proc = 1;
                    }
                }
            }
        }

        if (provider_has_plant) {
            ++snapshot->providers_with_plant;
        }
        if (provider_has_proc) {
            ++snapshot->providers_with_procobj;
        }
        if (provider_has_plant && provider_has_proc) {
            ++snapshot->providers_with_both;
        } else if (!provider_has_plant && !provider_has_proc) {
            ++snapshot->providers_with_neither;
        }

        index = read_u16(
            record + SOURCE_RECORD_ACTIVE_NEXT_OFFSET);
        ++visited;
    }
}

static int capture_telemetry_snapshot(struct TelemetrySnapshot *snapshot)
{
    DWORD attempt;

    for (attempt = 0; attempt < 3u; ++attempt) {
        BYTE *surface_records;
        BYTE *source_records;
        DWORD capacity;
        int source_free_valid;
        int source_active_valid;
        int surface_free_valid;
        DWORD list_index;
        DWORD flag_bit0_count = 0;
        DWORD flag_bit1_count = 0;
        DWORD bits0_and1_count = 0;
        DWORD active_count = 0;
        DWORD capacity_after;
        DWORD buffer0_after;

        ZeroMemory(snapshot, sizeof(*snapshot));
        if (!g_manager ||
            !readable_address_range(g_manager, MANAGER_MINIMUM_SIZE)) {
            return 0;
        }

        capacity = read_u32(g_manager + MANAGER_CAPACITY_OFFSET);
        snapshot->buffer0 = read_u32(g_manager + MANAGER_BUFFER0_OFFSET);
        snapshot->buffer1 = read_u32(g_manager + MANAGER_BUFFER1_OFFSET);
        snapshot->buffer2 = read_u32(g_manager + MANAGER_BUFFER2_OFFSET);
        snapshot->capacity = capacity;

        if (capacity < MIN_CAPACITY ||
            capacity > MAX_OPERATIONAL_CAPACITY ||
            snapshot->buffer0 == 0 ||
            snapshot->buffer1 == 0 ||
            snapshot->buffer2 == 0) {
            return 0;
        }

        surface_records = (BYTE *)snapshot->buffer0;
        source_records = g_provider_records;
        if (!readable_address_range(
                surface_records,
                capacity * PROCEDURAL_RECORD_SIZE) ||
            !readable_address_range(
                (BYTE *)snapshot->buffer1,
                capacity * PROCEDURAL_RECORD_SIZE) ||
            !readable_address_range(
                (BYTE *)snapshot->buffer2,
                capacity * PROCEDURAL_RECORD_SIZE) ||
            !readable_address_range(
                source_records,
                g_provider_capacity * PROCEDURAL_RECORD_SIZE)) {
            return 0;
        }

            snapshot->source_free = count_index_list(
                source_records, g_provider_capacity,
                read_u16(
                    g_manager +
                    MANAGER_SOURCE_FREE_HEAD_OFFSET),
                SOURCE_NEXT_OFFSET, NULL, NULL, NULL,
                &source_free_valid);
            snapshot->source_active = count_index_list(
                source_records, g_provider_capacity,
                read_u16(
                    g_manager +
                    MANAGER_SOURCE_ACTIVE_HEAD_OFFSET),
                SOURCE_RECORD_ACTIVE_NEXT_OFFSET, NULL, NULL, NULL,
                &source_active_valid);
        snapshot->source_lists_valid =
            source_free_valid && source_active_valid;
        capture_definition_map_telemetry(snapshot);
        if (snapshot->source_lists_valid) {
            capture_provider_bitmap_telemetry(
                snapshot, source_records);
        }

        snapshot->surface_free = count_index_list(
            surface_records, capacity,
            read_u16(g_manager + MANAGER_SURFACE_FREE_HEAD_OFFSET),
            SURFACE_NEXT_OFFSET, NULL, NULL, NULL,
            &surface_free_valid);
        snapshot->surface_lists_valid = surface_free_valid;

        for (list_index = 0;
             list_index < MANAGER_SURFACE_LIST_COUNT;
             ++list_index) {
            DWORD before_bit0 = flag_bit0_count;
            DWORD before_bit1 = flag_bit1_count;
            int list_valid;
            DWORD record_count;
            WORD head = read_u16(
                g_manager + MANAGER_SURFACE_LIST0_OFFSET +
                list_index * 2u);

            record_count = count_index_list(
                surface_records, capacity, head, SURFACE_NEXT_OFFSET,
                &flag_bit0_count, &flag_bit1_count,
                snapshot->surface_low3_flag_histogram,
                &list_valid);
            snapshot->category_counts[list_index] = record_count;
            active_count += record_count;
            if (!list_valid) {
                snapshot->surface_lists_valid = 0;
            }

            if (flag_bit0_count - before_bit0 != 0 &&
                flag_bit1_count - before_bit1 != 0) {
                DWORD scan_count = 0;
                WORD scan_index = head;
                while (scan_index != 0 && scan_count < capacity) {
                    const BYTE *record =
                        surface_records +
                        ((DWORD)scan_index - 1u) *
                        PROCEDURAL_RECORD_SIZE;
                    if ((record[SURFACE_FLAGS_OFFSET] & 3u) == 3u) {
                        ++bits0_and1_count;
                    }
                    scan_index = read_u16(
                        record + SURFACE_NEXT_OFFSET);
                    ++scan_count;
                }
            }
        }

        snapshot->surface_active = active_count;
        snapshot->surface_flag_bit0_records = flag_bit0_count;
        snapshot->surface_flag_bit1_records = flag_bit1_count;
        snapshot->surface_flag_bits0_and1_records =
            bits0_and1_count;
        snapshot->invariant_valid =
            snapshot->source_lists_valid &&
            snapshot->surface_lists_valid &&
            snapshot->source_free + snapshot->source_active ==
                g_provider_capacity &&
            snapshot->surface_free + snapshot->surface_active ==
                capacity;

        for (list_index = 0; list_index < 2u; ++list_index) {
            DWORD category;
            for (category = 0;
                 category < MANAGER_SURFACE_LIST_COUNT;
                 ++category) {
                DWORD value = (DWORD)read_u16(
                    g_manager + MANAGER_STAGING_COUNTS_OFFSET +
                    list_index * 8u + category * 2u);
                snapshot->staging_counts[list_index][category] =
                    value;
                snapshot->staging_totals[list_index] += value;
            }
        }
        snapshot->staging_index =
            read_u32(g_manager + MANAGER_STAGING_INDEX_OFFSET);

        snapshot->fade_close =
            read_float(g_manager + MANAGER_FADE_CLOSE_OFFSET);
        snapshot->plant_far =
            read_float(g_manager + MANAGER_PLANT_FAR_OFFSET);
        snapshot->query_radius =
            read_float(g_manager + MANAGER_QUERY_RADIUS_OFFSET);
        snapshot->plant_far_squared =
            read_float(g_manager + MANAGER_PLANT_FAR_SQUARED_OFFSET);
        snapshot->position_x =
            read_float(g_manager + MANAGER_POSITION_OFFSET);
        snapshot->position_y =
            read_float(g_manager + MANAGER_POSITION_OFFSET + 4u);
        snapshot->position_z =
            read_float(g_manager + MANAGER_POSITION_OFFSET + 8u);

        if (g_generator_manager &&
            readable_address_range(
                g_generator_manager,
                GENERATOR_MANAGER_MINIMUM_SIZE)) {
            DWORD total_records;
            snapshot->generator_readable = 1u;
            snapshot->generator_active_rendered =
                read_u32(
                    g_generator_manager +
                    GENERATOR_ACTIVE_RENDERED_COUNT_OFFSET);
            snapshot->generator_secondary_entities =
                read_u32(
                    g_generator_manager +
                    GENERATOR_SECONDARY_ENTITY_COUNT_OFFSET);
            snapshot->generator_free_records =
                read_u32(
                    g_generator_manager +
                    GENERATOR_FREE_COUNT_OFFSET);
            snapshot->generator_free_head =
                read_u32(
                    g_generator_manager +
                    GENERATOR_FREE_HEAD_OFFSET);
            snapshot->generator_extra_issued =
                (DWORD)InterlockedCompareExchange(
                    &g_generator_extra_issued, 0, 0);
            snapshot->generator_fallback_calls =
                (DWORD)InterlockedCompareExchange(
                    &g_generator_fallback_calls, 0, 0);
            snapshot->generator_extra_exhaustion_observed =
                (DWORD)InterlockedCompareExchange(
                    &g_generator_extra_exhaustion_observed,
                    0, 0);
            snapshot->generator_staging_capacity =
                read_u32(
                    g_generator_manager +
                    GENERATOR_STAGING_CAPACITY_OFFSET);
            snapshot->generator_staging_count =
                (DWORD)read_u16(
                    g_generator_manager +
                    GENERATOR_STAGING_COUNT_OFFSET);
            snapshot->generator_staging_buffer =
                read_u32(
                    g_generator_manager +
                    GENERATOR_STAGING_BUFFER_OFFSET);
            if (snapshot->generator_staging_buffer == 0u &&
                snapshot->generator_staging_capacity == 0u) {
                /* Constructor has not initialized the manager yet. */
                snapshot->generator_invariant_valid = 1u;
            } else if (
                snapshot->generator_staging_buffer != 0u &&
                snapshot->generator_staging_capacity ==
                    g_generator_render_capacity) {
                snapshot->generator_initialized = 1u;
                total_records =
                    VANILLA_RENDERED_OBJECT_CAPACITY +
                    snapshot->generator_extra_issued;
                snapshot->generator_total_records_introduced =
                    total_records;
                if (total_records <= g_generator_render_capacity &&
                    snapshot->generator_free_records <=
                        total_records) {
                    snapshot->generator_active_wrapper_records =
                        total_records -
                        snapshot->generator_free_records;
                    snapshot->generator_invariant_valid = 1u;
                }
            }
            update_long_high_watermark(
                &g_generator_active_high_watermark,
                snapshot->generator_active_rendered);
            update_long_high_watermark(
                &g_generator_wrapper_high_watermark,
                snapshot->generator_active_wrapper_records);
            update_long_high_watermark(
                &g_generator_staging_high_watermark,
                snapshot->generator_staging_count);
        }

        if (g_report.detail_distance_source != 0 &&
            readable_address_range(
                (BYTE *)g_report.detail_distance_source, 4u)) {
            snapshot->detail_distance =
                read_float((BYTE *)g_report.detail_distance_source);
        }
        {
            float multiplier = read_float(
                (BYTE *)&g_report.corrected_distance_multiplier_bits);
            float vanilla_far =
                snapshot->detail_distance * 0.5f + 20.0f;
            float authored_near = read_float(
                (const BYTE *)&g_gu_behavior.manager_original_near_bits);
            float vanilla_fade_width;
            if ((DWORD)InterlockedCompareExchange(
                    &g_gu_behavior.manager_key, 0, 0) !=
                    (DWORD)(ULONG_PTR)g_manager ||
                !nonnegative_finite_float(authored_near) ||
                authored_near > vanilla_far) {
                authored_near = 20.0f;
            }
            vanilla_fade_width = vanilla_far - authored_near;
            snapshot->expected_plant_far =
                vanilla_far * multiplier;
            snapshot->expected_fade_close =
                snapshot->expected_plant_far -
                vanilla_fade_width;
        }
        snapshot->expected_query_radius =
            snapshot->expected_plant_far + 40.0f;
        snapshot->expected_plant_far_squared =
            snapshot->expected_plant_far *
            snapshot->expected_plant_far;

        capacity_after =
            read_u32(g_manager + MANAGER_CAPACITY_OFFSET);
        buffer0_after =
            read_u32(g_manager + MANAGER_BUFFER0_OFFSET);
        if (capacity_after == capacity &&
            buffer0_after == snapshot->buffer0 &&
            snapshot->invariant_valid) {
            return 1;
        }
        Sleep(1);
    }
    return 1;
}

static void write_telemetry_sample(
    const struct TelemetrySnapshot *snapshot,
    DWORD sample_number, DWORD high_surface, DWORD high_source)
{
    struct GuSourceBankCounters source_bank;
    struct GuPlantBankCounters plant_bank;
    DWORD index;

    ZeroMemory(&source_bank, sizeof(source_bank));
    ZeroMemory(&plant_bank, sizeof(plant_bank));
    gu_source_bank_get_counters(&source_bank);
    gu_plant_bank_get_counters(&plant_bank);

    telemetry_begin_block("RUNTIME SAMPLE");
    log_key_u32("Sample", sample_number);
    log_key_u32("Snapshot invariants valid",
                snapshot->invariant_valid);
    log_key_u32("Configured capacity", snapshot->capacity);
    log_key_hex("Surface buffer primary", snapshot->buffer0);
    log_key_hex("Plant staging buffer A", snapshot->buffer1);
    log_key_hex("Plant staging buffer B", snapshot->buffer2);

    log_append("\r\nEligible surface-provider pool (vanilla 40; exact relocation active when configured above 40):\r\n");
    log_key_u32("Configured provider capacity", g_provider_capacity);
    log_key_u32("Provider active", snapshot->source_active);
    log_key_u32("Provider free", snapshot->source_free);
    log_key_u32("Provider high watermark", high_source);
    log_key_u32("Provider pool exhausted now",
                snapshot->source_free == 0u);
    log_key_u32("Provider exhaustion observed since start",
                (DWORD)InterlockedCompareExchange(
                    &g_provider_exhaustion_observed, 0, 0));
    log_key_u32(
        "Provider external-list rebuilds verified",
        (DWORD)InterlockedCompareExchange(
            &g_provider_rebuilds_verified, 0, 0));
    log_key_u32(
        "Provider external-list rebuild failures",
        (DWORD)InterlockedCompareExchange(
            &g_provider_rebuild_failures, 0, 0));
    log_key_u32(
        "Provider records inspected for eligibility",
        snapshot->provider_records_scanned);
    log_key_u32(
        "Provider bitmap telemetry valid",
        snapshot->provider_bitmaps_valid);
    log_key_u32(
        "Provider bitmap read failures",
        snapshot->provider_bitmap_read_failures);
    log_key_u32(
        "Provider eligibility bits declared",
        snapshot->provider_declared_bits);
    log_key_u32(
        "Live provider PLANT eligibility bits set",
        snapshot->provider_plant_eligibility_bits);
    log_key_u32(
        "Live provider PROCOBJ eligibility bits set",
        snapshot->provider_procobj_eligibility_bits);
    log_key_u32(
        "Providers containing PLANT eligibility",
        snapshot->providers_with_plant);
    log_key_u32(
        "Providers containing PROCOBJ eligibility",
        snapshot->providers_with_procobj);
    log_key_u32(
        "Providers containing both eligibility types",
        snapshot->providers_with_both);
    log_key_u32(
        "Providers containing neither eligibility type",
        snapshot->providers_with_neither);

    log_append("\r\nprocedural.dat material eligibility maps:\r\n");
    log_key_u32(
        "Definition maps readable",
        snapshot->definition_maps_readable);
    log_key_u32(
        "Definition map indices valid",
        snapshot->definition_map_indices_valid);
    log_key_u32(
        "Loaded PLANT definition count",
        snapshot->definition_plant_count);
    log_key_u32(
        "Loaded PROCOBJ definition count",
        snapshot->definition_procobj_count);
    log_key_u32(
        "Material IDs mapped to PLANT",
        snapshot->material_plant_mapped);
    log_key_u32(
        "Material IDs mapped to PROCOBJ",
        snapshot->material_procobj_mapped);
    log_key_u32(
        "Material IDs mapped to both",
        snapshot->material_both_mapped);
    log_key_u32(
        "Material IDs mapped to neither",
        snapshot->material_neither_mapped);
    log_key_u32(
        "Out-of-range PLANT map indices",
        snapshot->material_invalid_plant_indices);
    log_key_u32(
        "Out-of-range PROCOBJ map indices",
        snapshot->material_invalid_procobj_indices);
    log_append(
        "Pipeline interpretation: GTAIV.exe builds even provider bits "
        "from the PLANT map and odd provider bits from the PROCOBJ map. "
        "A nonzero PLANT-map count but zero live PLANT bits localizes "
        "the loss before surface construction; nonzero live PLANT bits "
        "but zero surface bit 0 localizes it to construction or the "
        "plant density/area gate.\r\n");

    log_append(
        "\r\nProcedural surface pool (vanilla 512; core fix configured "
        "up to 40,960):\r\n");
    log_key_u32("Surface active", snapshot->surface_active);
    log_key_u32("Surface free", snapshot->surface_free);
    log_key_u32("Surface high watermark", high_surface);
    log_key_u32("Vanilla 512 limit exceeded",
                snapshot->surface_active > 511u);
    log_key_u32("Expanded pool exhausted now",
                snapshot->surface_free <= 1u);
    log_key_u32("Surface exhaustion observed since start",
                (DWORD)InterlockedCompareExchange(
                    &g_surface_exhaustion_observed, 0, 0));
    log_key_u32("Active records with raw flag bit 0",
                snapshot->surface_flag_bit0_records);
    log_key_u32("Active records with raw flag bit 1",
                snapshot->surface_flag_bit1_records);
    log_key_u32("Active records with raw flag bits 0 and 1",
                snapshot->surface_flag_bits0_and1_records);
    log_append(
        "Verified flag path: bit 0 is supplied by the even PLANT "
        "provider eligibility bit and is required for plant staging. "
        "Bit 1 is supplied by the odd PROCOBJ eligibility bit and is "
        "required for procedural-object generation. GTA IV's shipped "
        "PROCOBJ definitions include proc_grass01 and many vegetation "
        "assets as well as rubbish, so visible grass is not confined to "
        "the separately named PLANT path. The plant "
        "density/triangle-area gate can clear bit 0 before the record "
        "is linked into an active surface list.\r\n");
    for (index = 0; index < 8u; ++index) {
        log_append("Active record low-three-bit flag value ");
        log_append_u32(index);
        log_append(": ");
        log_append_u32(
            snapshot->surface_low3_flag_histogram[index]);
        log_append("\r\n");
    }
    for (index = 0; index < MANAGER_SURFACE_LIST_COUNT; ++index) {
        log_append("Surface category ");
        log_append_u32(index);
        log_append(": ");
        log_append_u32(snapshot->category_counts[index]);
        log_append("\r\n");
    }

    log_append(
        "\r\nDownstream rendered procedural-object generator "
        "(vanilla 512; bounded owner-test expansion up to 32,768):\r\n");
    log_key_u32(
        "Generator manager readable",
        snapshot->generator_readable);
    log_key_u32(
        "Generator runtime initialized",
        snapshot->generator_initialized);
    log_key_u32(
        "Generator accounting invariant valid",
        snapshot->generator_invariant_valid);
    log_key_u32(
        "Configured rendered-object capacity",
        g_generator_render_capacity);
    log_key_u32(
        "Governor rendered/staging pressure threshold",
        tuning_governor_near_capacity_threshold(
            g_generator_render_capacity));
    log_key_u32(
        "Live rendered procedural entities",
        snapshot->generator_active_rendered);
    log_key_u32(
        "Rendered-entity high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_generator_active_high_watermark, 0, 0));
    log_key_u32(
        "Secondary generator entity count",
        snapshot->generator_secondary_entities);
    log_key_u32(
        "Active generator wrapper records",
        snapshot->generator_active_wrapper_records);
    log_key_u32(
        "Wrapper-record high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_generator_wrapper_high_watermark, 0, 0));
    log_key_u32(
        "Generator free-list records",
        snapshot->generator_free_records);
    log_key_hex(
        "Generator free-list head",
        snapshot->generator_free_head);
    log_key_u32(
        "Extra wrapper records issued",
        snapshot->generator_extra_issued);
    log_key_u32(
        "Total wrapper records introduced",
        snapshot->generator_total_records_introduced);
    log_key_u32(
        "Fallback allocator calls",
        snapshot->generator_fallback_calls);
    log_key_u32(
        "Expanded generator exhausted since start",
        snapshot->generator_extra_exhaustion_observed);
    log_key_u32(
        "Generator vanilla 512 rendered limit exceeded",
        snapshot->generator_active_rendered >
            VANILLA_RENDERED_OBJECT_CAPACITY - 1u);
    log_key_u32(
        "Configured rendered limit exceeded",
        snapshot->generator_active_rendered >
            g_generator_render_capacity);
    log_key_u32(
        "Configured wrapper limit exceeded",
        snapshot->generator_active_wrapper_records >
            g_generator_render_capacity);
    log_key_u32(
        "Generator staging capacity",
        snapshot->generator_staging_capacity);
    log_key_u32(
        "Generator staging count now",
        snapshot->generator_staging_count);
    log_key_u32(
        "Generator staging high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_generator_staging_high_watermark, 0, 0));
    log_key_hex(
        "Generator staging buffer",
        snapshot->generator_staging_buffer);
    if (g_generator_render_capacity == VANILLA_RENDERED_OBJECT_CAPACITY) {
        log_append(
            "External wrapper allocation is disabled because the configured "
            "rendered-object capacity is 512. Extra-wrapper and fallback-call "
            "counters must remain zero for the full session.\r\n");
    } else {
        log_append(
            "Bounded external wrapper allocation is enabled. No more than the "
            "configured ");
        log_append_u32(g_generator_render_capacity);
        log_append(
            " total records may be introduced. A nonzero exhaustion flag, "
            "accounting mismatch, or free-list count above the introduced-record "
            "total is an anomaly.\r\n");
    }

    log_append("\r\nPlant render staging:\r\n");
    log_key_u32("Current staging index", snapshot->staging_index);
    log_key_u32("Staging A total", snapshot->staging_totals[0]);
    log_key_u32("Staging B total", snapshot->staging_totals[1]);
    for (index = 0; index < MANAGER_SURFACE_LIST_COUNT; ++index) {
        log_append("Staging A/B category ");
        log_append_u32(index);
        log_append(": ");
        log_append_u32(snapshot->staging_counts[0][index]);
        log_append("/");
        log_append_u32(snapshot->staging_counts[1][index]);
        log_append("\r\n");
    }

    log_append("\r\nDistance state:\r\n");
    log_key_float("Live Detail Distance", snapshot->detail_distance);
    log_key_float("Actual fade-close distance", snapshot->fade_close);
    log_key_float("Expected fade-close distance",
                  snapshot->expected_fade_close);
    log_key_float("Actual plant far distance", snapshot->plant_far);
    log_key_float("Expected plant far distance",
                  snapshot->expected_plant_far);
    log_key_float("Actual surface query radius", snapshot->query_radius);
    log_key_float("Expected surface query radius",
                  snapshot->expected_query_radius);
    log_key_float("Actual plant far squared",
                  snapshot->plant_far_squared);
    log_key_float("Expected plant far squared",
                  snapshot->expected_plant_far_squared);
    log_key_float("Manager position X", snapshot->position_x);
    log_key_float("Manager position Y", snapshot->position_y);
    log_key_float("Manager position Z", snapshot->position_z);
    log_key_float("Procedural source-query radius",
                  gu_source_bank_radius());
    log_key_u32(
        "Universal manager scale updates",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_updates, 0, 0));
    log_key_u32(
        "Universal manager scale failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_failures, 0, 0));
    log_key_u32(
        "Manager setter generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation, 0, 0));
    log_key_u32(
        "Manager setter applied generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_applied_generation, 0, 0));
    log_key_u32(
        "Manager setter calls",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_calls, 0, 0));
    log_key_u32(
        "Manager setter coalesced updates",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_coalesced_updates, 0, 0));
    log_key_u32(
        "Manager setter handoffs",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_handoffs, 0, 0));
    log_key_u32(
        "Manager setter waits",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_waits, 0, 0));
    log_key_u32(
        "Manager setter wait failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_wait_failures, 0, 0));
    log_key_u32(
        "Manager setter rundown failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_rundown_failures, 0, 0));
    log_key_u32(
        "Manager setter generation exhaustions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation_exhaustions,
            0, 0));
    log_key_u32(
        "Generated PROCOBJ +0x50 lifecycle implementation present",
        GENERATED_PROCOBJ_DISTANCE_LIFECYCLE_ENABLED);
    log_key_u32(
        "Generated PROCOBJ internal release observer active",
        g_report.universal_behavior_installed);
    log_key_u32(
        "Tuning governor observation-only",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    log_key_u32(
        "Generated PROCOBJ visibility distances scaled",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_scaled, 0, 0));
    log_key_u32(
        "Generated PROCOBJ distances clamped to source coverage",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_clamped, 0, 0));
    log_key_u32(
        "Generated distance restores total",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_restored, 0, 0));
    log_key_u32(
        "Generated distance release restores",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restores, 0, 0));
    log_key_u32(
        "Generated distance release restore mismatches",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restore_mismatches, 0, 0));
    log_key_u32(
        "Generated distance validation refusals",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_scaling_disabled, 0, 0));
    log_key_u32(
        "Live generated ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0));
    log_key_u32(
        "Live applied-distance ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_applied_active, 0, 0));
    log_key_u32(
        "Successful ownership publications",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publications, 0, 0));
    log_key_u32(
        "Successful owned release takes",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_release_takes, 0, 0));
    log_key_u32(
        "Unowned release observer skips",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_unowned_release_skips, 0, 0));
    log_key_u32(
        "Commit callbacks refused after shutdown",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_shutdown_commit_skips, 0, 0));
    log_key_u32(
        "Owned pointer reuse refusals",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_reuse_refusals, 0, 0));
    log_key_u32(
        "Publish/CAS rollback ownership losses",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cas_losses, 0, 0));
    log_key_u32(
        "Publish/CAS rollback cleanup failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cleanup_failures,
            0, 0));
    log_key_u32(
        "Third-party distance relinquishments",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_third_party_relinquishments, 0, 0));
    log_key_u32(
        "Ownership table high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.high_watermark, 0, 0));
    log_key_u32(
        "Ownership insert failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.insert_failures, 0, 0));
    log_key_u32(
        "Ownership token exhausted",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.token_exhausted, 0, 0));
    log_key_u32(
        "Ownership token exhaustion transitions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.token_exhaustions, 0, 0));
    log_key_u32(
        "Ownership entity-index failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.entity_index_failures, 0, 0));
    log_key_u32(
        "Ownership stale lookups",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.stale_lookups, 0, 0));
    log_key_u32(
        "Ownership lock contentions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_lock_contentions, 0, 0));
    log_key_u32(
        "Clean shutdown lifecycle balances",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.clean_shutdown_balances, 0, 0));
    log_key_u32(
        "Clean shutdown lifecycle imbalances",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.clean_shutdown_imbalances, 0, 0));
    log_key_u32(
        "Generated ownership publication failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_failures, 0, 0));
    log_key_u32("Procedural source-bank capacity",
                GU_SOURCE_BANK_CAPACITY);
    log_key_u32("Procedural source active frames",
                source_bank.active_frames);
    log_key_u32("Procedural source stock fallback frames",
                source_bank.stock_fallback_frames);
    log_key_u32("Procedural source bank exhaustions",
                source_bank.bank_exhaustions);
    log_key_u32("Procedural source bounds refusals",
                source_bank.bounds_refusals);
    log_key_u32("Procedural source frame mismatches",
                source_bank.frame_mismatches);
    log_key_u32("Procedural source TLS failures",
                source_bank.tls_failures);
    log_key_u32(
        "PLANT selected positive-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_positive_calls, 0, 0));
    log_key_u32(
        "PLANT neutral-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_neutral_calls, 0, 0));
    log_key_u32("PLANT live count-hook calls", plant_bank.count_calls);
    log_key_u32("PLANT invalid count calls", plant_bank.invalid_calls);
    log_key_u32("PLANT selected count calls", plant_bank.selected_calls);
    log_key_u32("PLANT authored candidates total",
                plant_bank.authored_count_total);
    log_key_u32("PLANT emitted candidates total",
                plant_bank.emitted_count_total);
    log_key_u32("PLANT 128-cap clamp events", plant_bank.clamp_events);
    log_key_u32("PLANT 128-cap saturations",
                plant_bank.capacity_saturations);
    log_key_u32("PLANT maximum per-source total",
                plant_bank.maximum_total_after);
    append_current_log_buffer();
}

static void write_telemetry_compact(
    const struct TelemetrySnapshot *snapshot,
    DWORD sample_number, DWORD high_surface, DWORD high_source)
{
    struct GuSourceBankCounters source_bank;
    struct GuPlantBankCounters plant_bank;
    DWORD event_type;
    DWORD lost_full = 0u;
    DWORD lost_contention = 0u;
    DWORD lost_disabled = 0u;
    ZeroMemory(&source_bank, sizeof(source_bank));
    ZeroMemory(&plant_bank, sizeof(plant_bank));
    gu_source_bank_get_counters(&source_bank);
    gu_plant_bank_get_counters(&plant_bank);
    for (event_type = 0u;
         event_type < GU_EVENT_TYPE_COUNT; ++event_type) {
        lost_full += (DWORD)InterlockedCompareExchange(
            &g_gu_event_ring.lost_full[event_type], 0, 0);
        lost_contention += (DWORD)InterlockedCompareExchange(
            &g_gu_event_ring.lost_contention[event_type], 0, 0);
        lost_disabled += (DWORD)InterlockedCompareExchange(
            &g_gu_event_ring.lost_disabled[event_type], 0, 0);
    }
    telemetry_begin_block("RUNTIME SAMPLE COMPACT");
    log_key_u32("Sample", sample_number);
    log_key_u32("Snapshot invariants valid", snapshot->invariant_valid);
    log_key_u32("Provider active", snapshot->source_active);
    log_key_u32("Provider high watermark", high_source);
    log_key_u32("Provider free", snapshot->source_free);
    log_key_u32("Surface active", snapshot->surface_active);
    log_key_u32("Surface high watermark", high_surface);
    log_key_u32("Surface free", snapshot->surface_free);
    log_key_u32(
        "Configured rendered-object capacity",
        g_generator_render_capacity);
    log_key_u32(
        "Governor pressure threshold",
        tuning_governor_near_capacity_threshold(
            g_generator_render_capacity));
    log_key_u32(
        "Generator runtime initialized",
        snapshot->generator_initialized);
    log_key_u32("Live rendered entities", snapshot->generator_active_rendered);
    log_key_u32(
        "Rendered-entity high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_generator_active_high_watermark, 0, 0));
    log_key_u32(
        "Active generator wrappers",
        snapshot->generator_active_wrapper_records);
    log_key_u32("Generator free wrappers", snapshot->generator_free_records);
    log_key_u32(
        "Extra wrapper records issued",
        snapshot->generator_extra_issued);
    log_key_u32(
        "Fallback allocator calls",
        snapshot->generator_fallback_calls);
    log_key_u32(
        "Expanded allocator exhaustion observed",
        snapshot->generator_extra_exhaustion_observed);
    log_key_u32("Generator staging now", snapshot->generator_staging_count);
    log_key_u32(
        "Generator staging high watermark",
        (DWORD)InterlockedCompareExchange(
            &g_generator_staging_high_watermark, 0, 0));
    log_key_float("Plant far distance", snapshot->plant_far);
    log_key_float("Surface query radius", snapshot->query_radius);
    log_key_float("Source enumeration radius", gu_source_bank_radius());
    log_key_u32(
        "Manager scale updates",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_updates, 0, 0));
    log_key_u32(
        "Manager scale failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_failures, 0, 0));
    log_key_u32(
        "Manager setter generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation, 0, 0));
    log_key_u32(
        "Manager setter applied generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_applied_generation, 0, 0));
    log_key_u32(
        "Manager setter calls",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_calls, 0, 0));
    log_key_u32(
        "Manager setter coalesced updates",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_coalesced_updates, 0, 0));
    log_key_u32(
        "Manager setter handoffs",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_handoffs, 0, 0));
    log_key_u32(
        "Manager setter wait failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_wait_failures, 0, 0));
    log_key_u32(
        "Manager setter rundown failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_rundown_failures, 0, 0));
    log_key_u32(
        "Manager setter generation exhaustions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation_exhaustions,
            0, 0));
    log_key_u32(
        "Universal install lifecycle state",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_state, 0, 0));
    log_key_u32(
        "Universal install lifecycle rejections",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_rejections, 0, 0));
    log_key_u32(
        "Generated PROCOBJ +0x50 lifecycle implementation present",
        GENERATED_PROCOBJ_DISTANCE_LIFECYCLE_ENABLED);
    log_key_u32(
        "Generated PROCOBJ internal release observer active",
        g_report.universal_behavior_installed);
    log_key_u32(
        "Tuning governor observation-only",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    log_key_u32(
        "Generated PROCOBJ distances scaled",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_scaled, 0, 0));
    log_key_u32(
        "Generated PROCOBJ distances clamped to source coverage",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_clamped, 0, 0));
    log_key_u32(
        "Generated distance restores total",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.entities_restored, 0, 0));
    log_key_u32(
        "Generated distance release restores",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restores, 0, 0));
    log_key_u32(
        "Generated distance release restore mismatches",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restore_mismatches, 0, 0));
    log_key_u32(
        "Generated distance validation refusals",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_scaling_disabled, 0, 0));
    log_key_u32(
        "Live generated ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0));
    log_key_u32(
        "Live applied-distance ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_applied_active, 0, 0));
    log_key_u32(
        "Successful ownership publications",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publications, 0, 0));
    log_key_u32(
        "Successful owned release takes",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_release_takes, 0, 0));
    log_key_u32(
        "Commit callbacks refused after shutdown",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_shutdown_commit_skips, 0, 0));
    log_key_u32(
        "Publish/CAS rollback ownership losses",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cas_losses, 0, 0));
    log_key_u32(
        "Publish/CAS rollback cleanup failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cleanup_failures,
            0, 0));
    log_key_u32(
        "Third-party distance relinquishments",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_third_party_relinquishments, 0, 0));
    log_key_u32(
        "Ownership entity-index failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.entity_index_failures, 0, 0));
    log_key_u32(
        "Ownership stale lookups",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.stale_lookups, 0, 0));
    log_key_u32(
        "Ownership token exhaustion transitions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.token_exhaustions, 0, 0));
    log_key_u32(
        "Ownership lock contentions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_lock_contentions, 0, 0));
    log_key_u32(
        "Ownership publication failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_failures, 0, 0));
    log_key_u32("Source bank active frames", source_bank.active_frames);
    log_key_u32("Source bank stock fallbacks",
                source_bank.stock_fallback_frames);
    log_key_u32("Source bank exhaustions", source_bank.bank_exhaustions);
    log_key_u32("Source bank bounds refusals",
                source_bank.bounds_refusals);
    log_key_u32(
        "PLANT selected positive-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_positive_calls, 0, 0));
    log_key_u32(
        "PLANT neutral-key calls",
        (DWORD)InterlockedCompareExchange(
            &g_plant_selector_neutral_calls, 0, 0));
    log_key_u32("PLANT live count-hook calls", plant_bank.count_calls);
    log_key_u32("PLANT selected count calls", plant_bank.selected_calls);
    log_key_u32("PLANT emitted candidates total",
                plant_bank.emitted_count_total);
    log_key_u32("PLANT clamp events", plant_bank.clamp_events);
    log_key_u32("PLANT capacity saturations",
                plant_bank.capacity_saturations);
    log_key_u32(
        "Model-distance status",
        g_report.model_distance_status);
    log_key_u32(
        "Procedural models draw-distance scaled",
        g_report.model_distance_models_scaled);
    log_key_u32(
        "Model-distance live verified",
        g_report.model_distance_live_verified);
    log_key_u32(
        "Model-distance live mismatches",
        g_report.model_distance_live_mismatches);
    log_key_u32(
        "Definition live mismatches",
        g_report.definition_live_mismatches);
    log_key_u32(
        "Tuning safety governor active",
        g_report.tuning_governor_active);
    log_key_u32(
        "Tuning safety governor triggered",
        g_report.tuning_governor_triggered);
    log_key_u32(
        "Tuning governor state",
        (DWORD)InterlockedCompareExchange(
            &g_tuning_governor_state, 0, 0));
    log_key_u32(
        "Tuning governor live writes",
        g_report.tuning_governor_live_writes);
    log_key_u32(
        "Tuning governor detection thread ID",
        g_report.tuning_governor_detection_thread_id);
    log_key_u32(
        "Universal hook events drained",
        (DWORD)InterlockedCompareExchange(
            &g_gu_event_summary.consumed_total, 0, 0));
    log_key_u32(
        "Universal entity-create events",
        (DWORD)InterlockedCompareExchange(
            &g_gu_event_summary.consumed_by_type[
                GU_EVENT_ENTITY_CREATE], 0, 0));
    log_key_u32(
        "Universal entity-release events",
        (DWORD)InterlockedCompareExchange(
            &g_gu_event_summary.consumed_by_type[
                GU_EVENT_ENTITY_RELEASE], 0, 0));
    log_key_float(
        "Last generated entity authored visibility distance",
        read_float((const BYTE *)&g_gu_event_summary.last_by_type[
            GU_EVENT_ENTITY_CREATE].values[4]));
    log_key_float(
        "Last generated entity scaled visibility distance",
        read_float((const BYTE *)&g_gu_event_summary.last_by_type[
            GU_EVENT_ENTITY_CREATE].values[5]));
    log_key_hex(
        "Last generated entity PROCOBJ surface owner",
        g_gu_event_summary.last_by_type[
            GU_EVENT_ENTITY_CREATE].values[2]);
    log_key_hex(
        "Last generated entity PLANT source owner",
        g_gu_event_summary.last_by_type[
            GU_EVENT_ENTITY_CREATE].values[3]);
    log_key_u32("Universal hook events lost full", lost_full);
    log_key_u32(
        "Universal hook events lost contention",
        lost_contention);
    log_key_u32(
        "Universal hook events lost disabled",
        lost_disabled);
    log_key_float("Manager X", snapshot->position_x);
    log_key_float("Manager Y", snapshot->position_y);
    log_key_u32(
        "Hang watchdog consecutive failed probes",
        (DWORD)InterlockedCompareExchange(
            &g_hang_consecutive_failures, 0, 0));
    log_key_u32(
        "Hang watchdog last Win32 error",
        (DWORD)InterlockedCompareExchange(
            &g_hang_last_probe_error, 0, 0));
    append_current_log_buffer();
}

static int telemetry_snapshot_has_anomaly(
    const struct TelemetrySnapshot *snapshot)
{
    return !snapshot->invariant_valid ||
        !snapshot->generator_invariant_valid ||
        snapshot->source_free == 0u ||
        snapshot->surface_free == 0u ||
        snapshot->generator_active_wrapper_records >
            g_generator_render_capacity ||
        snapshot->generator_active_rendered >
            g_generator_render_capacity ||
        snapshot->generator_staging_count >
            g_generator_render_capacity ||
        snapshot->generator_extra_exhaustion_observed != 0u ||
        g_report.definition_live_mismatches != 0u ||
        g_report.model_distance_live_mismatches != 0u;
}

static void write_telemetry_scheduled(
    const struct TelemetrySnapshot *snapshot,
    DWORD sample_number, DWORD high_surface, DWORD high_source)
{
    if (sample_number == 1u ||
        sample_number % TELEMETRY_FULL_SAMPLE_INTERVAL == 0u ||
        telemetry_snapshot_has_anomaly(snapshot)) {
        write_telemetry_sample(
            snapshot, sample_number, high_surface, high_source);
    } else {
        write_telemetry_compact(
            snapshot, sample_number, high_surface, high_source);
    }
}

static void poll_exhaustion_once(void)
{
    WORD provider_free;
    WORD surface_free;
    DWORD capacity;
    DWORD buffer0;

    tuning_safety_governor_poll();

    if (!g_manager ||
        !readable_address_range(g_manager, MANAGER_MINIMUM_SIZE)) {
        return;
    }
    capacity = read_u32(g_manager + MANAGER_CAPACITY_OFFSET);
    buffer0 = read_u32(g_manager + MANAGER_BUFFER0_OFFSET);
    if (capacity < MIN_CAPACITY ||
        capacity > MAX_OPERATIONAL_CAPACITY ||
        buffer0 == 0u) {
        return;
    }

    /*
     * A zero-initialized manager also has a zero provider-free head. Do not
     * treat that startup state as exhaustion; capacity plus buffer0 prove the
     * runtime manager has completed its pool initialization.
     */
    provider_free = read_u16(
        g_manager + MANAGER_SOURCE_FREE_HEAD_OFFSET);
    if (provider_free == 0u) {
        InterlockedExchange(&g_provider_exhaustion_observed, 1);
    }

    surface_free = read_u16(
        g_manager + MANAGER_SURFACE_FREE_HEAD_OFFSET);
    if (surface_free == 0u) {
        InterlockedExchange(&g_surface_exhaustion_observed, 1);
        return;
    }

    if ((DWORD)surface_free <= capacity &&
        readable_address_range(
            (BYTE *)buffer0 +
                ((DWORD)surface_free - 1u) *
                PROCEDURAL_RECORD_SIZE,
            PROCEDURAL_RECORD_SIZE) &&
        read_u16(
            (BYTE *)buffer0 +
                ((DWORD)surface_free - 1u) *
                PROCEDURAL_RECORD_SIZE +
                SURFACE_NEXT_OFFSET) == 0u) {
        InterlockedExchange(&g_surface_exhaustion_observed, 1);
    }
}

static void telemetry_polling_wait(DWORD milliseconds)
{
    DWORD elapsed = 0;
    while (elapsed < milliseconds &&
           InterlockedCompareExchange(
               &g_telemetry_stop, 0, 0) == 0) {
        DWORD remaining = milliseconds - elapsed;
        DWORD interval = remaining < TELEMETRY_EXHAUSTION_POLL_MS ?
            remaining : TELEMETRY_EXHAUSTION_POLL_MS;
        gu_event_drain(
            &g_gu_event_ring, &g_gu_event_summary,
            GU_EVENT_RING_CAPACITY);
        poll_exhaustion_once();
        Sleep(interval);
        elapsed += interval;
    }
}

static DWORD density_fingerprint_u32(DWORD hash, DWORD value)
{
    DWORD shift;
    for (shift = 0u; shift < 32u; shift += 8u) {
        hash = (hash ^ ((value >> shift) & 0xFFu)) * 16777619u;
    }
    return hash;
}

static int calculate_procobj_catalog_fingerprint(
    DWORD proc_count, DWORD *fingerprint)
{
    DWORD hash = density_fingerprint_u32(2166136261u, proc_count);
    DWORD index;

    if (!fingerprint) {
        return 0;
    }
    for (index = 0u; index < proc_count; ++index) {
        BYTE *record =
            g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        float spacing = read_float(
            record + DEFINITION_PROCOBJ_SPACING_OFFSET);
        float inverse = read_float(
            record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET);
        float distance_gate = read_float(
            record +
            DEFINITION_PROCOBJ_DISTANCE_GATE_SQUARED_OFFSET);
        if (!positive_finite_float(spacing) ||
            !positive_finite_float(inverse) ||
            !nonnegative_finite_float(distance_gate) ||
            inverse > DEFINITION_VALUE_MAX ||
            distance_gate > DEFINITION_VALUE_MAX) {
            return 0;
        }
        hash = density_fingerprint_u32(
            hash, read_u32(
                record + DEFINITION_PROCOBJ_SPACING_OFFSET));
        hash = density_fingerprint_u32(
            hash, read_u32(
                record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET));
        hash = density_fingerprint_u32(
            hash, read_u32(
                record +
                DEFINITION_PROCOBJ_DISTANCE_GATE_SQUARED_OFFSET));
        hash = density_fingerprint_u32(
            hash, (DWORD)record[DEFINITION_PROCOBJ_ALIGN_OFFSET]);
        hash = density_fingerprint_u32(
            hash, (DWORD)record[DEFINITION_PROCOBJ_USEGRID_OFFSET]);
        hash = density_fingerprint_u32(
            hash, (DWORD)record[DEFINITION_PROCOBJ_USESEED_OFFSET]);
        hash = density_fingerprint_u32(
            hash, (DWORD)record[DEFINITION_PROCOBJ_FLOATS_OFFSET]);
    }
    *fingerprint = hash;
    return 1;
}

static int calculate_plant_catalog_fingerprint(
    DWORD plant_count, DWORD *fingerprint)
{
    DWORD hash = density_fingerprint_u32(2166136261u, plant_count);
    DWORD index;

    if (!fingerprint) {
        return 0;
    }
    for (index = 0u; index < plant_count; ++index) {
        BYTE *record =
            g_definition_manager +
            DEFINITION_PLANT_RECORDS_OFFSET +
            index * DEFINITION_PLANT_RECORD_SIZE;
        float density = read_float(
            record + DEFINITION_PLANT_DENSITY_OFFSET);
        if (!nonnegative_finite_float(density) ||
            density > DEFINITION_VALUE_MAX) {
            return 0;
        }
        hash = density_fingerprint_u32(
            hash, read_u32(
                record + DEFINITION_PLANT_DENSITY_OFFSET));
    }
    *fingerprint = hash;
    return 1;
}

static const BYTE *select_stock_procobj_catalog(
    DWORD proc_count, DWORD actual_fingerprint,
    DWORD *selection, DWORD *expected_fingerprint)
{
    *selection = DENSITY_CATALOG_NONE;
    *expected_fingerprint = 0u;
    if (proc_count == STOCK_PROCOBJ_BASE_COUNT) {
        *expected_fingerprint = STOCK_PROCOBJ_BASE_FINGERPRINT;
        if (actual_fingerprint == STOCK_PROCOBJ_BASE_FINGERPRINT) {
            *selection = DENSITY_CATALOG_STOCK_BASE;
            return g_stock_procobj_base_classes;
        }
    } else if (proc_count == STOCK_PROCOBJ_EXPANDED_COUNT) {
        *expected_fingerprint = STOCK_PROCOBJ_EXPANDED_FINGERPRINT;
        if (actual_fingerprint ==
                STOCK_PROCOBJ_EXPANDED_FINGERPRINT) {
            *selection = DENSITY_CATALOG_STOCK_EXPANDED;
            return g_stock_procobj_expanded_classes;
        }
    }
    return NULL;
}

static DWORD density_class_mask_bit(BYTE density_class)
{
    switch (density_class) {
    case DENSITY_CLASS_GRASS:
        return DENSITY_CLASS_MASK_GRASS;
    case DENSITY_CLASS_VEGETATION:
        return DENSITY_CLASS_MASK_VEGETATION;
    case DENSITY_CLASS_CLUTTER:
        return DENSITY_CLASS_MASK_CLUTTER;
    case DENSITY_CLASS_OTHER:
        return DENSITY_CLASS_MASK_OTHER;
    default:
        return 0u;
    }
}

static void reset_definition_scaling_tracking(void)
{
    ZeroMemory(
        g_definition_original_proc_spacing,
        sizeof(g_definition_original_proc_spacing));
    ZeroMemory(
        g_definition_original_proc_inverse,
        sizeof(g_definition_original_proc_inverse));
    ZeroMemory(
        g_definition_scaled_proc_spacing,
        sizeof(g_definition_scaled_proc_spacing));
    ZeroMemory(
        g_definition_scaled_proc_inverse,
        sizeof(g_definition_scaled_proc_inverse));
    ZeroMemory(
        g_definition_effective_proc_density,
        sizeof(g_definition_effective_proc_density));
    ZeroMemory(
        g_definition_proc_class,
        sizeof(g_definition_proc_class));
    ZeroMemory(
        g_definition_proc_selected,
        sizeof(g_definition_proc_selected));
    ZeroMemory(
        g_definition_proc_usegrid,
        sizeof(g_definition_proc_usegrid));
    ZeroMemory(
        g_definition_proc_mutated,
        sizeof(g_definition_proc_mutated));
    ZeroMemory(
        g_definition_original_plant_density,
        sizeof(g_definition_original_plant_density));
    ZeroMemory(
        g_definition_scaled_plant_density,
        sizeof(g_definition_scaled_plant_density));
    g_report.procobj_catalog_selection = DENSITY_CATALOG_NONE;
    g_report.procobj_catalog_actual_fingerprint = 0u;
    g_report.procobj_catalog_expected_fingerprint = 0u;
    g_report.plant_catalog_actual_fingerprint = 0u;
    g_report.plant_catalog_expected_fingerprint = 0u;
    g_report.plant_catalog_match = 0u;
    g_report.definition_fingerprint_refused = 0u;
    g_report.procobj_definitions_selected = 0u;
    g_report.procobj_random_definitions_scaled = 0u;
    g_report.procobj_grid_definitions_scaled = 0u;
    g_report.procobj_definitions_unselected = 0u;
    g_report.procobj_definitions_scaled = 0u;
    g_report.plant_definitions_scaled = 0u;
    g_report.definition_live_fields_verified = 0u;
    g_report.definition_live_mismatches = 0u;
    g_report.definition_first_live_mismatch_kind = 0u;
    g_report.definition_first_live_mismatch_index = 0xFFFFFFFFu;
    g_report.plant_live_fields_observed = 0u;
    g_report.plant_live_observation_mismatches = 0u;
}

static int restore_loaded_definitions_to_original(void)
{
    DWORD proc_count;
    DWORD index;
    DWORD verified = 1u;

    if (g_report.definition_scaling_status != DEFINITION_SCALING_APPLIED) {
        return 1;
    }
    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MANAGER_REQUIRED_SIZE)) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_ROLLBACK_FAILED;
        return 0;
    }
    proc_count = g_report.procobj_definition_count;
    if (proc_count > DEFINITION_PROCOBJ_CAPACITY) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_ROLLBACK_FAILED;
        return 0;
    }

    for (index = 0u; index < proc_count; ++index) {
        BYTE *record;
        if (!g_definition_proc_mutated[index]) {
            continue;
        }
        record = g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        if (g_definition_proc_usegrid[index]) {
            write_float_atomic(
                record + DEFINITION_PROCOBJ_SPACING_OFFSET,
                g_definition_original_proc_spacing[index]);
        }
        write_float_atomic(
            record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET,
            g_definition_original_proc_inverse[index]);
    }
    for (index = 0u; index < proc_count; ++index) {
        BYTE *record;
        if (!g_definition_proc_mutated[index]) {
            continue;
        }
        record = g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        if ((g_definition_proc_usegrid[index] &&
             read_u32(
                 record + DEFINITION_PROCOBJ_SPACING_OFFSET) !=
                 read_u32((BYTE *)&
                     g_definition_original_proc_spacing[index])) ||
            read_u32(
                record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET) !=
                read_u32((BYTE *)&
                    g_definition_original_proc_inverse[index])) {
            verified = 0u;
            break;
        }
    }
    g_report.definition_live_fields_verified = 0u;
    g_report.definition_live_mismatches = 0u;
    if (verified) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_ROLLED_BACK_SAFETY;
        return 1;
    }
    g_report.definition_scaling_status =
        DEFINITION_SCALING_ROLLBACK_FAILED;
    return 0;
}

static int scale_loaded_definitions_for_catalog(
    DWORD proc_count, DWORD plant_count, const BYTE *classes)
{
    float proc_spacing_min = 0.0f;
    float scaled_spacing_min = 0.0f;
    float proc_inverse_sum = 0.0f;
    float scaled_inverse_sum = 0.0f;
    float plant_density_sum = 0.0f;
    float effective_density_min = 0.0f;
    float effective_density_max = 0.0f;
    DWORD index;
    DWORD selected_count = 0u;
    DWORD mutated_count = 0u;
    DWORD random_count = 0u;
    DWORD grid_count = 0u;
    int verified = 1;

    if (!classes || proc_count == 0u ||
        proc_count > DEFINITION_PROCOBJ_CAPACITY ||
        plant_count == 0u ||
        plant_count > DEFINITION_PLANT_CAPACITY) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_BAD_COUNTS;
        return 0;
    }
    if (!positive_finite_float(g_procobj_density_multiplier) ||
        g_procobj_density_multiplier <
            MIN_PROCOBJ_DENSITY_MULTIPLIER ||
        g_procobj_density_multiplier >
            MAX_PROCOBJ_DENSITY_MULTIPLIER) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_BAD_FLOAT;
        return 0;
    }

    for (index = 0u; index < proc_count; ++index) {
        BYTE *record =
            g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        float spacing = read_float(
            record + DEFINITION_PROCOBJ_SPACING_OFFSET);
        float inverse = read_float(
            record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET);
        float effective_multiplier = 1.0f;
        float new_spacing = spacing;
        float new_inverse = inverse;
        BYTE density_class = classes[index];
        BYTE usegrid =
            record[DEFINITION_PROCOBJ_USEGRID_OFFSET] ? 1u : 0u;
        int selected =
            (density_class_mask_bit(density_class) &
             g_density_class_mask) != 0u;

        if (!positive_finite_float(spacing) ||
            !positive_finite_float(inverse) ||
            inverse > DEFINITION_VALUE_MAX) {
            g_report.definition_scaling_status =
                DEFINITION_SCALING_BAD_FLOAT;
            return 0;
        }
        if (selected) {
            ++selected_count;
        }
        if (selected && g_procobj_density_multiplier != 1.0f) {
            effective_multiplier = g_procobj_density_multiplier;
            if (usegrid) {
                float factor = positive_square_root(
                    effective_multiplier);
                new_spacing = spacing / factor;
                new_inverse = inverse * effective_multiplier;
                ++grid_count;
            } else {
                new_inverse = inverse * effective_multiplier;
                ++random_count;
            }
            if (!positive_finite_float(new_spacing) ||
                !positive_finite_float(new_inverse) ||
                new_inverse > DEFINITION_VALUE_MAX) {
                g_report.definition_scaling_status =
                    DEFINITION_SCALING_BAD_FLOAT;
                return 0;
            }
            g_definition_proc_mutated[index] = 1u;
            ++mutated_count;
        }

        g_definition_original_proc_spacing[index] = spacing;
        g_definition_original_proc_inverse[index] = inverse;
        g_definition_scaled_proc_spacing[index] = new_spacing;
        g_definition_scaled_proc_inverse[index] = new_inverse;
        g_definition_effective_proc_density[index] =
            effective_multiplier;
        g_definition_proc_class[index] = density_class;
        g_definition_proc_selected[index] = selected ? 1u : 0u;
        g_definition_proc_usegrid[index] = usegrid;
        if (index == 0u || spacing < proc_spacing_min) {
            proc_spacing_min = spacing;
        }
        if (index == 0u || new_spacing < scaled_spacing_min) {
            scaled_spacing_min = new_spacing;
        }
        if (index == 0u ||
            effective_multiplier < effective_density_min) {
            effective_density_min = effective_multiplier;
        }
        if (index == 0u ||
            effective_multiplier > effective_density_max) {
            effective_density_max = effective_multiplier;
        }
        proc_inverse_sum += inverse;
        scaled_inverse_sum += new_inverse;
    }

    for (index = 0u; index < plant_count; ++index) {
        BYTE *record =
            g_definition_manager +
            DEFINITION_PLANT_RECORDS_OFFSET +
            index * DEFINITION_PLANT_RECORD_SIZE;
        float density = read_float(
            record + DEFINITION_PLANT_DENSITY_OFFSET);
        if (!nonnegative_finite_float(density) ||
            density > DEFINITION_VALUE_MAX) {
            g_report.definition_scaling_status =
                DEFINITION_SCALING_BAD_FLOAT;
            return 0;
        }
        g_definition_original_plant_density[index] = density;
        g_definition_scaled_plant_density[index] = density;
        plant_density_sum += density;
    }

    g_report.procobj_original_spacing_min_bits =
        read_u32((BYTE *)&proc_spacing_min);
    g_report.procobj_scaled_spacing_min_bits =
        read_u32((BYTE *)&scaled_spacing_min);
    g_report.procobj_original_inverse_sum_bits =
        read_u32((BYTE *)&proc_inverse_sum);
    g_report.procobj_scaled_inverse_sum_bits =
        read_u32((BYTE *)&scaled_inverse_sum);
    g_report.plant_original_density_sum_bits =
        read_u32((BYTE *)&plant_density_sum);
    g_report.plant_scaled_density_sum_bits =
        read_u32((BYTE *)&plant_density_sum);
    g_report.procobj_spacing_floor_clamps = 0u;
    g_report.procobj_value_limit_clamps = 0u;
    g_report.plant_value_limit_clamps = 0u;
    g_report.procobj_effective_density_min_bits =
        read_u32((BYTE *)&effective_density_min);
    g_report.procobj_effective_density_max_bits =
        read_u32((BYTE *)&effective_density_max);
    g_report.procobj_definitions_selected = selected_count;
    g_report.procobj_definitions_unselected =
        proc_count - selected_count;
    g_report.procobj_definitions_scaled = mutated_count;
    g_report.procobj_random_definitions_scaled = random_count;
    g_report.procobj_grid_definitions_scaled = grid_count;
    g_report.plant_definitions_scaled = 0u;

    if (mutated_count == 0u) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_NO_CHANGE;
        return 1;
    }

    for (index = 0u; index < proc_count; ++index) {
        BYTE *record;
        if (!g_definition_proc_mutated[index]) {
            continue;
        }
        record = g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        if (g_definition_proc_usegrid[index]) {
            write_float_atomic(
                record + DEFINITION_PROCOBJ_SPACING_OFFSET,
                g_definition_scaled_proc_spacing[index]);
        }
        write_float_atomic(
            record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET,
            g_definition_scaled_proc_inverse[index]);
    }

    for (index = 0u; index < proc_count; ++index) {
        BYTE *record;
        if (!g_definition_proc_mutated[index]) {
            continue;
        }
        record = g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        if ((g_definition_proc_usegrid[index] &&
             read_u32(
                 record + DEFINITION_PROCOBJ_SPACING_OFFSET) !=
                 read_u32((BYTE *)&
                     g_definition_scaled_proc_spacing[index])) ||
            read_u32(
                record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET) !=
                read_u32((BYTE *)&
                    g_definition_scaled_proc_inverse[index])) {
            verified = 0;
            break;
        }
    }
    if (!verified) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_APPLIED;
        if (!restore_loaded_definitions_to_original()) {
            g_report.definition_scaling_status =
                DEFINITION_SCALING_ROLLBACK_FAILED;
        } else {
            g_report.definition_scaling_status =
                DEFINITION_SCALING_WRITE_VERIFY_FAILED;
        }
        return 0;
    }

    g_report.definition_scaling_status =
        DEFINITION_SCALING_APPLIED;
    return 1;
}

static int scale_loaded_definitions(void)
{
    DWORD proc_count;
    DWORD plant_count;
    DWORD proc_fingerprint;
    DWORD plant_fingerprint;
    const BYTE *classes;

    reset_definition_scaling_tracking();
    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MANAGER_REQUIRED_SIZE)) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_BAD_MANAGER;
        return 0;
    }
    proc_count = read_u32(
        g_definition_manager +
        DEFINITION_PROCOBJ_COUNT_OFFSET);
    plant_count = read_u32(
        g_definition_manager +
        DEFINITION_PLANT_COUNT_OFFSET);
    g_report.procobj_definition_count = proc_count;
    g_report.plant_definition_count = plant_count;
    if (proc_count == 0u ||
        proc_count > DEFINITION_PROCOBJ_CAPACITY ||
        plant_count == 0u ||
        plant_count > DEFINITION_PLANT_CAPACITY) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_BAD_COUNTS;
        return 0;
    }
    if (!calculate_procobj_catalog_fingerprint(
            proc_count, &proc_fingerprint) ||
        !calculate_plant_catalog_fingerprint(
            plant_count, &plant_fingerprint)) {
        g_report.definition_scaling_status =
            DEFINITION_SCALING_BAD_FLOAT;
        return 0;
    }
    g_report.procobj_catalog_actual_fingerprint =
        proc_fingerprint;
    g_report.plant_catalog_actual_fingerprint =
        plant_fingerprint;
    if (plant_count == STOCK_PLANT_COUNT) {
        g_report.plant_catalog_expected_fingerprint =
            STOCK_PLANT_FINGERPRINT;
        g_report.plant_catalog_match =
            plant_fingerprint == STOCK_PLANT_FINGERPRINT;
    }
    classes = select_stock_procobj_catalog(
        proc_count, proc_fingerprint,
        &g_report.procobj_catalog_selection,
        &g_report.procobj_catalog_expected_fingerprint);
    if (!classes) {
        g_report.definition_fingerprint_refused = 1u;
        g_report.definition_scaling_status =
            DEFINITION_SCALING_FINGERPRINT_REFUSED;
        return 0;
    }
    return scale_loaded_definitions_for_catalog(
        proc_count, plant_count, classes);
}

static int definition_catalog_is_unloaded(void)
{
    DWORD proc_count;
    DWORD plant_count;
    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MANAGER_REQUIRED_SIZE)) {
        return 0;
    }
    proc_count = read_u32(
        g_definition_manager +
        DEFINITION_PROCOBJ_COUNT_OFFSET);
    plant_count = read_u32(
        g_definition_manager +
        DEFINITION_PLANT_COUNT_OFFSET);
    g_report.procobj_definition_count = proc_count;
    g_report.plant_definition_count = plant_count;
    return proc_count == 0u && plant_count == 0u;
}

static void __stdcall definition_loaded_engine_callback(void)
{
    LONG callback_count = InterlockedIncrement(
        &g_definition_callback_calls);
    LONG prior_state;
    LONG final_state;
    g_report.definition_callback_calls = (DWORD)callback_count;
    /* The hook can run while the installer is publishing ARMED.  A single
     * read followed by a different CAS permits the installer to arm between
     * those operations, leaving an escaped ARMED state after an unscaled
     * callback.  Retry the exact observed transition until either this
     * callback owns RUNNING or a terminal/duplicate state is visible. */
    for (;;) {
        prior_state = InterlockedCompareExchange(
            &g_definition_callback_state, 0, 0);
        if (prior_state == DEFINITION_CALLBACK_UNARMED) {
#ifdef GTAIVPOOL_TEST_EXPORTS
            if (InterlockedCompareExchange(
                    &g_definition_callback_test_pause_unarmed,
                    0, 0) != 0) {
                InterlockedExchange(
                    &g_definition_callback_test_unarmed_reached, 1);
                while (InterlockedCompareExchange(
                           &g_definition_callback_test_unarmed_release,
                           0, 0) == 0) {
                    Sleep(0u);
                }
            }
#endif
            if (InterlockedCompareExchange(
                    &g_definition_callback_state,
                    DEFINITION_CALLBACK_FAILED,
                    DEFINITION_CALLBACK_UNARMED) !=
                    DEFINITION_CALLBACK_UNARMED) {
                continue;
            }
            g_report.definition_scaling_status =
                DEFINITION_SCALING_CALLBACK_BEFORE_ACTIVATION;
            g_report.definition_callback_state =
                DEFINITION_CALLBACK_FAILED;
            return;
        }
        if (prior_state == DEFINITION_CALLBACK_ARMED) {
            if (InterlockedCompareExchange(
                    &g_definition_callback_state,
                    DEFINITION_CALLBACK_RUNNING,
                    DEFINITION_CALLBACK_ARMED) !=
                    DEFINITION_CALLBACK_ARMED) {
                continue;
            }
            break;
        }
        return;
    }

    g_report.definition_callback_state =
        DEFINITION_CALLBACK_RUNNING;
    g_report.definition_callback_thread_id =
        GetCurrentThreadId();
    if (
#ifdef GTAIVPOOL_TEST_EXPORTS
        InterlockedCompareExchange(
            &g_definition_callback_test_force_success, 0, 0) != 0 ||
#endif
        scale_loaded_definitions()) {
        final_state = DEFINITION_CALLBACK_COMPLETE;
    } else {
        final_state = DEFINITION_CALLBACK_FAILED;
    }
    g_report.model_distance_status =
        MODEL_DISTANCE_RETIRED_GLOBAL_MUTATION;
    g_report.tuning_governor_active =
        g_report.definition_scaling_status ==
            DEFINITION_SCALING_APPLIED;
    if (InterlockedCompareExchange(
            &g_definition_callback_state,
            final_state,
            DEFINITION_CALLBACK_RUNNING) ==
            DEFINITION_CALLBACK_RUNNING) {
        g_report.definition_callback_state = (DWORD)final_state;
    } else {
        g_report.definition_callback_state = (DWORD)
            InterlockedCompareExchange(
                &g_definition_callback_state, 0, 0);
    }
}

static void verify_definition_scaling_live(void)
{
    DWORD proc_count;
    DWORD plant_count;
    DWORD index;
    DWORD verified = 0u;
    DWORD mismatches = 0u;
    DWORD first_kind = 0u;
    DWORD first_index = 0xFFFFFFFFu;
    DWORD plant_observed = 0u;
    DWORD plant_mismatches = 0u;

    if (g_report.definition_scaling_status !=
            DEFINITION_SCALING_APPLIED) {
        return;
    }
    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MANAGER_REQUIRED_SIZE)) {
        g_report.definition_live_fields_verified = 0u;
        g_report.definition_live_mismatches = 1u;
        g_report.definition_first_live_mismatch_kind = 4u;
        g_report.definition_first_live_mismatch_index =
            0xFFFFFFFFu;
        return;
    }
    proc_count = g_report.procobj_definition_count;
    plant_count = g_report.plant_definition_count;
    if (proc_count > DEFINITION_PROCOBJ_CAPACITY ||
        plant_count > DEFINITION_PLANT_CAPACITY) {
        g_report.definition_live_fields_verified = 0u;
        g_report.definition_live_mismatches = 1u;
        g_report.definition_first_live_mismatch_kind = 5u;
        g_report.definition_first_live_mismatch_index =
            0xFFFFFFFFu;
        return;
    }

    for (index = 0u; index < proc_count; ++index) {
        BYTE *record;
        if (!g_definition_proc_mutated[index]) {
            continue;
        }
        record = g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            index * DEFINITION_PROCOBJ_RECORD_SIZE;
        if (g_definition_proc_usegrid[index]) {
            if (read_u32(
                    record + DEFINITION_PROCOBJ_SPACING_OFFSET) ==
                    read_u32((BYTE *)&
                        g_definition_scaled_proc_spacing[index])) {
                ++verified;
            } else {
                ++mismatches;
                if (first_kind == 0u) {
                    first_kind = 1u;
                    first_index = index;
                }
            }
        }
        if (read_u32(
                record + DEFINITION_PROCOBJ_INVERSE_SQUARE_OFFSET) ==
                read_u32((BYTE *)&
                    g_definition_scaled_proc_inverse[index])) {
            ++verified;
        } else {
            ++mismatches;
            if (first_kind == 0u) {
                first_kind = 2u;
                first_index = index;
            }
        }
    }
    for (index = 0u; index < plant_count; ++index) {
        BYTE *record =
            g_definition_manager +
            DEFINITION_PLANT_RECORDS_OFFSET +
            index * DEFINITION_PLANT_RECORD_SIZE;
        ++plant_observed;
        if (read_u32(
                record + DEFINITION_PLANT_DENSITY_OFFSET) !=
                read_u32((BYTE *)&
                    g_definition_original_plant_density[index])) {
            ++plant_mismatches;
        }
    }
    g_report.definition_live_fields_verified = verified;
    g_report.definition_live_mismatches = mismatches;
    g_report.definition_first_live_mismatch_kind = first_kind;
    g_report.definition_first_live_mismatch_index = first_index;
    g_report.plant_live_fields_observed = plant_observed;
    g_report.plant_live_observation_mismatches = plant_mismatches;
}

static int scale_procedural_model_draw_distances_from_table(
    BYTE *model_info_table)
{
    DWORD proc_count;
    DWORD definition_index;
    DWORD unique_count = 0u;
    DWORD duplicate_count = 0u;
    float original_min = 0.0f;
    float original_max = 0.0f;
    float scaled_min = 0.0f;
    float scaled_max = 0.0f;
    float query_radius;
    DWORD clamped_count = 0u;

    ZeroMemory(
        g_model_distance_model_indices,
        sizeof(g_model_distance_model_indices));
    ZeroMemory(
        g_model_distance_model_pointers,
        sizeof(g_model_distance_model_pointers));
    ZeroMemory(
        g_model_distance_original,
        sizeof(g_model_distance_original));
    ZeroMemory(
        g_model_distance_scaled,
        sizeof(g_model_distance_scaled));
    g_report.model_distance_status = MODEL_DISTANCE_WAITING;
    g_report.model_info_table_address = (DWORD)model_info_table;
    g_report.model_distance_definitions_examined = 0u;
    g_report.model_distance_unique_models = 0u;
    g_report.model_distance_duplicate_references = 0u;
    g_report.model_distance_models_scaled = 0u;
    g_report.model_distance_invalid_definition_index = 0xFFFFFFFFu;
    g_report.model_distance_invalid_model_index = 0xFFFFFFFFu;
    g_report.model_distance_invalid_model_pointer = 0u;
    g_report.model_distance_invalid_draw_distance_bits = 0u;
    g_report.model_distance_write_mismatch_index = 0xFFFFFFFFu;
    g_report.model_distance_rollback_verified = 0u;
    g_report.model_distance_query_radius_bits = 0u;
    g_report.model_distance_models_clamped = 0u;
    g_report.model_distance_live_verified = 0u;
    g_report.model_distance_live_mismatches = 0u;
    g_report.model_distance_first_live_mismatch = 0xFFFFFFFFu;

    if (!g_definition_manager ||
        !readable_address_range(
            g_definition_manager,
            DEFINITION_MANAGER_REQUIRED_SIZE)) {
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_DEFINITION_MANAGER;
        return 0;
    }
    proc_count = read_u32(
        g_definition_manager +
        DEFINITION_PROCOBJ_COUNT_OFFSET);
    if (proc_count == 0u ||
        proc_count > DEFINITION_PROCOBJ_CAPACITY) {
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_DEFINITION_COUNT;
        return 0;
    }
    if (!model_info_table ||
        !readable_address_range(
            model_info_table,
            MODEL_INFO_TABLE_REQUIRED_SIZE)) {
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_MODEL_TABLE;
        return 0;
    }
    if (!g_manager ||
        !readable_address_range(g_manager, MANAGER_MINIMUM_SIZE)) {
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_QUERY_RADIUS;
        return 0;
    }
    query_radius = read_float(
        g_manager + MANAGER_QUERY_RADIUS_OFFSET);
    if (!positive_finite_float(query_radius) ||
        query_radius > MODEL_DRAW_DISTANCE_MAX) {
        g_report.model_distance_query_radius_bits =
            read_u32((BYTE *)&query_radius);
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_QUERY_RADIUS;
        return 0;
    }
    g_report.model_distance_query_radius_bits =
        read_u32((BYTE *)&query_radius);

    for (definition_index = 0u;
         definition_index < proc_count;
         ++definition_index) {
        BYTE *definition =
            g_definition_manager +
            DEFINITION_PROCOBJ_RECORDS_OFFSET +
            definition_index *
                DEFINITION_PROCOBJ_RECORD_SIZE;
        DWORD model_index = read_u32(
            definition +
            DEFINITION_PROCOBJ_MODEL_INDEX_OFFSET);
        DWORD existing;
        BYTE *model_info;
        float original;
        float scaled;

        g_report.model_distance_definitions_examined =
            definition_index + 1u;
        if (model_index >= MODEL_INFO_POINTER_CAPACITY) {
            g_report.model_distance_invalid_definition_index =
                definition_index;
            g_report.model_distance_invalid_model_index =
                model_index;
            g_report.model_distance_status =
                MODEL_DISTANCE_BAD_MODEL_INDEX;
            return 0;
        }
        for (existing = 0u;
             existing < unique_count;
             ++existing) {
            if (g_model_distance_model_indices[existing] ==
                    model_index) {
                ++duplicate_count;
                break;
            }
        }
        if (existing < unique_count) {
            continue;
        }
        if (unique_count >= MODEL_DISTANCE_RECORD_CAPACITY) {
            g_report.model_distance_status =
                MODEL_DISTANCE_BAD_DEFINITION_COUNT;
            return 0;
        }

        model_info = (BYTE *)read_u32(
            model_info_table +
            model_index * MODEL_INFO_POINTER_SIZE);
        if (!model_info ||
            !readable_address_range(
                model_info, MODEL_INFO_REQUIRED_SIZE) ||
            !writable_address_range(
                model_info + MODEL_INFO_DRAW_DISTANCE_OFFSET,
                4u)) {
            g_report.model_distance_invalid_definition_index =
                definition_index;
            g_report.model_distance_invalid_model_index =
                model_index;
            g_report.model_distance_invalid_model_pointer =
                (DWORD)model_info;
            g_report.model_distance_status =
                MODEL_DISTANCE_BAD_MODEL_POINTER;
            return 0;
        }
        original = read_float(
            model_info + MODEL_INFO_DRAW_DISTANCE_OFFSET);
        if (!positive_finite_float(original) ||
            original > MODEL_DRAW_DISTANCE_MAX) {
            g_report.model_distance_invalid_definition_index =
                definition_index;
            g_report.model_distance_invalid_model_index =
                model_index;
            g_report.model_distance_invalid_model_pointer =
                (DWORD)model_info;
            g_report.model_distance_invalid_draw_distance_bits =
                read_u32((BYTE *)&original);
            g_report.model_distance_status =
                MODEL_DISTANCE_BAD_DRAW_DISTANCE;
            return 0;
        }
        scaled = original;
        if (g_report.distance_multiplier > 1u) {
            if (original < query_radius) {
                float requested = original *
                    (float)g_report.distance_multiplier;
                if (!positive_finite_float(requested) ||
                    requested > query_radius) {
                    scaled = query_radius;
                    ++clamped_count;
                } else {
                    scaled = requested;
                }
            } else {
                ++clamped_count;
            }
        }

        g_model_distance_model_indices[unique_count] =
            model_index;
        g_model_distance_model_pointers[unique_count] =
            model_info;
        g_model_distance_original[unique_count] = original;
        g_model_distance_scaled[unique_count] = scaled;
        if (unique_count == 0u || original < original_min) {
            original_min = original;
        }
        if (unique_count == 0u || original > original_max) {
            original_max = original;
        }
        if (unique_count == 0u || scaled < scaled_min) {
            scaled_min = scaled;
        }
        if (unique_count == 0u || scaled > scaled_max) {
            scaled_max = scaled;
        }
        ++unique_count;
    }

    g_report.model_distance_unique_models = unique_count;
    g_report.model_distance_duplicate_references =
        duplicate_count;
    g_report.model_distance_models_clamped = clamped_count;
    g_report.model_distance_original_min_bits =
        read_u32((BYTE *)&original_min);
    g_report.model_distance_original_max_bits =
        read_u32((BYTE *)&original_max);
    g_report.model_distance_scaled_min_bits =
        read_u32((BYTE *)&scaled_min);
    g_report.model_distance_scaled_max_bits =
        read_u32((BYTE *)&scaled_max);

    if (g_report.distance_multiplier == 1u) {
        g_report.model_distance_rollback_verified = 1u;
        g_report.model_distance_status =
            MODEL_DISTANCE_NO_CHANGE;
        return 1;
    }

    for (definition_index = 0u;
         definition_index < unique_count;
         ++definition_index) {
        write_float_atomic(
            g_model_distance_model_pointers[definition_index] +
                MODEL_INFO_DRAW_DISTANCE_OFFSET,
            g_model_distance_scaled[definition_index]);
    }
    for (definition_index = 0u;
         definition_index < unique_count;
         ++definition_index) {
        BYTE *field =
            g_model_distance_model_pointers[definition_index] +
            MODEL_INFO_DRAW_DISTANCE_OFFSET;
        if (read_u32(field) !=
                read_u32((BYTE *)&
                    g_model_distance_scaled[definition_index])) {
            DWORD rollback_index;
            DWORD rollback_ok = 1u;
            g_report.model_distance_write_mismatch_index =
                definition_index;
            for (rollback_index = 0u;
                 rollback_index < unique_count;
                 ++rollback_index) {
                write_float_atomic(
                    g_model_distance_model_pointers[rollback_index] +
                        MODEL_INFO_DRAW_DISTANCE_OFFSET,
                    g_model_distance_original[rollback_index]);
            }
            for (rollback_index = 0u;
                 rollback_index < unique_count;
                 ++rollback_index) {
                if (read_u32(
                        g_model_distance_model_pointers[rollback_index] +
                        MODEL_INFO_DRAW_DISTANCE_OFFSET) !=
                    read_u32((BYTE *)&
                        g_model_distance_original[rollback_index])) {
                    rollback_ok = 0u;
                    break;
                }
            }
            g_report.model_distance_rollback_verified =
                rollback_ok;
            g_report.model_distance_status = rollback_ok ?
                MODEL_DISTANCE_WRITE_VERIFY_FAILED :
                MODEL_DISTANCE_ROLLBACK_FAILED;
            return 0;
        }
    }

    g_report.model_distance_models_scaled = unique_count;
    g_report.model_distance_rollback_verified = 1u;
    g_report.model_distance_status = MODEL_DISTANCE_APPLIED;
    return 1;
}

static void verify_procedural_model_draw_distances_live(void)
{
    DWORD index;
    DWORD verified = 0u;
    DWORD mismatches = 0u;
    DWORD first_mismatch = 0xFFFFFFFFu;

    if (g_report.model_distance_status != MODEL_DISTANCE_APPLIED) {
        return;
    }
    for (index = 0u;
         index < g_report.model_distance_models_scaled &&
         index < MODEL_DISTANCE_RECORD_CAPACITY;
         ++index) {
        BYTE *field;
        if (!g_model_distance_model_pointers[index]) {
            ++mismatches;
            if (first_mismatch == 0xFFFFFFFFu) {
                first_mismatch = index;
            }
            continue;
        }
        field = g_model_distance_model_pointers[index] +
            MODEL_INFO_DRAW_DISTANCE_OFFSET;
        if (readable_address_range(field, 4u) &&
            read_u32(field) ==
                read_u32((BYTE *)&g_model_distance_scaled[index])) {
            ++verified;
        } else {
            ++mismatches;
            if (first_mismatch == 0xFFFFFFFFu) {
                first_mismatch = index;
            }
        }
    }
    g_report.model_distance_live_verified = verified;
    g_report.model_distance_live_mismatches = mismatches;
    g_report.model_distance_first_live_mismatch = first_mismatch;
}

static DWORD tuning_governor_near_capacity_threshold(DWORD capacity)
{
    DWORD margin;

    if (capacity < VANILLA_RENDERED_OBJECT_CAPACITY) {
        capacity = VANILLA_RENDERED_OBJECT_CAPACITY;
    }
    margin = capacity / TUNING_GOVERNOR_MARGIN_DIVISOR;
    if (margin < TUNING_GOVERNOR_MIN_MARGIN) {
        margin = TUNING_GOVERNOR_MIN_MARGIN;
    }
    if (margin > TUNING_GOVERNOR_MAX_MARGIN) {
        margin = TUNING_GOVERNOR_MAX_MARGIN;
    }
    if (capacity <= margin) {
        return capacity;
    }
    return capacity - margin;
}

static const char *tuning_governor_reason_text(DWORD reason)
{
    switch (reason) {
    case TUNING_GOVERNOR_REASON_PRESSURE:
        return "SUSTAINED_RENDERED_OR_STAGING_PRESSURE";
    case TUNING_GOVERNOR_REASON_DENSITY_MISMATCH:
        return "LIVE_DENSITY_WRITE_MISMATCH";
    case TUNING_GOVERNOR_REASON_DISTANCE_MISMATCH:
        return "LIVE_MODEL_DISTANCE_WRITE_MISMATCH";
    case TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID:
        return "GENERATOR_ACCOUNTING_OR_CAPACITY_INVALID";
    case TUNING_GOVERNOR_REASON_EXPANSION_EXHAUSTED:
        return "BOUNDED_EXTERNAL_WRAPPER_POOL_EXHAUSTED";
    default:
        return "UNKNOWN_GOVERNOR_REASON";
    }
}

static void write_tuning_governor_event(void)
{
    telemetry_begin_block("TUNING SAFETY GOVERNOR");
    log_key_u32("Governor triggered", g_report.tuning_governor_triggered);
    log_key_u32("Trigger reason", g_report.tuning_governor_reason);
    log_key_text(
        "Trigger reason text",
        tuning_governor_reason_text(g_report.tuning_governor_reason));
    log_key_u32(
        "Configured rendered-object capacity",
        g_generator_render_capacity);
    log_key_u32(
        "Dynamic rendered/staging pressure threshold",
        tuning_governor_near_capacity_threshold(
            g_generator_render_capacity));
    log_key_u32(
        "Sustained pressure polls",
        g_report.tuning_governor_pressure_polls);
    log_key_u32(
        "Active wrappers at trigger",
        g_report.tuning_governor_trigger_active_wrappers);
    log_key_u32(
        "Staging entries at trigger",
        g_report.tuning_governor_trigger_staging_count);
    log_key_u32(
        "Rendered entities at trigger",
        g_report.tuning_governor_trigger_rendered_entities);
    log_key_u32(
        "External wrapper records issued",
        (DWORD)InterlockedCompareExchange(
            &g_generator_extra_issued, 0, 0));
    log_key_u32(
        "Fallback allocator calls",
        (DWORD)InterlockedCompareExchange(
            &g_generator_fallback_calls, 0, 0));
    log_key_u32(
        "Expanded allocator exhaustion observed",
        (DWORD)InterlockedCompareExchange(
            &g_generator_extra_exhaustion_observed, 0, 0));
    log_key_float(
        "Requested distance multiplier",
        read_float((const BYTE *)&
            g_report.corrected_distance_multiplier_bits));
    log_key_hex(
        "Requested distance multiplier bits",
        g_report.corrected_distance_multiplier_bits);
    log_key_float(
        "Requested plant-density multiplier",
        read_float((const BYTE *)&
            g_report.corrected_plant_density_multiplier_bits));
    log_key_hex(
        "Requested plant-density multiplier bits",
        g_report.corrected_plant_density_multiplier_bits);
    log_key_float(
        "Requested procedural-object-density multiplier",
        read_float((BYTE *)&
            g_report.procobj_density_multiplier_bits));
    log_key_hex(
        "Requested procedural-object-density multiplier bits",
        g_report.procobj_density_multiplier_bits);
    log_key_u32(
        "Requested density class mask",
        g_report.density_class_mask);
    log_key_text(
        "Definition state",
        definition_status_text(g_report.definition_scaling_status));
    log_key_text(
        "Model-distance state",
        model_distance_status_text(g_report.model_distance_status));
    log_key_u32(
        "Observation-only monitor",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    log_key_u32(
        "Monitor state",
        g_report.tuning_governor_state);
    log_key_u32(
        "Live writes attempted by monitor",
        g_report.tuning_governor_live_writes);
    log_key_u32(
        "Detection thread ID",
        g_report.tuning_governor_detection_thread_id);
    log_key_u32(
        "Restart required",
        g_report.tuning_governor_state ==
            TUNING_GOVERNOR_STATE_RESTART_REQUIRED);
    log_append(
        "The 40,960 core procedural-surface pool and the configured bounded "
        "rendered-object capacity remain installed. The monitor performed zero "
        "live writes and did not claim an in-session repair. This launch is a "
        "failed test: exit GTA IV normally, preserve the log, and restart with "
        "neutral tuning before further testing.\r\n");
    append_current_log_buffer();
}

static void publish_tuning_governor_event_if_pending(void)
{
    if (InterlockedCompareExchange(
            &g_tuning_governor_event_pending, 0, 1) == 1) {
        write_tuning_governor_event();
    }
}

static void trigger_tuning_safety_governor(
    DWORD reason, DWORD active_wrappers,
    DWORD staging_count, DWORD rendered_entities)
{
    if (g_report.tuning_governor_active == 0u ||
        InterlockedCompareExchange(
            &g_tuning_governor_triggered, 1, 0) != 0) {
        return;
    }
    g_report.tuning_governor_triggered = 1u;
    g_report.tuning_governor_reason = reason;
    g_report.tuning_governor_pressure_polls =
        (DWORD)InterlockedCompareExchange(
            &g_tuning_governor_pressure_polls, 0, 0);
    g_report.tuning_governor_trigger_active_wrappers =
        active_wrappers;
    g_report.tuning_governor_trigger_staging_count =
        staging_count;
    g_report.tuning_governor_trigger_rendered_entities =
        rendered_entities;
    g_report.tuning_governor_live_writes = 0u;
    g_report.tuning_governor_detection_thread_id =
        GetCurrentThreadId();
    g_report.tuning_governor_state =
        TUNING_GOVERNOR_STATE_RESTART_REQUIRED;
    g_report.tuning_governor_active = 0u;
    /*
     * Telemetry is observation-only. It never repairs live game state from a
     * background thread and it never promises a later game-thread checkpoint.
     * Publish only after every diagnostic field is complete. The owner test
     * protocol treats any trigger as a failed launch and requires a restart.
     */
    InterlockedExchange(
        &g_tuning_governor_state,
        TUNING_GOVERNOR_STATE_RESTART_REQUIRED);
    InterlockedExchange(&g_tuning_governor_event_pending, 1);
}

static void tuning_safety_governor_poll(void)
{
    DWORD capacity;
    DWORD staging_capacity;
    DWORD staging_buffer;
    DWORD introduced_records;
    DWORD extra_issued;
    DWORD free_records;
    DWORD active_wrappers;
    DWORD staging_count;
    DWORD rendered_entities;
    DWORD near_capacity;
    DWORD expansion_exhausted;
    DWORD pressure;
    DWORD pressure_reason;
    LONG polls;

    if (g_report.tuning_governor_active == 0u ||
        InterlockedCompareExchange(
            &g_tuning_governor_triggered, 0, 0) != 0 ||
        !g_generator_manager ||
        !readable_address_range(
            g_generator_manager,
            GENERATOR_MANAGER_MINIMUM_SIZE)) {
        return;
    }

    capacity = g_generator_render_capacity;
    if (capacity < VANILLA_RENDERED_OBJECT_CAPACITY ||
        capacity > MAX_RENDERED_OBJECT_CAPACITY) {
        trigger_tuning_safety_governor(
            TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID,
            0u, 0u, 0u);
        return;
    }

    staging_capacity = read_u32(
        g_generator_manager + GENERATOR_STAGING_CAPACITY_OFFSET);
    staging_buffer = read_u32(
        g_generator_manager + GENERATOR_STAGING_BUFFER_OFFSET);
    if (staging_capacity == 0u && staging_buffer == 0u) {
        /* Generator constructor has not run yet. This is not pressure. */
        InterlockedExchange(&g_tuning_governor_pressure_polls, 0);
        g_report.tuning_governor_pressure_polls = 0u;
        return;
    }
    if (staging_capacity != capacity || staging_buffer == 0u) {
        trigger_tuning_safety_governor(
            TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID,
            0u, staging_capacity, 0u);
        return;
    }

    extra_issued = (DWORD)InterlockedCompareExchange(
        &g_generator_extra_issued, 0, 0);
    if (extra_issued >
            capacity - VANILLA_RENDERED_OBJECT_CAPACITY) {
        trigger_tuning_safety_governor(
            TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID,
            extra_issued, staging_capacity, capacity);
        return;
    }
    introduced_records =
        VANILLA_RENDERED_OBJECT_CAPACITY + extra_issued;
    free_records = read_u32(
        g_generator_manager + GENERATOR_FREE_COUNT_OFFSET);
    if (free_records > introduced_records ||
        free_records > capacity) {
        trigger_tuning_safety_governor(
            TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID,
            introduced_records, free_records, capacity);
        return;
    }

    active_wrappers = introduced_records - free_records;
    staging_count = (DWORD)read_u16(
        g_generator_manager + GENERATOR_STAGING_COUNT_OFFSET);
    rendered_entities = read_u32(
        g_generator_manager +
        GENERATOR_ACTIVE_RENDERED_COUNT_OFFSET);
    if (staging_count > capacity || rendered_entities > capacity) {
        trigger_tuning_safety_governor(
            TUNING_GOVERNOR_REASON_ACCOUNTING_INVALID,
            active_wrappers, staging_count, rendered_entities);
        return;
    }

    near_capacity =
        tuning_governor_near_capacity_threshold(capacity);
    expansion_exhausted =
        (DWORD)InterlockedCompareExchange(
            &g_generator_extra_exhaustion_observed, 0, 0);

    /*
     * Wrapper occupancy by itself is not a reliable overload signal. GTA IV
     * can legitimately have all 512 embedded records off the free list while
     * rendered and staging counts are still zero during startup/transition.
     * BLD0035 incorrectly treated that state as pressure and rolled back every
     * tuning request five polls after it was applied. Only actual
     * rendered/staging saturation or a genuinely exhausted expanded allocator
     * is allowed to latch a restart-required test failure now.
     */
    pressure_reason = TUNING_GOVERNOR_REASON_PRESSURE;
    pressure =
        staging_count >= near_capacity ||
        rendered_entities >= near_capacity;
    if (capacity > VANILLA_RENDERED_OBJECT_CAPACITY &&
        expansion_exhausted != 0u &&
        extra_issued ==
            capacity - VANILLA_RENDERED_OBJECT_CAPACITY &&
        free_records == 0u) {
        pressure = 1u;
        pressure_reason =
            TUNING_GOVERNOR_REASON_EXPANSION_EXHAUSTED;
    }
    if (!pressure) {
        InterlockedExchange(&g_tuning_governor_pressure_polls, 0);
        g_report.tuning_governor_pressure_polls = 0u;
        return;
    }
    polls = InterlockedIncrement(&g_tuning_governor_pressure_polls);
    g_report.tuning_governor_pressure_polls = (DWORD)polls;
    if ((DWORD)polls >= TUNING_GOVERNOR_SUSTAINED_POLLS) {
        trigger_tuning_safety_governor(
            pressure_reason,
            active_wrappers, staging_count, rendered_entities);
    }
}

static int scale_procedural_model_draw_distances(void)
{
    BYTE *model_info_table;

    if (!g_build_profile ||
        g_report.image_base == 0u ||
        VERIFIED_MODEL_INFO_TABLE_RVA == 0u ||
        VERIFIED_MODEL_INFO_TABLE_RVA >=
            g_report.size_of_image ||
        MODEL_INFO_TABLE_REQUIRED_SIZE >
            g_report.size_of_image -
            VERIFIED_MODEL_INFO_TABLE_RVA) {
        g_report.model_distance_status =
            MODEL_DISTANCE_BAD_MODEL_TABLE;
        return 0;
    }
    g_report.model_info_table_rva =
        VERIFIED_MODEL_INFO_TABLE_RVA;
    model_info_table =
        (BYTE *)g_report.image_base +
        VERIFIED_MODEL_INFO_TABLE_RVA;
    return scale_procedural_model_draw_distances_from_table(
        model_info_table);
}

static void write_model_distance_scaling_result(void)
{
    DWORD index;
    telemetry_begin_block(
        "PROCEDURAL MODEL DRAW-DISTANCE SCALING");
    log_key_u32(
        "Status code", g_report.model_distance_status);
    log_key_text(
        "Status",
        model_distance_status_text(
            g_report.model_distance_status));
    log_key_u32(
        "Distance multiplier",
        g_report.distance_multiplier);
    log_key_text(
        "Distance mechanism",
        "RETIRED - no model-global CBaseModelInfo fields are written");
    log_key_text(
        "Legacy manager-radius multiplier",
        "OLD CONSTANT PATCH DISABLED - UNIVERSAL LIVE MANAGER/SOURCE HOOKS OWN RANGE");
    log_key_hex(
        "Model-info pointer table RVA",
        g_report.model_info_table_rva);
    log_key_hex(
        "Model-info pointer table address",
        g_report.model_info_table_address);
    log_key_u32(
        "PROCOBJ definitions examined",
        g_report.model_distance_definitions_examined);
    log_key_u32(
        "Unique procedural models validated",
        g_report.model_distance_unique_models);
    log_key_u32(
        "Duplicate model references",
        g_report.model_distance_duplicate_references);
    log_key_u32(
        "Model draw distances scaled",
        g_report.model_distance_models_scaled);
    log_key_float(
        "Vanilla procedural query-radius ceiling",
        read_float((BYTE *)&
            g_report.model_distance_query_radius_bits));
    log_key_u32(
        "Models clamped to safe generated coverage",
        g_report.model_distance_models_clamped);
    log_key_u32(
        "Live model distances verified",
        g_report.model_distance_live_verified);
    log_key_u32(
        "Live model-distance mismatches",
        g_report.model_distance_live_mismatches);
    log_key_u32(
        "First live mismatch index",
        g_report.model_distance_first_live_mismatch);
    log_key_float(
        "Minimum original model draw distance",
        read_float((BYTE *)&
            g_report.model_distance_original_min_bits));
    log_key_float(
        "Maximum original model draw distance",
        read_float((BYTE *)&
            g_report.model_distance_original_max_bits));
    log_key_float(
        "Minimum effective model draw distance",
        read_float((BYTE *)&
            g_report.model_distance_scaled_min_bits));
    log_key_float(
        "Maximum effective model draw distance",
        read_float((BYTE *)&
            g_report.model_distance_scaled_max_bits));
    log_key_u32(
        "Invalid definition index",
        g_report.model_distance_invalid_definition_index);
    log_key_u32(
        "Invalid model index",
        g_report.model_distance_invalid_model_index);
    log_key_hex(
        "Invalid model pointer",
        g_report.model_distance_invalid_model_pointer);
    log_key_hex(
        "Invalid draw-distance bits",
        g_report.model_distance_invalid_draw_distance_bits);
    log_key_u32(
        "Write-mismatch unique-model index",
        g_report.model_distance_write_mismatch_index);
    log_key_u32(
        "Rollback/read-back verified",
        g_report.model_distance_rollback_verified);

    log_append("\r\nPer-model verification:\r\n");
    for (index = 0u;
         index < g_report.model_distance_unique_models &&
         index < MODEL_DISTANCE_RECORD_CAPACITY;
         ++index) {
        log_append("MODEL[");
        log_append_u32(g_model_distance_model_indices[index]);
        log_append("] pointer=");
        log_append_hex32(
            (DWORD)g_model_distance_model_pointers[index]);
        log_append(" draw-distance ");
        log_append_float_3(
            g_model_distance_original[index]);
        log_append(" -> ");
        log_append_float_3(
            g_model_distance_scaled[index]);
        log_append("; live=");
        if (g_model_distance_model_pointers[index] &&
            readable_address_range(
                g_model_distance_model_pointers[index] +
                    MODEL_INFO_DRAW_DISTANCE_OFFSET,
                4u)) {
            log_append_float_3(read_float(
                g_model_distance_model_pointers[index] +
                    MODEL_INFO_DRAW_DISTANCE_OFFSET));
        } else {
            log_append("(unreadable)");
        }
        log_append("\r\n");
    }
    log_append(
        "This block reports the deliberately retired model-global layer only. "
        "The active distance implementation is the exact-hash manager/source/"
        "PLANT transaction reported in the universal behavior fields; "
        "ordinary placed objects sharing a model are not mutated.\r\n");
    append_current_log_buffer();
}

static void write_definition_scaling_result(void)
{
    DWORD index;
    telemetry_begin_block("PROCEDURAL.DAT SCALING");
    log_key_u32(
        "Status code",
        g_report.definition_scaling_status);
    log_key_text(
        "Status",
        definition_status_text(
            g_report.definition_scaling_status));
    log_key_u32(
        "Game-thread callback state",
        g_report.definition_callback_state);
    log_key_u32(
        "Game-thread callback calls",
        g_report.definition_callback_calls);
    log_key_u32(
        "Game-thread callback thread ID",
        g_report.definition_callback_thread_id);
    log_key_hex(
        "Definition manager RVA",
        g_report.definition_manager_rva);
    log_key_u32(
        "Plant density multiplier",
        g_report.plant_density_multiplier);
    log_key_float(
        "Procedural-object density multiplier",
        read_float((BYTE *)&
            g_report.procobj_density_multiplier_bits));
    log_key_hex(
        "Procedural-object density multiplier bits",
        g_report.procobj_density_multiplier_bits);
    log_key_u32(
        "Density class mask",
        g_report.density_class_mask);
    log_append(
        "DensityClassMask bits: grass=1, vegetation=2, clutter=4, "
        "other=8. Default=3 (grass+vegetation).\r\n");
    log_key_text(
        "Selected stock PROCOBJ catalog",
        density_catalog_text(
            g_report.procobj_catalog_selection));
    log_key_hex(
        "Observed PROCOBJ catalog fingerprint",
        g_report.procobj_catalog_actual_fingerprint);
    log_key_hex(
        "Expected PROCOBJ catalog fingerprint",
        g_report.procobj_catalog_expected_fingerprint);
    log_key_u32(
        "PROCOBJ catalog mutation refused",
        g_report.definition_fingerprint_refused);
    log_key_u32(
        "Plant definitions loaded",
        g_report.plant_definition_count);
    log_key_u32(
        "Plant definitions scaled",
        g_report.plant_definitions_scaled);
    log_key_hex(
        "Observed PLANT catalog fingerprint",
        g_report.plant_catalog_actual_fingerprint);
    log_key_hex(
        "Expected PLANT catalog fingerprint",
        g_report.plant_catalog_expected_fingerprint);
    log_key_u32(
        "Stock PLANT catalog fingerprint matched",
        g_report.plant_catalog_match);
    log_key_text(
        "PLANT definition-field mutation",
        "DISABLED - authored +0x2C eligibility fields are observed and preserved");
    log_key_text(
        "PLANT visible-count density",
        "SELECTIVE EXACT-KEY MULTIPLIER IN THE PROVEN LIVE COUNT CONSUMER; RUNTIME VISUAL ACCEPTANCE PENDING");
    log_key_float(
        "Original total plant density",
        read_float(
            (BYTE *)&g_report.plant_original_density_sum_bits));
    log_key_float(
        "Scaled total plant density",
        read_float(
            (BYTE *)&g_report.plant_scaled_density_sum_bits));
    log_key_u32(
        "Procedural-object definitions loaded",
        g_report.procobj_definition_count);
    log_key_u32(
        "Procedural-object definitions scaled",
        g_report.procobj_definitions_scaled);
    log_key_u32(
        "Procedural-object definitions selected by class",
        g_report.procobj_definitions_selected);
    log_key_u32(
        "Procedural-object definitions not selected",
        g_report.procobj_definitions_unselected);
    log_key_u32(
        "Selected random-sampler definitions scaled",
        g_report.procobj_random_definitions_scaled);
    log_key_u32(
        "Selected USEGRID definitions scaled",
        g_report.procobj_grid_definitions_scaled);
    log_key_float(
        "Original minimum procedural spacing",
        read_float(
            (BYTE *)&g_report.procobj_original_spacing_min_bits));
    log_key_float(
        "Scaled minimum procedural spacing",
        read_float(
            (BYTE *)&g_report.procobj_scaled_spacing_min_bits));
    log_key_float(
        "Original total inverse-spacing-squared",
        read_float(
            (BYTE *)&g_report.procobj_original_inverse_sum_bits));
    log_key_float(
        "Scaled total inverse-spacing-squared",
        read_float(
            (BYTE *)&g_report.procobj_scaled_inverse_sum_bits));
    log_key_u32(
        "PROCOBJ spacing-floor clamps",
        g_report.procobj_spacing_floor_clamps);
    log_key_u32(
        "PROCOBJ numeric-limit clamps",
        g_report.procobj_value_limit_clamps);
    log_key_u32(
        "PLANT numeric-limit clamps",
        g_report.plant_value_limit_clamps);
    log_key_float(
        "Minimum effective PROCOBJ density multiplier",
        read_float((BYTE *)&
            g_report.procobj_effective_density_min_bits));
    log_key_float(
        "Maximum effective PROCOBJ density multiplier",
        read_float((BYTE *)&
            g_report.procobj_effective_density_max_bits));
    log_key_u32(
        "Live definition fields verified",
        g_report.definition_live_fields_verified);
    log_key_u32(
        "Live definition mismatches",
        g_report.definition_live_mismatches);
    log_key_u32(
        "First live mismatch kind",
        g_report.definition_first_live_mismatch_kind);
    log_key_u32(
        "First live mismatch index",
        g_report.definition_first_live_mismatch_index);
    log_key_u32(
        "PLANT density fields observed live",
        g_report.plant_live_fields_observed);
    log_key_u32(
        "PLANT observation mismatches",
        g_report.plant_live_observation_mismatches);

    log_append("\r\nPer-definition verification:\r\n");
    for (index = 0;
         g_report.procobj_catalog_selection != DENSITY_CATALOG_NONE &&
         index < g_report.procobj_definition_count &&
         index < DEFINITION_PROCOBJ_CAPACITY;
         ++index) {
        log_append("PROCOBJ[");
        log_append_u32(index);
        log_append("] spacing ");
        log_append_float_3(
            g_definition_original_proc_spacing[index]);
        log_append(" -> ");
        log_append_float_3(
            g_definition_scaled_proc_spacing[index]);
        log_append("; inverse-spacing-squared ");
        log_append_float_3(
            g_definition_original_proc_inverse[index]);
        log_append(" -> ");
        log_append_float_3(
            g_definition_scaled_proc_inverse[index]);
        log_append("; effective areal multiplier ");
        log_append_float_3(
            g_definition_effective_proc_density[index]);
        log_append("; class ");
        log_append(density_class_text(
            g_definition_proc_class[index]));
        log_append("; selected ");
        log_append_u32(g_definition_proc_selected[index]);
        log_append("; sampler ");
        log_append(g_definition_proc_usegrid[index] ?
            "USEGRID-spacing" : "random-inverse");
        if (g_definition_manager) {
            BYTE *record =
                g_definition_manager +
                DEFINITION_PROCOBJ_RECORDS_OFFSET +
                index * DEFINITION_PROCOBJ_RECORD_SIZE;
            log_append("; material-map-index ");
            log_append_u32(read_u32(record));
            log_append("; model-index ");
            log_append_u32(read_u32(record + 4u));
            log_append("; field+0x10(MINDIST near-exclusion squared) ");
            log_append_float_3(read_float(
                record +
                DEFINITION_PROCOBJ_DISTANCE_GATE_SQUARED_OFFSET));
            log_append("; field+0x14 ");
            log_append_float_3(read_float(
                record + DEFINITION_PROCOBJ_FIELD14_OFFSET));
            log_append("; field+0x18 ");
            log_append_float_3(read_float(
                record + DEFINITION_PROCOBJ_FIELD18_OFFSET));
        }
        log_append("\r\n");
    }
    for (index = 0;
         g_report.procobj_catalog_selection != DENSITY_CATALOG_NONE &&
         index < g_report.plant_definition_count &&
         index < DEFINITION_PLANT_CAPACITY;
         ++index) {
        log_append("PLANT[");
        log_append_u32(index);
        log_append("] density observed/preserved ");
        log_append_float_3(
            g_definition_original_plant_density[index]);
        log_append(" == ");
        log_append_float_3(
            g_definition_scaled_plant_density[index]);
        log_append("\r\n");
    }
    log_append("All written definition fields re-read and matched: ");
    log_append_u32(
        g_report.definition_scaling_status ==
            DEFINITION_SCALING_APPLIED ||
        g_report.definition_scaling_status ==
            DEFINITION_SCALING_NO_CHANGE);
    log_append("\r\n");
    append_current_log_buffer();
}

static void capture_early_telemetry(
    DWORD *sample_number,
    DWORD *high_surface,
    DWORD *high_source)
{
    struct TelemetrySnapshot snapshot;

    if (!capture_telemetry_snapshot(&snapshot)) {
        return;
    }
    if (snapshot.surface_active > *high_surface) {
        *high_surface = snapshot.surface_active;
    }
    if (snapshot.source_active > *high_source) {
        *high_source = snapshot.source_active;
    }
    ++*sample_number;
    publish_telemetry_snapshot(
        &snapshot, *sample_number,
        *high_surface, *high_source);
    write_telemetry_scheduled(
        &snapshot, *sample_number,
        *high_surface, *high_source);
}

static int wait_for_definition_engine_callback(
    DWORD *sample_number,
    DWORD *high_surface,
    DWORD *high_source)
{
    DWORD elapsed = 0;
    DWORD loaded_without_callback_elapsed = 0u;

    while (InterlockedCompareExchange(
               &g_telemetry_stop, 0, 0) == 0) {
        DWORD proc_count;
        DWORD plant_count;
        LONG callback_state = InterlockedCompareExchange(
            &g_definition_callback_state, 0, 0);

        if (callback_state == DEFINITION_CALLBACK_COMPLETE) {
            return 1;
        }
        if (callback_state == DEFINITION_CALLBACK_FAILED) {
            return 0;
        }

        if (!g_definition_manager ||
            !readable_address_range(
                g_definition_manager,
                DEFINITION_MANAGER_REQUIRED_SIZE)) {
            g_report.definition_scaling_status =
                DEFINITION_SCALING_BAD_MANAGER;
            return 0;
        }

        proc_count = read_u32(
            g_definition_manager +
            DEFINITION_PROCOBJ_COUNT_OFFSET);
        plant_count = read_u32(
            g_definition_manager +
            DEFINITION_PLANT_COUNT_OFFSET);
        if (proc_count > DEFINITION_PROCOBJ_CAPACITY ||
            plant_count > DEFINITION_PLANT_CAPACITY) {
            g_report.procobj_definition_count = proc_count;
            g_report.plant_definition_count = plant_count;
            g_report.definition_scaling_status =
                DEFINITION_SCALING_BAD_COUNTS;
            return 0;
        }
        if ((callback_state == DEFINITION_CALLBACK_UNARMED ||
             callback_state == DEFINITION_CALLBACK_ARMED) &&
            (proc_count != 0u || plant_count != 0u)) {
            loaded_without_callback_elapsed += 10u;
            if (loaded_without_callback_elapsed >= 30000u &&
                InterlockedCompareExchange(
                    &g_definition_callback_state,
                    DEFINITION_CALLBACK_FAILED,
                    callback_state) == callback_state) {
                g_report.procobj_definition_count = proc_count;
                g_report.plant_definition_count = plant_count;
                g_report.definition_scaling_status =
                    DEFINITION_SCALING_CALLBACK_MISSED;
                g_report.definition_callback_state =
                    DEFINITION_CALLBACK_FAILED;
                g_report.status =
                    STATUS_DEFINITION_CALLBACK_MISSED;
                return 0;
            }
        } else {
            loaded_without_callback_elapsed = 0u;
        }

        gu_event_drain(
            &g_gu_event_ring, &g_gu_event_summary,
            GU_EVENT_RING_CAPACITY);
        Sleep(10);
        elapsed += 10u;

        if (elapsed % 1000u == 0u) {
            capture_early_telemetry(
                sample_number, high_surface, high_source);
        }
        if (elapsed % DEFINITION_WAIT_LOG_INTERVAL_MS == 0u) {
            telemetry_begin_block("PROCEDURAL.DAT WAIT");
            log_append(
                "Waiting for the transaction-installed game-thread "
                "definition-load callback. GTA IV continues loading; "
                "the telemetry thread does not mutate definitions.\r\n");
            log_key_u32("Milliseconds waiting", elapsed);
            log_key_u32(
                "Definition callback state",
                (DWORD)callback_state);
            log_key_u32(
                "Definition callback calls",
                (DWORD)InterlockedCompareExchange(
                    &g_definition_callback_calls, 0, 0));
            log_key_u32(
                "Procedural-object definitions currently visible",
                proc_count);
            log_key_u32(
                "Plant definitions currently visible",
                plant_count);
            append_current_log_buffer();
        }
    }

    return 0;
}

static int wait_for_background_threads_ready(
    volatile LONG *stop_flag)
{
    while (InterlockedCompareExchange(
               &g_background_threads_ready, 0, 0) == 0) {
        if (stop_flag &&
            InterlockedCompareExchange(stop_flag, 0, 0) != 0) {
            return 0;
        }
        Sleep(1);
    }
    return 1;
}

static DWORD WINAPI telemetry_thread_main(LPVOID parameter)
{
    DWORD sample_number = 0;
    DWORD high_surface = 0;
    DWORD high_source = 0;
    DWORD wait_count = 0;
    struct TelemetrySnapshot snapshot;

    (void)parameter;
    /*
     * The startup worker creates this thread before it writes the initial log
     * so that the log can truthfully report thread-creation success. An exact
     * interlocked gate keeps this thread away from the shared log buffer until
     * that initial write is complete. This thread observes definitions but
     * never writes them.
     */
    if (!wait_for_background_threads_ready(&g_telemetry_stop)) {
        return 0u;
    }
#ifndef GTAIVPOOL_TEST_NO_FATAL
    if (wait_for_definition_engine_callback(
            &sample_number, &high_surface, &high_source)) {
        write_definition_scaling_result();
        write_model_distance_scaling_result();
    } else if (InterlockedCompareExchange(
                   &g_telemetry_stop, 0, 0) == 0) {
        write_definition_scaling_result();
        write_model_distance_scaling_result();
        if (g_report.status ==
                STATUS_DEFINITION_CALLBACK_MISSED) {
            telemetry_begin_block(
                "FATAL DEFINITION CALLBACK MISS");
            log_append(
                "Procedural definitions became visible, but the exact "
                "transaction-installed one-shot callback did not execute "
                "within 30 seconds. Continuing would falsely claim active "
                "density scaling; the process is closing fail-closed.\r\n");
            append_current_log_buffer();
            /* Optional observation failure disables telemetry; never close GTA IV. */
            InterlockedExchange(&g_telemetry_stop, 1);
            return g_report.status;
        }
    }
    /* Runtime definition loading itself has no fixed deadline.  Only the
     * stronger impossible-success condition above is fatal: nonzero catalog
     * counts remained visible for 30 seconds without the installed one-shot
     * callback ever entering. */
#endif
    telemetry_begin_block("TELEMETRY");
    log_append("Thread active. Waiting for the procedural manager buffers.\r\n");
    append_current_log_buffer();

    while (InterlockedCompareExchange(&g_telemetry_stop, 0, 0) == 0) {
        publish_tuning_governor_event_if_pending();
        if (capture_telemetry_snapshot(&snapshot)) {
            verify_definition_scaling_live();
            verify_procedural_model_draw_distances_live();
            if (g_report.definition_live_mismatches != 0u) {
                trigger_tuning_safety_governor(
                    TUNING_GOVERNOR_REASON_DENSITY_MISMATCH,
                    snapshot.generator_active_wrapper_records,
                    snapshot.generator_staging_count,
                    snapshot.generator_active_rendered);
            } else if (g_report.model_distance_live_mismatches != 0u) {
                trigger_tuning_safety_governor(
                    TUNING_GOVERNOR_REASON_DISTANCE_MISMATCH,
                    snapshot.generator_active_wrapper_records,
                    snapshot.generator_staging_count,
                    snapshot.generator_active_rendered);
            }
            if (snapshot.surface_active > high_surface) {
                high_surface = snapshot.surface_active;
            }
            if (snapshot.source_active > high_source) {
                high_source = snapshot.source_active;
            }
            ++sample_number;
            publish_telemetry_snapshot(
                &snapshot, sample_number,
                high_surface, high_source);
            write_telemetry_scheduled(
                &snapshot, sample_number, high_surface, high_source);
            telemetry_polling_wait(TELEMETRY_INTERVAL_MS);
        } else {
            ++wait_count;
            if (wait_count == 1u || wait_count % 10u == 0u) {
                telemetry_begin_block("TELEMETRY WAIT");
                log_append("Manager not initialized/readable yet. Attempt ");
                log_append_u32(wait_count);
                log_append(".\r\n");
                append_current_log_buffer();
            }
            gu_event_drain(
                &g_gu_event_ring, &g_gu_event_summary,
                GU_EVENT_RING_CAPACITY);
            Sleep(1000);
        }
    }
    return 0;
}

static void start_telemetry(void)
{
    HANDLE thread;

    if (g_report.status != STATUS_PATCH_APPLIED ||
        !g_manager) {
        return;
    }

    InterlockedExchange(&g_telemetry_stop, 0);
    InterlockedExchange(&g_provider_exhaustion_observed, 0);
    InterlockedExchange(&g_surface_exhaustion_observed, 0);
    InterlockedExchange(
        &g_generator_active_high_watermark, 0);
    InterlockedExchange(
        &g_generator_wrapper_high_watermark, 0);
    InterlockedExchange(
        &g_generator_staging_high_watermark, 0);
    InterlockedExchange(&g_last_snapshot_state, 0);
    ZeroMemory(
        &g_last_telemetry_snapshot,
        sizeof(g_last_telemetry_snapshot));
    g_last_sample_number = 0u;
    g_last_surface_high_watermark = 0u;
    g_last_provider_high_watermark = 0u;
    thread = CreateThread(
        NULL, 0, telemetry_thread_main, NULL, 0, NULL);
    if (thread) {
        g_report.telemetry_thread_started = 1;
        CloseHandle(thread);
    } else {
        g_report.last_error = GetLastError();
    }
}

static const char *build_startup_error_message(void)
{
    char *message = g_startup_error_message;
    message[0] = '\0';

    if (g_report.status == STATUS_PATCH_APPLIED) {
        return message;
    }

    text_buffer_append(
        message, sizeof(g_startup_error_message),
        "The Grass Fix was disabled because it could not verify a safe "
        "startup. GTA IV will continue unchanged.\r\n\r\nReason: ");

    switch (g_report.status) {
    case STATUS_INVALID_CONFIG:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            g_config_error_key[0] ? g_config_error_key : "INI setting");
        text_buffer_append(message, sizeof(g_startup_error_message), "=");
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            g_config_error_value[0] ? g_config_error_value : "(missing)");
        if (g_config_error_kind == CONFIG_ERROR_MISSING) {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                " is missing from [ProceduralPool].");
        } else if (
            g_config_error_kind == CONFIG_ERROR_NOT_WHOLE_NUMBER) {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                " is not a whole number using digits only.");
        } else if (
            g_config_error_kind == CONFIG_ERROR_NOT_DECIMAL_FLOAT) {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                " is not a plain decimal number.");
        } else {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                " is outside the allowed range.");
        }
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            " Allowed: ");
        if (g_config_error_float_range) {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                g_config_error_min_text);
        } else {
            text_buffer_append_u32(
                message, sizeof(g_startup_error_message),
                g_config_error_min);
        }
        text_buffer_append(message, sizeof(g_startup_error_message), " to ");
        if (g_config_error_float_range) {
            text_buffer_append(
                message, sizeof(g_startup_error_message),
                g_config_error_max_text);
        } else {
            text_buffer_append_u32(
                message, sizeof(g_startup_error_message),
                g_config_error_max);
        }
        text_buffer_append(message, sizeof(g_startup_error_message), ".");
        break;
    case STATUS_BEHAVIOR_AXIS_ISOLATION_REFUSED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "A historical one-axis isolation refusal was reported. "
            "GRASS-BLD-0050 does not use this status; startup remains "
            "fail-closed and no additional game patch is written.");
        break;
    case STATUS_ASI_IDENTITY_READ_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The loaded ASI file could not be opened with a retained "
            "read-only identity lock and hashed. No game patch was written.");
        break;
    case STATUS_IDENTITY_REVALIDATION_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The retained ASI, INI, or GTAIV.exe file identity, size, timestamp, "
            "path binding, or SHA-256 changed before the first loaded-image "
            "write. Startup failed closed and no game patch was written.");
        break;
    case STATUS_STARTUP_WORKER_PIN_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The startup worker could not pin this ASI module before leaving "
            "initialization. No hashing or game patch was attempted.");
        break;
    case STATUS_INI_IDENTITY_READ_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The required INI could not be opened with a retained read-only "
            "identity lock and hashed before configuration parsing. No game "
            "patch was written.");
        break;
    case STATUS_DEFINITION_LOAD_ALREADY_STARTED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "GTA IV began loading procedural.dat before the exact "
            "game-thread definition callback was installed. Startup failed "
            "closed and all earlier writes were rolled back; restart the "
            "game before testing this build again.");
        break;
    case STATUS_BAD_MAIN_MODULE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "Windows could not locate GTAIV.exe as the main module.");
        break;
    case STATUS_BAD_PE_IMAGE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The main executable is not a valid 32-bit GTA IV PE image.");
        break;
    case STATUS_UNSUPPORTED_MACHINE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The main executable is not 32-bit x86.");
        break;
    case STATUS_SIGNATURE_NOT_FOUND:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The procedural-capacity code signature was not found.");
        break;
    case STATUS_SIGNATURE_AMBIGUOUS:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The procedural-capacity signature appeared more than once.");
        break;
    case STATUS_BAD_MANAGER_REFERENCE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The capacity code points outside the GTAIV.exe image.");
        break;
    case STATUS_TOO_LATE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The procedural manager was already initialized before this "
            "ASI loaded.");
        break;
    case STATUS_UNEXPECTED_CAPACITY:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The capacity instruction contains ");
        text_buffer_append_u32(
            message, sizeof(g_startup_error_message),
            g_report.original_immediate);
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "; expected 512. Another mod may have changed it.");
        break;
    case STATUS_DISTANCE_INIT_SIGNATURE_NOT_FOUND:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The distance telemetry initialization signature was not found.");
        break;
    case STATUS_DISTANCE_INIT_SIGNATURE_AMBIGUOUS:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The distance telemetry initialization signature appeared more than "
            "once.");
        break;
    case STATUS_DISTANCE_UPDATE_SIGNATURE_NOT_FOUND:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The distance telemetry update signature was not found.");
        break;
    case STATUS_DISTANCE_UPDATE_SIGNATURE_AMBIGUOUS:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The distance telemetry update signature appeared more than once.");
        break;
    case STATUS_DISTANCE_SOURCE_VALIDATION_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The distance code uses unexpected source values or pointers.");
        break;
    case STATUS_PROVIDER_UNSUPPORTED_BUILD:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "ProviderCapacity above 40 is not verified for the selected "
            "executable profile. Set ProviderCapacity=40; all other "
            "features of this profile remain supported.");
        break;
    case STATUS_PROVIDER_CODE_MISMATCH:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "Provider patch site ");
        text_buffer_append_u32(
            message, sizeof(g_startup_error_message),
            g_report.provider_first_mismatch_site);
        text_buffer_append(message, sizeof(g_startup_error_message),
                           " differs at byte ");
        text_buffer_append_u32(
            message, sizeof(g_startup_error_message),
            g_report.provider_first_mismatch_byte);
        text_buffer_append(message, sizeof(g_startup_error_message), ".");
        break;
    case STATUS_DEFINITION_UNSUPPORTED_BUILD:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The definition-manager startup code at RVA ");
        text_buffer_append_hex32(
            message, sizeof(g_startup_error_message),
            VERIFIED_DEFINITION_CALL_RVA);
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            " does not match the selected executable profile: ");
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            g_build_profile->name);
        text_buffer_append(message, sizeof(g_startup_error_message), ".");
        break;
    case STATUS_GENERATOR_UNSUPPORTED_BUILD:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The procedural generator manager is at an unexpected RVA.");
        break;
    case STATUS_GENERATOR_CODE_MISMATCH:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "Generator patch site ");
        text_buffer_append_u32(
            message, sizeof(g_startup_error_message),
            g_report.generator_first_mismatch_site);
        text_buffer_append(message, sizeof(g_startup_error_message),
                           " differs at byte ");
        text_buffer_append_u32(
            message, sizeof(g_startup_error_message),
            g_report.generator_first_mismatch_byte);
        text_buffer_append(message, sizeof(g_startup_error_message), ".");
        break;
    case STATUS_REQUIRED_LAYOUT_OUT_OF_RANGE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "A required address for the selected executable profile is "
            "outside this EXE image (size ");
        text_buffer_append_hex32(
            message, sizeof(g_startup_error_message),
            g_report.size_of_image);
        text_buffer_append(message, sizeof(g_startup_error_message), ").");
        break;
    case STATUS_UNSUPPORTED_BUILD_PROFILE:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "No permanently registered executable profile matched this "
            "GTAIV.exe identity. This build uses a generated catalog of "
            "independently verified exact hashes spanning 1.0.4.0, Patch 7 1.0.7.0, both "
            "1.0.8.0 code families, legacy Complete Edition layouts, and "
            "Complete Edition 1.2.0.59.");
        break;
    case STATUS_EXECUTABLE_HASH_READ_FAILED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The ASI could not read GTAIV.exe to calculate its required "
            "SHA-256 identity. No patch method was selected.");
        break;
    case STATUS_UNREGISTERED_EXECUTABLE_HASH:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "This exact GTAIV.exe SHA-256 is not in the permanent "
            "executable registry. The ASI will not guess from a similar "
            "timestamp or file layout. Send the log and this GTAIV.exe so "
            "its own method can be verified and appended.");
        break;
    case STATUS_COMPATIBILITY_PROBE_DECLINED:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "This GTAIV.exe is unrecognized and the optional compatibility "
            "probe was declined. The mod is disabled and the game may "
            "continue unchanged.");
        break;
    case STATUS_COMPATIBILITY_PROBE_NO_UNIQUE_FAMILY:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The optional compatibility probe did not find exactly one "
            "complete verified profile family. No patch was retained; the "
            "mod is disabled and the game may continue unchanged.");
        break;
    case STATUS_EXECUTABLE_IDENTITY_MISMATCH:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The exact executable hash was registered, but its PE metadata "
            "did not match the immutable patch-method contract. Startup was "
            "blocked because the evidence registry is inconsistent.");
        break;
    case STATUS_GENERATOR_INITIALIZED_BEFORE_PATCH:
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            "The generator staging queue was already initialized before the "
            "ASI could replace its constructor capacity. Continuing would "
            "leave the old 512-entry queue active, so startup was blocked.");
        break;
    default:
        text_buffer_append(
            message, sizeof(g_startup_error_message), status_text(g_report.status));
        text_buffer_append(
            message, sizeof(g_startup_error_message),
            ". Patch application or rollback verification failed.");
        break;
    }

    text_buffer_append(
        message, sizeof(g_startup_error_message),
        "\r\n\r\nStatus ");
    text_buffer_append_u32(
        message, sizeof(g_startup_error_message), g_report.status);
    text_buffer_append(message, sizeof(g_startup_error_message), ": ");
    text_buffer_append(
        message, sizeof(g_startup_error_message), status_text(g_report.status));
    text_buffer_append(
        message, sizeof(g_startup_error_message),
        "\r\nDetails: GTAIV.EFLC.ProceduralFixes.log\r\n"
        "The mod is disabled and GTA IV will continue unchanged.");
    return message;
}

#ifndef GTAIVPOOL_TEST_NO_FATAL
static int prompt_unrecognized_executable_probe(void)
{
    int response;
    char *message = g_compatibility_probe_message;

    ZeroMemory(
        g_compatibility_probe_message,
        sizeof(g_compatibility_probe_message));
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        "This GTAIV.exe is unrecognized.\r\n\r\nSHA-256: ");
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        g_main_sha256[0] ? g_main_sha256 : "(unavailable)");
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        "\r\nSize: ");
    text_buffer_append_u32(
        message, sizeof(g_compatibility_probe_message),
        g_report.executable_file_size);
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        " bytes\r\n\r\nWould you like to try loading the Grass Fix "
        "using a compatibility probe?\r\n\r\n"
        "YES: compare the complete guarded code against every verified "
        "profile family. A patch is attempted only when exactly one family "
        "matches and every transactional preimage check passes.\r\n\r\n"
        "NO: leave the mod disabled and continue into the game.\r\n\r\n"
        "Either choice keeps this executable unrecognized and unsupported.");

    InterlockedExchange(&g_compatibility_probe_prompted, 1);
    g_report.compatibility_probe_prompted = 1u;
    response = MessageBoxA(
        NULL, message,
        "GTA IV Grass Fix v1.2 - Unrecognized EXE",
        MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND |
            MB_TOPMOST | MB_DEFBUTTON2);
    if (response != IDYES) {
        return 0;
    }
    InterlockedExchange(&g_compatibility_probe_authorized, 1);
    g_report.compatibility_probe_authorized = 1u;
    return 1;
}

static void show_compatibility_probe_result(void)
{
    char *message = g_compatibility_probe_message;
    if (InterlockedCompareExchange(
            &g_compatibility_probe_prompted, 0, 0) == 0 ||
        InterlockedCompareExchange(
            &g_compatibility_probe_authorized, 0, 0) == 0) {
        return;
    }

    ZeroMemory(
        g_compatibility_probe_message,
        sizeof(g_compatibility_probe_message));
    if (g_report.status == STATUS_PATCH_APPLIED) {
        text_buffer_append(
            message, sizeof(g_compatibility_probe_message),
            "Compatibility patch succeeded. GTA IV will continue.\r\n\r\n"
            "This exact executable is still unrecognized and is not an "
            "officially supported target. Preserve and return "
            "GTAIV.EFLC.ProceduralFixes.log.");
        MessageBoxA(
            NULL, message,
            "GTA IV Grass Fix v1.2 - Patch succeeded",
            MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
        return;
    }

    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        "Compatibility patch failed. The Grass Fix is disabled and GTA IV "
        "will continue. No compatibility patch remains installed.\r\n\r\n"
        "Status ");
    text_buffer_append_u32(
        message, sizeof(g_compatibility_probe_message),
        g_report.status);
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message), ": ");
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        status_text(g_report.status));
    text_buffer_append(
        message, sizeof(g_compatibility_probe_message),
        "\r\nDetails: GTAIV.EFLC.ProceduralFixes.log");
    MessageBoxA(
        NULL, message,
        "GTA IV Grass Fix v1.2 - Patch failed",
        MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST);
}

static void show_disabled_result(void)
{
    if (g_report.status == STATUS_PATCH_APPLIED ||
        InterlockedCompareExchange(
            &g_compatibility_probe_prompted, 0, 0) != 0) {
        return;
    }
    MessageBoxA(
        NULL, build_startup_error_message(),
        "GTA IV Grass Fix v1.2 - Mod disabled",
        MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
}
#endif

static void capture_module_paths(void)
{
    DWORD length;
    DWORD process_id;
    SYSTEMTIME utc;
    char capture_stem[160];
    char capture_name[192];

    g_self_path[0] = '\0';
    g_main_path[0] = '\0';
    GetModuleFileNameA(g_self_module, g_self_path, MAX_PATH);
    GetModuleFileNameA(NULL, g_main_path, MAX_PATH);

    make_sibling_path(g_self_path, PLUGIN_NAME ".ini", g_ini_path, MAX_PATH);
    make_sibling_path(g_self_path, PLUGIN_NAME ".log", g_log_path, MAX_PATH);

    GetSystemTime(&utc);
    process_id = GetCurrentProcessId();
    wsprintfA(
        capture_stem,
        PLUGIN_NAME "_%04u%02u%02uT%02u%02u%02uZ_PID%lu",
        (DWORD)utc.wYear, (DWORD)utc.wMonth, (DWORD)utc.wDay,
        (DWORD)utc.wHour, (DWORD)utc.wMinute, (DWORD)utc.wSecond,
        process_id);
    wsprintfA(capture_name, "%s_crash.dmp", capture_stem);
    make_sibling_path(
        g_self_path, capture_name,
        g_crash_dump_path, MAX_PATH);
    wsprintfA(capture_name, "%s_crash_fallback.dmp", capture_stem);
    make_sibling_path(
        g_self_path, capture_name,
        g_crash_fallback_dump_path, MAX_PATH);
    wsprintfA(capture_name, "%s_hang.dmp", capture_stem);
    make_sibling_path(
        g_self_path, capture_name,
        g_hang_dump_path, MAX_PATH);
    wsprintfA(capture_name, "%s_hang_fallback.dmp", capture_stem);
    make_sibling_path(
        g_self_path, capture_name,
        g_hang_fallback_dump_path, MAX_PATH);
    wsprintfA(capture_name, "%s_hang_summary.txt", capture_stem);
    make_sibling_path(
        g_self_path, capture_name,
        g_hang_summary_path, MAX_PATH);
    g_existing_crash_dump_at_start =
        GetFileAttributesA(g_crash_dump_path) !=
        INVALID_FILE_ATTRIBUTES;
    g_existing_hang_dump_at_start =
        GetFileAttributesA(g_hang_dump_path) !=
        INVALID_FILE_ATTRIBUTES;

    length = string_length(g_self_path);
    if (length == 0 || length >= MAX_PATH - 1) {
        copy_string(g_self_path, MAX_PATH, "(module path unavailable)");
    }
    length = string_length(g_main_path);
    if (length == 0 || length >= MAX_PATH - 1) {
        copy_string(g_main_path, MAX_PATH, "(main path unavailable)");
    }
}

struct Sha256Context {
    DWORD state[8];
    ULONGLONG total_bytes;
    BYTE block[64];
    DWORD block_length;
};

static DWORD sha256_rotate_right(DWORD value, DWORD count)
{
    return (value >> count) | (value << (32u - count));
}

static void sha256_transform(
    struct Sha256Context *context, const BYTE *block)
{
    static const DWORD constants[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u,
        0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
        0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u,
        0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
        0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
        0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
        0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
        0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u,
        0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
        0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u,
        0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u,
        0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
        0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
    };
    DWORD words[64];
    DWORD a;
    DWORD b;
    DWORD c;
    DWORD d;
    DWORD e;
    DWORD f;
    DWORD g;
    DWORD h;
    DWORD index;

    for (index = 0u; index < 16u; ++index) {
        DWORD offset = index * 4u;
        words[index] =
            ((DWORD)block[offset] << 24) |
            ((DWORD)block[offset + 1u] << 16) |
            ((DWORD)block[offset + 2u] << 8) |
            (DWORD)block[offset + 3u];
    }
    for (index = 16u; index < 64u; ++index) {
        DWORD x = words[index - 15u];
        DWORD y = words[index - 2u];
        DWORD s0 = sha256_rotate_right(x, 7u) ^
            sha256_rotate_right(x, 18u) ^ (x >> 3u);
        DWORD s1 = sha256_rotate_right(y, 17u) ^
            sha256_rotate_right(y, 19u) ^ (y >> 10u);
        words[index] = words[index - 16u] + s0 +
            words[index - 7u] + s1;
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];
    for (index = 0u; index < 64u; ++index) {
        DWORD sum1 = sha256_rotate_right(e, 6u) ^
            sha256_rotate_right(e, 11u) ^
            sha256_rotate_right(e, 25u);
        DWORD choice = (e & f) ^ ((~e) & g);
        DWORD temp1 = h + sum1 + choice +
            constants[index] + words[index];
        DWORD sum0 = sha256_rotate_right(a, 2u) ^
            sha256_rotate_right(a, 13u) ^
            sha256_rotate_right(a, 22u);
        DWORD majority = (a & b) ^ (a & c) ^ (b & c);
        DWORD temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

static void sha256_initialize(struct Sha256Context *context)
{
    context->state[0] = 0x6A09E667u;
    context->state[1] = 0xBB67AE85u;
    context->state[2] = 0x3C6EF372u;
    context->state[3] = 0xA54FF53Au;
    context->state[4] = 0x510E527Fu;
    context->state[5] = 0x9B05688Cu;
    context->state[6] = 0x1F83D9ABu;
    context->state[7] = 0x5BE0CD19u;
    context->total_bytes = 0u;
    context->block_length = 0u;
}

static void sha256_update(
    struct Sha256Context *context, const BYTE *data, DWORD length)
{
    DWORD index;
    context->total_bytes += (ULONGLONG)length;
    for (index = 0u; index < length; ++index) {
        context->block[context->block_length++] = data[index];
        if (context->block_length == 64u) {
            sha256_transform(context, context->block);
            context->block_length = 0u;
        }
    }
}

static void sha256_finish(
    struct Sha256Context *context, BYTE digest[32])
{
    BYTE padding[128];
    BYTE length_bytes[8];
    ULONGLONG bit_length = context->total_bytes * 8u;
    DWORD padding_length;
    DWORD index;

    ZeroMemory(padding, sizeof(padding));
    padding[0] = 0x80u;
    padding_length = context->block_length < 56u ?
        56u - context->block_length :
        120u - context->block_length;
    for (index = 0u; index < 8u; ++index) {
        length_bytes[7u - index] =
            (BYTE)(bit_length >> (index * 8u));
    }
    sha256_update(context, padding, padding_length);
    sha256_update(context, length_bytes, sizeof(length_bytes));
    for (index = 0u; index < 8u; ++index) {
        digest[index * 4u] = (BYTE)(context->state[index] >> 24);
        digest[index * 4u + 1u] =
            (BYTE)(context->state[index] >> 16);
        digest[index * 4u + 2u] =
            (BYTE)(context->state[index] >> 8);
        digest[index * 4u + 3u] = (BYTE)context->state[index];
    }
}

static int set_file_position_to_start(HANDLE file)
{
    DWORD result;

    SetLastError(NO_ERROR);
    result = SetFilePointer(file, 0, NULL, FILE_BEGIN);
    return result != INVALID_SET_FILE_POINTER ||
        GetLastError() == NO_ERROR;
}

static int hash_open_file_sha256_hex(
    HANDLE file, char output[65], DWORD *file_size)
{
    static const char hex[] = "0123456789ABCDEF";
    struct Sha256Context context;
    BYTE buffer[4096];
    BYTE digest[32];
    DWORD read_count;
    DWORD size_low;
    DWORD size_high = 0u;
    DWORD total_read = 0u;
    DWORD index;

    if (!file || file == INVALID_HANDLE_VALUE ||
        !output || !file_size) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    output[0] = '\0';
    *file_size = 0u;
    if (!set_file_position_to_start(file)) {
        return 0;
    }
    SetLastError(NO_ERROR);
    size_low = GetFileSize(file, &size_high);
    if (size_high != 0u ||
        (size_low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)) {
        return 0;
    }
    sha256_initialize(&context);
    for (;;) {
        if (!ReadFile(
                file, buffer, sizeof(buffer), &read_count, NULL)) {
            return 0;
        }
        if (read_count == 0u) {
            break;
        }
        if (total_read > 0xFFFFFFFFu - read_count) {
            SetLastError(ERROR_BAD_LENGTH);
            return 0;
        }
        total_read += read_count;
        sha256_update(&context, buffer, read_count);
    }
    if (total_read != size_low) {
        SetLastError(ERROR_INVALID_DATA);
        return 0;
    }
    sha256_finish(&context, digest);
    for (index = 0u; index < sizeof(digest); ++index) {
        output[index * 2u] = hex[digest[index] >> 4];
        output[index * 2u + 1u] = hex[digest[index] & 0x0Fu];
    }
    output[64] = '\0';
    *file_size = size_low;
    return 1;
}

static int file_identity_snapshots_equal(
    const BY_HANDLE_FILE_INFORMATION *left,
    const BY_HANDLE_FILE_INFORMATION *right)
{
    return left && right &&
        left->dwVolumeSerialNumber == right->dwVolumeSerialNumber &&
        left->nFileIndexHigh == right->nFileIndexHigh &&
        left->nFileIndexLow == right->nFileIndexLow &&
        left->nFileSizeHigh == right->nFileSizeHigh &&
        left->nFileSizeLow == right->nFileSizeLow &&
        left->ftLastWriteTime.dwHighDateTime ==
            right->ftLastWriteTime.dwHighDateTime &&
        left->ftLastWriteTime.dwLowDateTime ==
            right->ftLastWriteTime.dwLowDateTime;
}

static void close_locked_file_identity(
    struct LockedFileIdentity *identity)
{
    if (!identity) {
        return;
    }
    if (identity->handle &&
        identity->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(identity->handle);
    }
    ZeroMemory(identity, sizeof(*identity));
}

static int acquire_locked_file_identity(
    const char *path, struct LockedFileIdentity *identity)
{
    BY_HANDLE_FILE_INFORMATION post_hash_snapshot;

    if (!path || !identity) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    ZeroMemory(identity, sizeof(*identity));
    identity->handle = CreateFileA(
        path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (identity->handle == INVALID_HANDLE_VALUE) {
        identity->handle = NULL;
        return 0;
    }
    if (!GetFileInformationByHandle(
            identity->handle, &identity->snapshot) ||
        identity->snapshot.nFileSizeHigh != 0u ||
        !hash_open_file_sha256_hex(
            identity->handle, identity->sha256,
            &identity->file_size) ||
        !GetFileInformationByHandle(
            identity->handle, &post_hash_snapshot) ||
        !file_identity_snapshots_equal(
            &identity->snapshot, &post_hash_snapshot) ||
        identity->snapshot.nFileSizeLow != identity->file_size) {
        DWORD error = GetLastError();
        close_locked_file_identity(identity);
        SetLastError(error == NO_ERROR ? ERROR_INVALID_DATA : error);
        return 0;
    }
    return 1;
}

static int locked_file_identity_matches_path(
    const char *path, const struct LockedFileIdentity *identity)
{
    HANDLE path_file;
    BY_HANDLE_FILE_INFORMATION path_snapshot;
    int matches;
    DWORD error = NO_ERROR;

    if (!path || !identity || !identity->handle) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    path_file = CreateFileA(
        path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (path_file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!GetFileInformationByHandle(
            path_file, &path_snapshot)) {
        matches = 0;
        error = GetLastError();
    } else {
        matches = file_identity_snapshots_equal(
            &identity->snapshot, &path_snapshot);
        if (!matches) {
            error = ERROR_INVALID_DATA;
        }
    }
    CloseHandle(path_file);
    if (!matches) {
        SetLastError(error == NO_ERROR ? ERROR_INVALID_DATA : error);
    }
    return matches;
}

static int revalidate_locked_file_identity(
    const char *path, const struct LockedFileIdentity *identity)
{
    BY_HANDLE_FILE_INFORMATION before_hash_snapshot;
    BY_HANDLE_FILE_INFORMATION after_hash_snapshot;
    char current_sha256[65];
    DWORD current_size = 0u;

    if (!path || !identity || !identity->handle ||
        !GetFileInformationByHandle(
            identity->handle, &before_hash_snapshot) ||
        !file_identity_snapshots_equal(
            &identity->snapshot, &before_hash_snapshot) ||
        !hash_open_file_sha256_hex(
            identity->handle, current_sha256, &current_size) ||
        !GetFileInformationByHandle(
            identity->handle, &after_hash_snapshot) ||
        !file_identity_snapshots_equal(
            &identity->snapshot, &after_hash_snapshot) ||
        current_size != identity->file_size ||
        !strings_are_equal(current_sha256, identity->sha256) ||
        !locked_file_identity_matches_path(path, identity)) {
        if (GetLastError() == NO_ERROR) {
            SetLastError(ERROR_INVALID_DATA);
        }
        return 0;
    }
    return 1;
}

static DWORD read_disk_preferred_image_base(void)
{
    HANDLE file = g_main_file_identity.handle;
    IMAGE_DOS_HEADER dos_header;
    DWORD signature;
    IMAGE_FILE_HEADER file_header;
    IMAGE_OPTIONAL_HEADER32 optional_header;
    DWORD read_count;
    DWORD result = 0u;

    if (!file || file == INVALID_HANDLE_VALUE ||
        !set_file_position_to_start(file)) {
        return 0u;
    }
    if (!ReadFile(
            file, &dos_header, sizeof(dos_header),
            &read_count, NULL) ||
        read_count != sizeof(dos_header) ||
        dos_header.e_magic != IMAGE_DOS_SIGNATURE ||
        dos_header.e_lfanew <= 0 ||
        (DWORD)dos_header.e_lfanew > 0x100000u ||
        SetFilePointer(
            file, dos_header.e_lfanew, NULL,
            FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
        !ReadFile(
            file, &signature, sizeof(signature),
            &read_count, NULL) ||
        read_count != sizeof(signature) ||
        signature != IMAGE_NT_SIGNATURE ||
        !ReadFile(
            file, &file_header, sizeof(file_header),
            &read_count, NULL) ||
        read_count != sizeof(file_header) ||
        file_header.Machine != IMAGE_FILE_MACHINE_I386 ||
        file_header.SizeOfOptionalHeader < sizeof(optional_header) ||
        !ReadFile(
            file, &optional_header, sizeof(optional_header),
            &read_count, NULL) ||
        read_count != sizeof(optional_header) ||
        optional_header.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return 0u;
    }
    result = optional_header.ImageBase;
    return result;
}

static int revalidate_startup_file_identities(void)
{
    char current_self_path[MAX_PATH];
    char current_main_path[MAX_PATH];
    DWORD self_length;
    DWORD main_length;

    SetLastError(NO_ERROR);
    current_self_path[0] = '\0';
    current_main_path[0] = '\0';
    self_length = GetModuleFileNameA(
        g_self_module, current_self_path, MAX_PATH);
    main_length = GetModuleFileNameA(
        NULL, current_main_path, MAX_PATH);
    if (self_length == 0u || self_length >= MAX_PATH ||
        main_length == 0u || main_length >= MAX_PATH ||
        lstrcmpiA(current_self_path, g_self_path) != 0 ||
        lstrcmpiA(current_main_path, g_main_path) != 0 ||
        !revalidate_locked_file_identity(
            g_self_path, &g_self_file_identity) ||
        !revalidate_locked_file_identity(
            g_ini_path, &g_ini_file_identity) ||
        !revalidate_locked_file_identity(
            g_main_path, &g_main_file_identity)) {
        if (GetLastError() == NO_ERROR) {
            SetLastError(ERROR_INVALID_DATA);
        }
        return 0;
    }
    return 1;
}

static int ensure_startup_identities_before_image_write(void)
{
    if (g_report.identity_revalidation_passed) {
        return 1;
    }
    if (!revalidate_startup_file_identities()) {
        g_report.last_error = GetLastError();
        g_report.status = STATUS_IDENTITY_REVALIDATION_FAILED;
        return 0;
    }
    g_report.identity_revalidation_passed = 1u;
    return 1;
}

struct CrashLogBuilder {
    char bytes[CRASH_LOG_BUFFER_CAPACITY];
    DWORD length;
};

static void crash_log_append(
    struct CrashLogBuilder *builder, const char *text)
{
    while (text && *text &&
           builder->length + 1u < CRASH_LOG_BUFFER_CAPACITY) {
        builder->bytes[builder->length++] = *text++;
    }
    builder->bytes[builder->length] = '\0';
}

static void crash_log_append_u32(
    struct CrashLogBuilder *builder, DWORD value)
{
    char digits[16];
    DWORD count = 0;
    if (value == 0u) {
        crash_log_append(builder, "0");
        return;
    }
    while (value != 0u && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count > 0u) {
        char output[2];
        output[0] = digits[--count];
        output[1] = '\0';
        crash_log_append(builder, output);
    }
}

static void crash_log_append_hex32(
    struct CrashLogBuilder *builder, DWORD value)
{
    static const char hex[] = "0123456789ABCDEF";
    char output[11];
    int index;
    output[0] = '0';
    output[1] = 'x';
    for (index = 0; index < 8; ++index) {
        output[2 + index] =
            hex[(value >> ((7 - index) * 4)) & 0xFu];
    }
    output[10] = '\0';
    crash_log_append(builder, output);
}

static void crash_log_append_hex_bytes(
    struct CrashLogBuilder *builder,
    const BYTE *bytes, DWORD length)
{
    static const char hex[] = "0123456789ABCDEF";
    DWORD index;
    for (index = 0; index < length; ++index) {
        char output[4];
        output[0] = hex[(bytes[index] >> 4) & 0x0Fu];
        output[1] = hex[bytes[index] & 0x0Fu];
        output[2] = index + 1u < length ? ' ' : '\0';
        output[3] = '\0';
        crash_log_append(builder, output);
    }
}

static void crash_log_append_float(
    struct CrashLogBuilder *builder, float value)
{
    DWORD whole;
    DWORD fraction;
    if (value < 0.0f) {
        crash_log_append(builder, "-");
        value = -value;
    }
    if (value > 1000000.0f ||
        !nonnegative_finite_float(value)) {
        crash_log_append(builder, "(invalid/out-of-range)");
        return;
    }
    whole = (DWORD)value;
    fraction =
        (DWORD)((value - (float)whole) * 1000.0f + 0.5f);
    if (fraction >= 1000u) {
        ++whole;
        fraction = 0u;
    }
    crash_log_append_u32(builder, whole);
    crash_log_append(builder, ".");
    if (fraction < 100u) crash_log_append(builder, "0");
    if (fraction < 10u) crash_log_append(builder, "0");
    crash_log_append_u32(builder, fraction);
}

static void crash_log_key_u32(
    struct CrashLogBuilder *builder,
    const char *key, DWORD value)
{
    crash_log_append(builder, key);
    crash_log_append(builder, ": ");
    crash_log_append_u32(builder, value);
    crash_log_append(builder, "\r\n");
}

static void crash_log_key_hex(
    struct CrashLogBuilder *builder,
    const char *key, DWORD value)
{
    crash_log_append(builder, key);
    crash_log_append(builder, ": ");
    crash_log_append_hex32(builder, value);
    crash_log_append(builder, "\r\n");
}

static void crash_log_key_float(
    struct CrashLogBuilder *builder,
    const char *key, float value)
{
    crash_log_append(builder, key);
    crash_log_append(builder, ": ");
    crash_log_append_float(builder, value);
    crash_log_append(builder, "\r\n");
}

static void append_crash_diagnostics(
    PEXCEPTION_POINTERS exception_pointers,
    const char *stage,
    DWORD dump_type,
    DWORD dump_result,
    DWORD dump_error)
{
    struct CrashLogBuilder builder;
    struct TelemetrySnapshot snapshot;
    DWORD sample_number = 0;
    DWORD high_surface = 0;
    DWORD high_provider = 0;
    DWORD snapshot_valid;
    DWORD written = 0;
    DWORD index;
    HANDLE file;
    EXCEPTION_RECORD *record =
        exception_pointers ?
        exception_pointers->ExceptionRecord : NULL;
    CONTEXT *context =
        exception_pointers ?
        exception_pointers->ContextRecord : NULL;
    int targeted_log_only = strings_are_equal(
        stage,
        "TARGETED_FIRST_CHANCE_DIAGNOSTIC_ONLY_NO_DUMP");

    ZeroMemory(&builder, sizeof(builder));
    crash_log_append(
        &builder,
        "\r\n[CRASH DIAGNOSTICS]\r\n");
    crash_log_append(&builder, "Stage: ");
    crash_log_append(&builder, stage ? stage : "(unknown)");
    crash_log_append(&builder, "\r\n");
    crash_log_append(&builder, "Plugin version: ");
    crash_log_append(&builder, PLUGIN_VERSION);
    crash_log_append(&builder, "\r\n");
    crash_log_append(&builder, "Capture mode: ");
    crash_log_append(
        &builder,
        targeted_log_only ?
        "targeted first-chance log only; exception not intercepted\r\n" :
        "final unhandled-exception filter\r\n");
    crash_log_key_u32(
        &builder, "Dump existed before this launch",
        g_existing_crash_dump_at_start);
    crash_log_key_u32(
        &builder, "Process ID", GetCurrentProcessId());
    crash_log_key_u32(
        &builder, "Thread ID", GetCurrentThreadId());
    if (record) {
        crash_log_key_hex(
            &builder, "Exception code", record->ExceptionCode);
        crash_log_key_hex(
            &builder, "Exception flags", record->ExceptionFlags);
        crash_log_key_hex(
            &builder, "Exception address",
            (DWORD)record->ExceptionAddress);
        if ((DWORD)record->ExceptionAddress >=
                g_report.image_base &&
            (DWORD)record->ExceptionAddress <
                g_report.image_base + g_report.size_of_image) {
            crash_log_key_hex(
                &builder, "Exception GTAIV.exe RVA",
                (DWORD)record->ExceptionAddress -
                g_report.image_base);
        }
        if (readable_address_range(
                (const BYTE *)record->ExceptionAddress, 16u)) {
            crash_log_append(
                &builder, "Instruction bytes at exception address: ");
            crash_log_append_hex_bytes(
                &builder,
                (const BYTE *)record->ExceptionAddress, 16u);
            crash_log_append(&builder, "\r\n");
        }
        crash_log_key_u32(
            &builder, "Exception parameter count",
            record->NumberParameters);
        for (index = 0;
             index < record->NumberParameters &&
             index < EXCEPTION_MAXIMUM_PARAMETERS;
             ++index) {
            crash_log_append(&builder, "Exception parameter ");
            crash_log_append_u32(&builder, index);
            crash_log_append(&builder, ": ");
            crash_log_append_hex32(
                &builder,
                (DWORD)record->ExceptionInformation[index]);
            crash_log_append(&builder, "\r\n");
        }
    }
    if (context) {
        crash_log_append(&builder, "\r\nx86 registers:\r\n");
        crash_log_key_hex(&builder, "EIP", context->Eip);
        crash_log_key_hex(&builder, "ESP", context->Esp);
        crash_log_key_hex(&builder, "EBP", context->Ebp);
        crash_log_key_hex(&builder, "EAX", context->Eax);
        crash_log_key_hex(&builder, "EBX", context->Ebx);
        crash_log_key_hex(&builder, "ECX", context->Ecx);
        crash_log_key_hex(&builder, "EDX", context->Edx);
        crash_log_key_hex(&builder, "ESI", context->Esi);
        crash_log_key_hex(&builder, "EDI", context->Edi);
        crash_log_key_hex(&builder, "EFlags", context->EFlags);
    }

    crash_log_append(&builder, "\r\nPatch/config state:\r\n");
    crash_log_key_u32(&builder, "Patch status", g_report.status);
    crash_log_key_u32(
        &builder, "Startup quiescence completed",
        g_report.startup_quiescence_completed);
    crash_log_key_u32(
        &builder, "Startup exact-call records selected",
        g_report.startup_call_contract_record_count);
    crash_log_key_u32(
        &builder, "Startup exact-call preimages verified",
        g_report.startup_call_contract_preimages_verified);
    crash_log_key_hex(
        &builder, "Startup exact-call mismatch address",
        g_report.startup_call_contract_mismatch_address);
    crash_log_key_u32(
        &builder, "Startup guarded intervals selected",
        g_report.startup_guard_interval_count);
    crash_log_key_u32(
        &builder, "Startup guarded-interval validation passes",
        g_report.startup_guard_interval_validation_passes);
    crash_log_key_u32(
        &builder, "Startup guarded-interval bytes verified",
        g_report.startup_guard_interval_bytes_verified);
    crash_log_key_u32(
        &builder, "Startup guarded-interval relocations verified",
        g_report.startup_guard_interval_relocations_verified);
    crash_log_key_u32(
        &builder, "Startup quiescence failure kind",
        g_report.startup_quiescence_failure_kind);
    crash_log_key_u32(
        &builder, "Startup quiescence resume failures",
        g_report.startup_quiescence_resume_failures);
    crash_log_key_u32(
        &builder, "Thread-attach gate waits",
        g_report.startup_thread_attach_gate_waits);
    crash_log_key_u32(
        &builder, "Universal install lifecycle state",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_state, 0, 0));
    crash_log_key_u32(
        &builder, "Universal install lifecycle rejections",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_rejections, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter applied generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_applied_generation, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter wait failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_wait_failures, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter rundown failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_rundown_failures, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter generation exhaustions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation_exhaustions,
            0, 0));
    crash_log_key_u32(
        &builder, "Configured surface capacity",
        g_report.requested_capacity);
    crash_log_key_u32(
        &builder, "Requested rendered-object capacity",
        g_report.requested_rendered_object_capacity);
    crash_log_key_u32(
        &builder, "Effective rendered-object capacity",
        g_report.effective_rendered_object_capacity);
    crash_log_key_u32(
        &builder, "Configured provider capacity",
        g_report.requested_provider_capacity);
    crash_log_key_hex(
        &builder, "Configured distance multiplier bits",
        g_report.corrected_distance_multiplier_bits);
    crash_log_key_u32(
        &builder, "Generated PROCOBJ +0x50 lifecycle implementation present",
        GENERATED_PROCOBJ_DISTANCE_LIFECYCLE_ENABLED);
    crash_log_key_u32(
        &builder, "Generated PROCOBJ internal release observer active",
        g_report.universal_behavior_installed);
    crash_log_key_u32(
        &builder, "Tuning governor observation-only",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    crash_log_key_u32(
        &builder, "Generated distance release restores",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restores, 0, 0));
    crash_log_key_u32(
        &builder, "Generated distance release restore mismatches",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restore_mismatches, 0, 0));
    crash_log_key_u32(
        &builder, "Generated distance validation refusals",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_scaling_disabled, 0, 0));
    crash_log_key_u32(
        &builder, "Live generated ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0));
    crash_log_key_u32(
        &builder, "Live applied-distance ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_applied_active, 0, 0));
    crash_log_key_u32(
        &builder, "Successful ownership publications",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publications, 0, 0));
    crash_log_key_u32(
        &builder, "Successful owned release takes",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_release_takes, 0, 0));
    crash_log_key_u32(
        &builder, "Commit callbacks refused after shutdown",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_shutdown_commit_skips, 0, 0));
    crash_log_key_u32(
        &builder, "Publish/CAS rollback ownership losses",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cas_losses, 0, 0));
    crash_log_key_u32(
        &builder, "Publish/CAS rollback cleanup failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cleanup_failures,
            0, 0));
    crash_log_key_u32(
        &builder, "Ownership stale lookups",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.stale_lookups, 0, 0));
    crash_log_key_u32(
        &builder, "Ownership token exhaustion transitions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.token_exhaustions, 0, 0));
    crash_log_key_u32(
        &builder, "Model-distance status",
        g_report.model_distance_status);
    crash_log_key_u32(
        &builder, "Procedural models scaled",
        g_report.model_distance_models_scaled);
    crash_log_key_u32(
        &builder, "Models clamped to query coverage",
        g_report.model_distance_models_clamped);
    crash_log_key_u32(
        &builder, "Live model distances verified",
        g_report.model_distance_live_verified);
    crash_log_key_u32(
        &builder, "Live model-distance mismatches",
        g_report.model_distance_live_mismatches);
    crash_log_key_u32(
        &builder, "Configured plant density multiplier",
        g_report.plant_density_multiplier);
    crash_log_key_hex(
        &builder, "Configured PROCOBJ density multiplier bits",
        g_report.procobj_density_multiplier_bits);
    crash_log_key_u32(
        &builder, "Configured density class mask",
        g_report.density_class_mask);
    crash_log_key_hex(
        &builder, "Manager pointer", (DWORD)g_manager);
    crash_log_key_hex(
        &builder, "Loaded GTAIV.exe image base",
        g_report.image_base);
    crash_log_key_hex(
        &builder, "GTAIV.exe rebase delta",
        g_report.image_rebase_delta);
    crash_log_key_hex(
        &builder, "Provider storage pointer",
        (DWORD)g_provider_storage);
    crash_log_key_hex(
        &builder, "Provider records pointer",
        (DWORD)g_provider_records);
    crash_log_key_hex(
        &builder, "Provider storage end",
        (DWORD)g_provider_storage +
        g_report.provider_storage_bytes);
    crash_log_key_u32(
        &builder, "Provider patch sites applied",
        g_report.provider_patch_sites_applied);
    crash_log_key_u32(
        &builder, "Provider free list verified",
        g_report.provider_free_list_verified);
    crash_log_key_hex(
        &builder, "Generator manager pointer",
        (DWORD)g_generator_manager);
    crash_log_key_hex(
        &builder, "Generator external records",
        (DWORD)g_generator_extra_records);
    crash_log_key_u32(
        &builder, "Generator extra record count",
        g_generator_extra_record_count);
    crash_log_key_u32(
        &builder, "Generator patch sites applied",
        g_report.generator_patch_sites_applied);
    crash_log_key_u32(
        &builder, "Generator extra records issued",
        (DWORD)InterlockedCompareExchange(
            &g_generator_extra_issued, 0, 0));
    crash_log_key_u32(
        &builder, "Generator fallback allocator calls",
        (DWORD)InterlockedCompareExchange(
            &g_generator_fallback_calls, 0, 0));
    crash_log_key_u32(
        &builder, "Generator exhaustion latched",
        (DWORD)InterlockedCompareExchange(
            &g_generator_extra_exhaustion_observed,
            0, 0));
    crash_log_key_u32(
        &builder, "Definition scaling status",
        g_report.definition_scaling_status);
    crash_log_key_u32(
        &builder, "Definition callback state",
        (DWORD)InterlockedCompareExchange(
            &g_definition_callback_state, 0, 0));
    crash_log_key_u32(
        &builder, "Definition callback calls",
        (DWORD)InterlockedCompareExchange(
            &g_definition_callback_calls, 0, 0));
    crash_log_key_u32(
        &builder, "Definition callback thread ID",
        g_report.definition_callback_thread_id);
    crash_log_key_u32(
        &builder, "Tuning governor state",
        (DWORD)InterlockedCompareExchange(
            &g_tuning_governor_state, 0, 0));
    crash_log_key_u32(
        &builder, "Tuning governor live writes",
        g_report.tuning_governor_live_writes);
    crash_log_key_u32(
        &builder, "Tuning governor detection thread ID",
        g_report.tuning_governor_detection_thread_id);
    crash_log_key_u32(
        &builder, "PLANT definitions scaled",
        g_report.plant_definitions_scaled);
    crash_log_key_u32(
        &builder, "PROCOBJ definitions scaled",
        g_report.procobj_definitions_scaled);
    crash_log_key_u32(
        &builder, "Provider exhaustion latched",
        (DWORD)InterlockedCompareExchange(
            &g_provider_exhaustion_observed, 0, 0));
    crash_log_key_u32(
        &builder, "Surface exhaustion latched",
        (DWORD)InterlockedCompareExchange(
            &g_surface_exhaustion_observed, 0, 0));

    snapshot_valid = (DWORD)read_published_telemetry_snapshot(
        &snapshot, &sample_number,
        &high_surface, &high_provider);
    crash_log_append(
        &builder, "\r\nLast coherent telemetry snapshot:\r\n");
    crash_log_key_u32(
        &builder, "Snapshot available", snapshot_valid);
    if (snapshot_valid) {
        crash_log_key_u32(
            &builder, "Sample number", sample_number);
        crash_log_key_u32(
            &builder, "Snapshot invariants valid",
            snapshot.invariant_valid);
        crash_log_key_u32(
            &builder, "Provider active", snapshot.source_active);
        crash_log_key_u32(
            &builder, "Provider free", snapshot.source_free);
        crash_log_key_u32(
            &builder, "Provider high watermark", high_provider);
        crash_log_key_u32(
            &builder, "Surface active", snapshot.surface_active);
        crash_log_key_u32(
            &builder, "Surface free", snapshot.surface_free);
        crash_log_key_u32(
            &builder, "Surface high watermark", high_surface);
        crash_log_key_u32(
            &builder, "Generator accounting valid",
            snapshot.generator_invariant_valid);
        crash_log_key_u32(
            &builder, "Generator rendered entities",
            snapshot.generator_active_rendered);
        crash_log_key_u32(
            &builder, "Generator rendered high watermark",
            (DWORD)InterlockedCompareExchange(
                &g_generator_active_high_watermark, 0, 0));
        crash_log_key_u32(
            &builder, "Generator active wrapper records",
            snapshot.generator_active_wrapper_records);
        crash_log_key_u32(
            &builder, "Generator free records",
            snapshot.generator_free_records);
        crash_log_key_u32(
            &builder, "Generator staging count",
            snapshot.generator_staging_count);
        crash_log_key_u32(
            &builder, "Generator staging high watermark",
            (DWORD)InterlockedCompareExchange(
                &g_generator_staging_high_watermark, 0, 0));
        crash_log_key_u32(
            &builder, "Surface records with raw flag bit 0",
            snapshot.surface_flag_bit0_records);
        crash_log_key_u32(
            &builder, "Surface records with raw flag bit 1",
            snapshot.surface_flag_bit1_records);
        crash_log_key_u32(
            &builder, "Staging A total",
            snapshot.staging_totals[0]);
        crash_log_key_u32(
            &builder, "Staging B total",
            snapshot.staging_totals[1]);
        crash_log_key_float(
            &builder, "Live Detail Distance",
            snapshot.detail_distance);
        crash_log_key_float(
            &builder, "Actual plant far distance",
            snapshot.plant_far);
        crash_log_key_float(
            &builder, "Expected plant far distance",
            snapshot.expected_plant_far);
        crash_log_key_float(
            &builder, "Actual surface query radius",
            snapshot.query_radius);
        crash_log_key_float(
            &builder, "Expected surface query radius",
            snapshot.expected_query_radius);
        crash_log_key_float(
            &builder, "Manager position X",
            snapshot.position_x);
        crash_log_key_float(
            &builder, "Manager position Y",
            snapshot.position_y);
        crash_log_key_float(
            &builder, "Manager position Z",
            snapshot.position_z);
    }

    crash_log_append(&builder, "\r\nDump result:\r\n");
    if (targeted_log_only) {
        crash_log_append(
            &builder,
            "Dump intentionally not attempted by the first-chance "
            "observer. The exception was allowed to continue to the "
            "game and its final unhandled handler.\r\n");
    } else {
        crash_log_append(&builder, "Dump path: ");
        crash_log_append(&builder, g_crash_dump_path);
        crash_log_append(&builder, "\r\n");
        crash_log_key_hex(&builder, "Dump type flags", dump_type);
        crash_log_key_u32(
            &builder, "MiniDumpWriteDump result", dump_result);
        crash_log_key_u32(
            &builder, "Dump Win32 error", dump_error);
    }

    file = CreateFileA(
        g_log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(
            file, builder.bytes, builder.length,
            &written, NULL);
        FlushFileBuffers(file);
        CloseHandle(file);
    }
}

static int should_capture_exception(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case 0xC0000374u: /* STATUS_HEAP_CORRUPTION */
    case 0xC0000409u: /* STATUS_STACK_BUFFER_OVERRUN */
        return 1;
    default:
        return 0;
    }
}

static int address_is_in_half_open_range(
    DWORD address, DWORD start, DWORD end)
{
    return end > start &&
           address >= start &&
           address < end;
}

static int provider_exception_is_relevant(
    PEXCEPTION_POINTERS exception_pointers)
{
    EXCEPTION_RECORD *record;
    DWORD instruction;
    DWORD procedural_start;
    DWORD procedural_end;
    DWORD generator_start;
    DWORD generator_end;
    MEMORY_BASIC_INFORMATION information;

    if (!exception_pointers ||
        !exception_pointers->ExceptionRecord) {
        return 0;
    }
    record = exception_pointers->ExceptionRecord;
    if (!should_capture_exception(record->ExceptionCode)) {
        return 0;
    }

    instruction = (DWORD)record->ExceptionAddress;
    procedural_start =
        g_report.image_base + PROCEDURAL_CODE_FIRST_RVA;
    procedural_end =
        g_report.image_base + PROCEDURAL_CODE_LAST_RVA;
    if (address_is_in_half_open_range(
            instruction, procedural_start, procedural_end)) {
        return 1;
    }
    generator_start =
        g_report.image_base + GENERATOR_CODE_FIRST_RVA;
    generator_end =
        g_report.image_base + GENERATOR_CODE_LAST_RVA;
    if (address_is_in_half_open_range(
            instruction, generator_start, generator_end)) {
        return 1;
    }

    ZeroMemory(&information, sizeof(information));
    if (VirtualQuery(
            (const void *)instruction, &information,
            sizeof(information)) == sizeof(information) &&
        information.AllocationBase == g_self_module) {
        return 1;
    }

    if (record->NumberParameters >= 2u &&
        g_provider_storage &&
        g_report.provider_storage_bytes != 0u) {
        DWORD fault_address =
            (DWORD)record->ExceptionInformation[1];
        DWORD provider_start = (DWORD)g_provider_storage;
        DWORD provider_end =
            provider_start + g_report.provider_storage_bytes;
        DWORD margin_start =
            provider_start > PROVIDER_EXCEPTION_ADDRESS_MARGIN ?
            provider_start - PROVIDER_EXCEPTION_ADDRESS_MARGIN :
            0u;
        DWORD margin_end =
            provider_end <=
                0xFFFFFFFFu - PROVIDER_EXCEPTION_ADDRESS_MARGIN ?
            provider_end + PROVIDER_EXCEPTION_ADDRESS_MARGIN :
            0xFFFFFFFFu;
        if (address_is_in_half_open_range(
                fault_address, margin_start, margin_end)) {
            return 1;
        }
    }
    if (record->NumberParameters >= 2u &&
        g_generator_extra_records &&
        g_report.generator_extra_storage_bytes != 0u) {
        DWORD fault_address =
            (DWORD)record->ExceptionInformation[1];
        DWORD storage_start =
            (DWORD)g_generator_extra_records;
        DWORD storage_end =
            storage_start +
            g_report.generator_extra_storage_bytes;
        DWORD margin_start =
            storage_start >
                PROVIDER_EXCEPTION_ADDRESS_MARGIN ?
            storage_start -
                PROVIDER_EXCEPTION_ADDRESS_MARGIN :
            0u;
        DWORD margin_end =
            storage_end <=
                0xFFFFFFFFu -
                PROVIDER_EXCEPTION_ADDRESS_MARGIN ?
            storage_end +
                PROVIDER_EXCEPTION_ADDRESS_MARGIN :
            0xFFFFFFFFu;
        if (address_is_in_half_open_range(
                fault_address, margin_start, margin_end)) {
            return 1;
        }
    }
    return 0;
}

static LONG WINAPI targeted_provider_exception_logger(
    PEXCEPTION_POINTERS exception_pointers)
{
    if (provider_exception_is_relevant(exception_pointers) &&
        InterlockedCompareExchange(
            &g_targeted_exception_logged, 1, 0) == 0) {
        append_crash_diagnostics(
            exception_pointers,
            "TARGETED_FIRST_CHANCE_DIAGNOSTIC_ONLY_NO_DUMP",
            0u, 0u, 0u);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI crash_exception_handler(
    PEXCEPTION_POINTERS exception_pointers);

static LONG continue_unhandled_chain(
    PEXCEPTION_POINTERS exception_pointers)
{
    if (g_previous_unhandled_filter &&
        g_previous_unhandled_filter != crash_exception_handler) {
        return g_previous_unhandled_filter(exception_pointers);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static LONG WINAPI crash_exception_handler(
    PEXCEPTION_POINTERS exception_pointers)
{
    HANDLE dump_file;
    struct MiniDumpExceptionInformationLocal exception_information;
    DWORD dump_type = MINIDUMP_COMPACT_TYPE;
    DWORD dump_error = 0;
    BOOL dump_result;

    if (!exception_pointers ||
        !exception_pointers->ExceptionRecord ||
        !should_capture_exception(
            exception_pointers->ExceptionRecord->ExceptionCode)) {
        return continue_unhandled_chain(exception_pointers);
    }

    if (
        InterlockedCompareExchange(&g_crash_dump_started, 1, 0) != 0) {
        return continue_unhandled_chain(exception_pointers);
    }
    if (!g_mini_dump_write_dump) {
        append_crash_diagnostics(
            exception_pointers, "DBGHELP_LOAD_FAILED",
            0u, 0u, ERROR_PROC_NOT_FOUND);
        return continue_unhandled_chain(exception_pointers);
    }

    dump_file = CreateFileA(
        g_crash_dump_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dump_file == INVALID_HANDLE_VALUE) {
        append_crash_diagnostics(
            exception_pointers, "DUMP_FILE_CREATE_FAILED",
            0u, 0u, GetLastError());
        return continue_unhandled_chain(exception_pointers);
    }

    exception_information.thread_id = GetCurrentThreadId();
    exception_information.exception_pointers = exception_pointers;
    exception_information.client_pointers = FALSE;
    dump_result = g_mini_dump_write_dump(
        GetCurrentProcess(), GetCurrentProcessId(), dump_file,
        dump_type, &exception_information, NULL, NULL);
    dump_error = dump_result ? 0u : GetLastError();
    FlushFileBuffers(dump_file);
    CloseHandle(dump_file);
    if (!dump_result) {
        dump_type = MINIDUMP_COMPACT_FALLBACK_TYPE;
        dump_file = CreateFileA(
            g_crash_fallback_dump_path, GENERIC_WRITE,
            FILE_SHARE_READ, NULL, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (dump_file != INVALID_HANDLE_VALUE) {
            dump_result = g_mini_dump_write_dump(
                GetCurrentProcess(), GetCurrentProcessId(), dump_file,
                dump_type, &exception_information, NULL, NULL);
            dump_error = dump_result ? 0u : GetLastError();
            FlushFileBuffers(dump_file);
            CloseHandle(dump_file);
        } else {
            dump_error = GetLastError();
        }
    }
    append_crash_diagnostics(
        exception_pointers,
        dump_result ? "DUMP_WRITTEN" : "DUMP_WRITE_FAILED",
        dump_type, (DWORD)dump_result, dump_error);
    return continue_unhandled_chain(exception_pointers);
}

struct GameWindowSearchContext {
    DWORD process_id;
    HWND best_window;
    DWORD best_area;
};

static BOOL CALLBACK find_game_window_callback(HWND window, LPARAM parameter)
{
    struct GameWindowSearchContext *context =
        (struct GameWindowSearchContext *)parameter;
    DWORD process_id = 0u;
    RECT rectangle;
    DWORD width;
    DWORD height;
    DWORD area;

    if (!context || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != NULL) {
        return TRUE;
    }
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != context->process_id ||
        !GetClientRect(window, &rectangle)) {
        return TRUE;
    }
    width = rectangle.right > rectangle.left ?
        (DWORD)(rectangle.right - rectangle.left) : 0u;
    height = rectangle.bottom > rectangle.top ?
        (DWORD)(rectangle.bottom - rectangle.top) : 0u;
    if (width == 0u || height == 0u ||
        width > 0xFFFFFFFFu / height) {
        return TRUE;
    }
    area = width * height;
    if (area > context->best_area) {
        context->best_area = area;
        context->best_window = window;
    }
    return TRUE;
}

static HWND find_main_game_window(void)
{
    struct GameWindowSearchContext context;
    ZeroMemory(&context, sizeof(context));
    context.process_id = GetCurrentProcessId();
    EnumWindows(find_game_window_callback, (LPARAM)&context);
    return context.best_window;
}

static int game_window_is_responsive(HWND window, DWORD *probe_error)
{
    DWORD_PTR result = 0u;
    LRESULT send_result;

    if (probe_error) {
        *probe_error = 0u;
    }
    if (!window) {
        return -1;
    }
    SetLastError(0u);
    send_result = SendMessageTimeoutA(
        window, WM_NULL, 0u, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,
        HANG_WATCHDOG_PROBE_TIMEOUT_MS, &result);
    if (send_result == 0) {
        if (probe_error) {
            *probe_error = GetLastError();
        }
        return 0;
    }
    return 1;
}

static void append_hang_diagnostics(
    const char *stage, DWORD failed_probes,
    DWORD probe_error, DWORD dump_type,
    DWORD dump_result, DWORD dump_error)
{
    struct CrashLogBuilder builder;
    struct TelemetrySnapshot snapshot;
    DWORD sample_number = 0u;
    DWORD high_surface = 0u;
    DWORD high_provider = 0u;
    DWORD snapshot_valid;
    DWORD written = 0u;
    HANDLE file;

    ZeroMemory(&builder, sizeof(builder));
    crash_log_append(&builder, "\r\n[GAME FREEZE WATCHDOG]\r\n");
    crash_log_append(&builder, "Stage: ");
    crash_log_append(&builder, stage ? stage : "(unknown)");
    crash_log_append(&builder, "\r\nPlugin version: ");
    crash_log_append(&builder, PLUGIN_VERSION);
    crash_log_append(&builder, "\r\nDetection method: process-owned GTA IV window failed WM_NULL ");
    crash_log_append(&builder, "SendMessageTimeout probes. This captures a hang; it does not ");
    crash_log_append(&builder, "claim the mod caused every possible freeze.\r\n");
    crash_log_key_u32(&builder, "Failed consecutive probes", failed_probes);
    crash_log_key_u32(
        &builder, "Configured threshold seconds", g_hang_timeout_seconds);
    crash_log_key_u32(&builder, "Last probe Win32 error", probe_error);
    crash_log_key_hex(
        &builder, "Last probed window",
        (DWORD)InterlockedCompareExchange(&g_hang_last_window, 0, 0));
    crash_log_key_u32(
        &builder, "Configured surface capacity",
        g_report.requested_capacity);
    crash_log_key_u32(
        &builder, "Configured rendered-object capacity",
        g_report.effective_rendered_object_capacity);
    crash_log_key_u32(
        &builder, "Configured provider capacity",
        g_report.requested_provider_capacity);
    crash_log_key_hex(
        &builder, "Configured distance multiplier bits",
        g_report.corrected_distance_multiplier_bits);
    crash_log_key_u32(
        &builder, "Generated PROCOBJ +0x50 lifecycle implementation present",
        GENERATED_PROCOBJ_DISTANCE_LIFECYCLE_ENABLED);
    crash_log_key_u32(
        &builder, "Generated PROCOBJ internal release observer active",
        g_report.universal_behavior_installed);
    crash_log_key_u32(
        &builder, "Tuning governor observation-only",
        TUNING_GOVERNOR_OBSERVATION_ONLY);
    crash_log_key_u32(
        &builder, "Generated distance release restores",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restores, 0, 0));
    crash_log_key_u32(
        &builder, "Generated distance release restore mismatches",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.release_restore_mismatches, 0, 0));
    crash_log_key_u32(
        &builder, "Generated distance validation refusals",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_scaling_disabled, 0, 0));
    crash_log_key_u32(
        &builder, "Live generated ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.active_count, 0, 0));
    crash_log_key_u32(
        &builder, "Live applied-distance ownership records",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.distance_applied_active, 0, 0));
    crash_log_key_u32(
        &builder, "Successful ownership publications",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publications, 0, 0));
    crash_log_key_u32(
        &builder, "Successful owned release takes",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_release_takes, 0, 0));
    crash_log_key_u32(
        &builder, "Commit callbacks refused after shutdown",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_shutdown_commit_skips, 0, 0));
    crash_log_key_u32(
        &builder, "Publish/CAS rollback ownership losses",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cas_losses, 0, 0));
    crash_log_key_u32(
        &builder, "Ownership stale lookups",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.stale_lookups, 0, 0));
    crash_log_key_u32(
        &builder, "Ownership token exhaustion transitions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_ownership.token_exhaustions, 0, 0));
    crash_log_key_u32(
        &builder, "Model-distance status",
        g_report.model_distance_status);
    crash_log_key_u32(
        &builder, "Procedural models scaled",
        g_report.model_distance_models_scaled);
    crash_log_key_u32(
        &builder, "Models clamped to query coverage",
        g_report.model_distance_models_clamped);
    crash_log_key_u32(
        &builder, "Live model distances verified",
        g_report.model_distance_live_verified);
    crash_log_key_u32(
        &builder, "Live model-distance mismatches",
        g_report.model_distance_live_mismatches);
    crash_log_key_hex(
        &builder, "Configured PLANT density multiplier bits",
        g_report.corrected_plant_density_multiplier_bits);
    crash_log_key_hex(
        &builder, "Configured PROCOBJ density multiplier bits",
        g_report.procobj_density_multiplier_bits);
    crash_log_key_u32(
        &builder, "Configured density class mask",
        g_report.density_class_mask);

    crash_log_key_u32(
        &builder, "Startup quiescence completed",
        g_report.startup_quiescence_completed);
    crash_log_key_u32(
        &builder, "Startup exact-call records selected",
        g_report.startup_call_contract_record_count);
    crash_log_key_u32(
        &builder, "Startup exact-call preimages verified",
        g_report.startup_call_contract_preimages_verified);
    crash_log_key_hex(
        &builder, "Startup exact-call mismatch address",
        g_report.startup_call_contract_mismatch_address);
    crash_log_key_u32(
        &builder, "Startup guarded intervals selected",
        g_report.startup_guard_interval_count);
    crash_log_key_u32(
        &builder, "Startup guarded-interval validation passes",
        g_report.startup_guard_interval_validation_passes);
    crash_log_key_u32(
        &builder, "Startup guarded-interval bytes verified",
        g_report.startup_guard_interval_bytes_verified);
    crash_log_key_u32(
        &builder, "Startup guarded-interval relocations verified",
        g_report.startup_guard_interval_relocations_verified);
    crash_log_key_u32(
        &builder, "Startup quiescence failure kind",
        g_report.startup_quiescence_failure_kind);
    crash_log_key_u32(
        &builder, "Startup quiescence resume failures",
        g_report.startup_quiescence_resume_failures);
    crash_log_key_u32(
        &builder, "Thread-attach gate waits",
        g_report.startup_thread_attach_gate_waits);
    crash_log_key_u32(
        &builder, "Universal install lifecycle state",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_state, 0, 0));
    crash_log_key_u32(
        &builder, "Universal install lifecycle rejections",
        (DWORD)InterlockedCompareExchange(
            &g_gu_install_lifecycle_rejections, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter applied generation",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_applied_generation, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter wait failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_wait_failures, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter rundown failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_rundown_failures, 0, 0));
    crash_log_key_u32(
        &builder, "Manager setter generation exhaustions",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.manager_setter_generation_exhaustions,
            0, 0));
    crash_log_key_u32(
        &builder, "Publish/CAS rollback cleanup failures",
        (DWORD)InterlockedCompareExchange(
            &g_gu_behavior.ownership_publish_cleanup_failures,
            0, 0));

    snapshot_valid = (DWORD)read_published_telemetry_snapshot(
        &snapshot, &sample_number, &high_surface, &high_provider);
    crash_log_append(&builder, "\r\nLast coherent telemetry snapshot:\r\n");
    crash_log_key_u32(&builder, "Snapshot valid", snapshot_valid);
    if (snapshot_valid) {
        crash_log_key_u32(&builder, "Sample", sample_number);
        crash_log_key_u32(&builder, "Snapshot invariants valid", snapshot.invariant_valid);
        crash_log_key_u32(&builder, "Provider active", snapshot.source_active);
        crash_log_key_u32(&builder, "Provider free", snapshot.source_free);
        crash_log_key_u32(&builder, "Provider high watermark", high_provider);
        crash_log_key_u32(&builder, "Surface active", snapshot.surface_active);
        crash_log_key_u32(&builder, "Surface free", snapshot.surface_free);
        crash_log_key_u32(&builder, "Surface high watermark", high_surface);
        crash_log_key_u32(
            &builder, "Generator runtime initialized",
            snapshot.generator_initialized);
        crash_log_key_u32(
            &builder, "Live rendered procedural entities",
            snapshot.generator_active_rendered);
        crash_log_key_u32(
            &builder, "Active generator wrappers",
            snapshot.generator_active_wrapper_records);
        crash_log_key_u32(
            &builder, "Generator free wrappers",
            snapshot.generator_free_records);
        crash_log_key_u32(
            &builder, "Extra wrapper records issued",
            snapshot.generator_extra_issued);
        crash_log_key_u32(
            &builder, "Fallback allocator calls",
            snapshot.generator_fallback_calls);
        crash_log_key_u32(
            &builder, "Expanded allocator exhaustion observed",
            snapshot.generator_extra_exhaustion_observed);
        crash_log_key_u32(
            &builder, "Generator staging count",
            snapshot.generator_staging_count);
        crash_log_key_float(&builder, "Plant far distance", snapshot.plant_far);
        crash_log_key_float(&builder, "Surface query radius", snapshot.query_radius);
        crash_log_key_float(&builder, "Manager X", snapshot.position_x);
        crash_log_key_float(&builder, "Manager Y", snapshot.position_y);
        crash_log_key_float(&builder, "Manager Z", snapshot.position_z);
    }
    crash_log_append(&builder, "\r\nHang dump path: ");
    crash_log_append(&builder, g_hang_dump_path);
    crash_log_append(&builder, "\r\n");
    crash_log_key_hex(&builder, "Dump type flags", dump_type);
    crash_log_key_u32(&builder, "MiniDumpWriteDump result", dump_result);
    crash_log_key_u32(&builder, "Dump Win32 error", dump_error);

    file = CreateFileA(
        g_hang_summary_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, builder.bytes, builder.length, &written, NULL);
        FlushFileBuffers(file);
        CloseHandle(file);
    }

    file = CreateFileA(
        g_log_path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        WriteFile(file, builder.bytes, builder.length, &written, NULL);
        FlushFileBuffers(file);
        CloseHandle(file);
    }
}

static BOOL write_hang_dump(
    DWORD failed_probes, DWORD probe_error,
    DWORD *used_dump_type, DWORD *dump_error)
{
    HANDLE dump_file;
    DWORD dump_type = g_full_hang_dump_enabled ?
        MINIDUMP_FULL_OPT_IN_TYPE : MINIDUMP_COMPACT_TYPE;
    BOOL dump_result = FALSE;

    if (used_dump_type) {
        *used_dump_type = 0u;
    }
    if (dump_error) {
        *dump_error = 0u;
    }
    if (!g_mini_dump_write_dump) {
        if (dump_error) {
            *dump_error = ERROR_PROC_NOT_FOUND;
        }
        append_hang_diagnostics(
            "DBGHELP_LOAD_FAILED", failed_probes, probe_error,
            0u, 0u, dump_error ? *dump_error : 0u);
        return FALSE;
    }

    dump_file = CreateFileA(
        g_hang_dump_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dump_file == INVALID_HANDLE_VALUE) {
        if (dump_error) {
            *dump_error = GetLastError();
        }
        append_hang_diagnostics(
            "HANG_DUMP_FILE_CREATE_FAILED", failed_probes, probe_error,
            0u, 0u, dump_error ? *dump_error : 0u);
        return FALSE;
    }
    dump_result = g_mini_dump_write_dump(
        GetCurrentProcess(), GetCurrentProcessId(), dump_file,
        dump_type, NULL, NULL, NULL);
    if (!dump_result && dump_error) {
        *dump_error = GetLastError();
    }
    FlushFileBuffers(dump_file);
    CloseHandle(dump_file);

    if (!dump_result) {
        dump_type = MINIDUMP_COMPACT_FALLBACK_TYPE;
        dump_file = CreateFileA(
            g_hang_fallback_dump_path, GENERIC_WRITE,
            FILE_SHARE_READ, NULL, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (dump_file != INVALID_HANDLE_VALUE) {
            dump_result = g_mini_dump_write_dump(
                GetCurrentProcess(), GetCurrentProcessId(), dump_file,
                dump_type, NULL, NULL, NULL);
            if (dump_error) {
                *dump_error = dump_result ? 0u : GetLastError();
            }
            FlushFileBuffers(dump_file);
            CloseHandle(dump_file);
        } else if (dump_error) {
            *dump_error = GetLastError();
        }
    }
    if (used_dump_type) {
        *used_dump_type = dump_type;
    }
    append_hang_diagnostics(
        dump_result ? "HANG_DUMP_WRITTEN" : "HANG_DUMP_WRITE_FAILED",
        failed_probes, probe_error, dump_type,
        (DWORD)dump_result, dump_error ? *dump_error : 0u);
    return dump_result;
}

static DWORD WINAPI hang_watchdog_thread_main(LPVOID parameter)
{
    DWORD failed_probes = 0u;
    DWORD threshold_probes;
    DWORD probe_error;
    DWORD dump_type;
    DWORD dump_error;
    HWND window;
    int responsive;

    (void)parameter;
    if (!wait_for_background_threads_ready(&g_hang_watchdog_stop)) {
        return 0u;
    }
    Sleep(HANG_WATCHDOG_STARTUP_GRACE_MS);
    threshold_probes =
        (g_hang_timeout_seconds * 1000u +
         HANG_WATCHDOG_POLL_MS - 1u) /
        HANG_WATCHDOG_POLL_MS;
    if (threshold_probes == 0u) {
        threshold_probes = 1u;
    }

    while (InterlockedCompareExchange(
               &g_hang_watchdog_stop, 0, 0) == 0) {
        probe_error = 0u;
        window = find_main_game_window();
        InterlockedExchange(&g_hang_last_window, (LONG)(DWORD)window);
        responsive = game_window_is_responsive(window, &probe_error);
        InterlockedExchange(&g_hang_last_probe_error, (LONG)probe_error);
        if (responsive > 0) {
            failed_probes = 0u;
        } else if (responsive == 0) {
            ++failed_probes;
        }
        InterlockedExchange(
            &g_hang_consecutive_failures, (LONG)failed_probes);

        if (failed_probes >= threshold_probes &&
            InterlockedCompareExchange(
                &g_hang_dump_started, 1, 0) == 0) {
            dump_type = 0u;
            dump_error = 0u;
            write_hang_dump(
                failed_probes, probe_error,
                &dump_type, &dump_error);
            if (g_hang_message_box_enabled) {
                MessageBoxA(
                    NULL,
                    "GTA IV stopped responding long enough to trigger the "
                    "Procedural Fixes freeze watchdog. A diagnostic log and "
                    "hang dump were written beside the ASI. The watchdog does "
                    "not terminate the game automatically.",
                    "GTAIV Procedural Fixes - freeze detected",
                    MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
            }
            failed_probes = 0u;
        }
        Sleep(HANG_WATCHDOG_POLL_MS);
    }
    return 0u;
}

static void start_hang_watchdog(void)
{
    HANDLE thread;

    if (!g_hang_watchdog_enabled ||
        g_report.status != STATUS_PATCH_APPLIED) {
        return;
    }
    InterlockedExchange(&g_hang_watchdog_stop, 0);
    InterlockedExchange(&g_hang_dump_started, 0);
    InterlockedExchange(&g_hang_consecutive_failures, 0);
    InterlockedExchange(&g_hang_last_probe_error, 0);
    InterlockedExchange(&g_hang_last_window, 0);
    thread = CreateThread(
        NULL, 0, hang_watchdog_thread_main, NULL, 0, NULL);
    if (thread) {
        g_report.hang_watchdog_thread_started = 1u;
        CloseHandle(thread);
    } else {
        g_report.last_error = GetLastError();
    }
}

static void install_crash_dump_handler(void)
{
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

    if (!kernel32) {
        return;
    }
    g_set_unhandled_exception_filter =
        (SetUnhandledExceptionFilterFunction)GetProcAddress(
            kernel32, "SetUnhandledExceptionFilter");
    if (!g_set_unhandled_exception_filter) {
        return;
    }
    g_previous_unhandled_filter =
        g_set_unhandled_exception_filter(crash_exception_handler);
    g_unhandled_filter_installed = 1u;
    g_report.crash_handler_installed = 1u;
}

static void install_targeted_exception_logger(void)
{
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    AddVectoredExceptionHandlerFunction add_handler;

    if (!kernel32) {
        return;
    }
    add_handler =
        (AddVectoredExceptionHandlerFunction)GetProcAddress(
            kernel32, "AddVectoredExceptionHandler");
    g_remove_vectored_exception_handler =
        (RemoveVectoredExceptionHandlerFunction)GetProcAddress(
            kernel32, "RemoveVectoredExceptionHandler");
    if (!add_handler || !g_remove_vectored_exception_handler) {
        return;
    }
    InterlockedExchange(&g_targeted_exception_logged, 0);
    g_targeted_vectored_handler =
        add_handler(1, targeted_provider_exception_logger);
    g_report.targeted_exception_handler_installed =
        g_targeted_vectored_handler != NULL;
}

static int restore_code_u32(BYTE *address, DWORD value)
{
    DWORD old_protection = 0;
    int restored;

    if (!VirtualProtect(address, 4, PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        return 0;
    }

    write_u32(address, value);
    restored = read_u32(address) == value;
    if (restored) {
        restored = flush_patch_span_retry(address, 4u);
    }

    if (!restore_patch_page_protection_retry(
            address, 4u, old_protection)) {
        restored = 0;
    }
    return restored;
}

static int rollback_capacity_patch_writable(
    BYTE *capacity_immediate, DWORD old_protection)
{
    int bytes_restored;
    int cache_flushed;
    int protection_restored;
    write_u32(capacity_immediate, VANILLA_CAPACITY);
    g_report.final_immediate = read_u32(capacity_immediate);
    bytes_restored =
        g_report.final_immediate == VANILLA_CAPACITY;
    cache_flushed = flush_patch_span_retry(capacity_immediate, 4u);
    protection_restored = restore_patch_page_protection_retry(
        capacity_immediate, 4u, old_protection);
    if (!bytes_restored || !cache_flushed || !protection_restored) {
        return 0;
    }
    InterlockedExchange(&g_capacity_patch_transaction_dirty, 0);
    return 1;
}

static int rollback_capacity_after_downstream_failure(
    BYTE *capacity_immediate)
{
    DWORD observed_protection = 0u;
    DWORD target_protection;
    LONG dirty = InterlockedCompareExchange(
        &g_capacity_patch_transaction_dirty, 0, 0);
    if (g_report.final_immediate == VANILLA_CAPACITY && dirty == 0) {
        return 1;
    }
    if (!VirtualProtect(
            capacity_immediate, 4u, PAGE_EXECUTE_READWRITE,
            &observed_protection)) {
        return 0;
    }
    target_protection = dirty != 0 && g_report.old_protection != 0u ?
        g_report.old_protection : observed_protection;
    InterlockedExchange(&g_capacity_patch_transaction_dirty, 1);
    return rollback_capacity_patch_writable(
        capacity_immediate, target_protection);
}

static int apply_capacity_patch_transaction(
    BYTE *capacity_immediate, DWORD requested)
{
    DWORD old_protection = 0u;
    DWORD unused_protection = 0u;
    if (!VirtualProtect(
            capacity_immediate, 4u, PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        g_report.status = STATUS_VIRTUAL_PROTECT_FAILED;
        g_report.last_error = GetLastError();
        return 0;
    }
    g_report.old_protection = old_protection;
    InterlockedExchange(&g_capacity_patch_transaction_dirty, 1);

    write_u32(capacity_immediate, requested);
    g_report.final_immediate = read_u32(capacity_immediate);
    if (g_report.final_immediate != requested) {
        int restored = rollback_capacity_patch_writable(
            capacity_immediate, old_protection);
        g_report.status = restored ?
            STATUS_WRITE_VERIFY_FAILED :
            STATUS_CAPACITY_ROLLBACK_FAILED;
        return 0;
    }

    g_report.flush_result = (DWORD)FlushInstructionCache(
        GetCurrentProcess(), capacity_immediate, 4u);
    if (!g_report.flush_result) {
        DWORD flush_error = GetLastError();
        int restored = rollback_capacity_patch_writable(
            capacity_immediate, old_protection);
        g_report.status = restored ?
            STATUS_FLUSH_FAILED_REVERTED :
            STATUS_CAPACITY_ROLLBACK_FAILED;
        g_report.last_error = flush_error;
        return 0;
    }

    g_report.protection_restore_result =
        (DWORD)restore_patch_page_protection_once(
            capacity_immediate, 4u, old_protection,
            &unused_protection);
    if (!g_report.protection_restore_result) {
        DWORD protection_error = GetLastError();
        int restored = rollback_capacity_patch_writable(
            capacity_immediate, old_protection);
        g_report.status = restored ?
            STATUS_PROTECTION_RESTORE_FAILED :
            STATUS_CAPACITY_ROLLBACK_FAILED;
        g_report.last_error = protection_error;
        return 0;
    }
    InterlockedExchange(&g_capacity_patch_transaction_dirty, 0);
    return 1;
}

static int rollback_provider_after_downstream_failure(void)
{
    BYTE *span_start;
    BYTE *span_end;
    DWORD span;
    DWORD old_protection = 0u;
    DWORD target_protection;
    DWORD index;
    LONG dirty = InterlockedCompareExchange(
        &g_provider_patch_transaction_dirty, 0, 0);
    if (g_report.provider_patch_sites_applied == 0u && dirty == 0) {
        return 1;
    }
    span_start = g_provider_patch_manifest[0].address;
    span_end = span_start + g_provider_patch_manifest[0].length;
    for (index = 0u; index < PROVIDER_PATCH_COUNT; ++index) {
        BYTE *patch_end =
            g_provider_patch_manifest[index].address +
            g_provider_patch_manifest[index].length;
        if (dirty == 0 && !bytes_are_equal(
                g_provider_patch_manifest[index].address,
                g_provider_patch_manifest[index].replacement,
                g_provider_patch_manifest[index].length)) {
            return 0;
        }
        if (g_provider_patch_manifest[index].address < span_start) {
            span_start = g_provider_patch_manifest[index].address;
        }
        if (patch_end > span_end) {
            span_end = patch_end;
        }
    }
    span = (DWORD)(span_end - span_start);
    if (!VirtualProtect(
            span_start, span, PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        return 0;
    }
    target_protection = dirty != 0 &&
            g_report.provider_old_protection != 0u ?
        g_report.provider_old_protection : old_protection;
    InterlockedExchange(&g_provider_patch_transaction_dirty, 1);
    if (!rollback_provider_patch_transaction_writable(
            g_provider_patch_manifest,
            span_start, span, target_protection)) {
        return 0;
    }
    return 1;
}

static int rollback_generator_after_downstream_failure(void)
{
    BYTE *span_start;
    BYTE *span_end;
    DWORD span;
    DWORD old_protection = 0u;
    DWORD target_protection;
    DWORD index;
    LONG dirty = InterlockedCompareExchange(
        &g_generator_patch_transaction_dirty, 0, 0);
    if (g_report.generator_patch_sites_applied == 0u && dirty == 0) {
        return 1;
    }
    span_start = g_generator_patch_manifest[0].address;
    span_end = span_start + g_generator_patch_manifest[0].length;
    for (index = 0u; index < GENERATOR_PATCH_COUNT; ++index) {
        BYTE *patch_end =
            g_generator_patch_manifest[index].address +
            g_generator_patch_manifest[index].length;
        if (dirty == 0 && !bytes_are_equal(
                g_generator_patch_manifest[index].address,
                g_generator_patch_manifest[index].replacement,
                g_generator_patch_manifest[index].length)) {
            return 0;
        }
        if (g_generator_patch_manifest[index].address < span_start) {
            span_start = g_generator_patch_manifest[index].address;
        }
        if (patch_end > span_end) {
            span_end = patch_end;
        }
    }
    span = (DWORD)(span_end - span_start);
    if (!VirtualProtect(
            span_start, span, PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        return 0;
    }
    target_protection = dirty != 0 &&
            g_report.generator_old_protection != 0u ?
        g_report.generator_old_protection : old_protection;
    InterlockedExchange(&g_generator_patch_transaction_dirty, 1);
    if (!rollback_generator_patch_transaction_writable(
            g_generator_patch_manifest,
            span_start, span, target_protection)) {
        return 0;
    }
    return 1;
}

static int rollback_distance_after_downstream_failure(
    BYTE **distance_operands,
    const DWORD *distance_originals,
    BYTE *distance_span_start, DWORD distance_span,
    DWORD distance_original_protection,
    int distance_patch_attempted)
{
    DWORD index;
    DWORD observed_protection = 0u;
    DWORD target_protection;
    int bytes_ok = 1;
    int flush_ok;
    int protection_ok;
    if (!distance_patch_attempted) {
        return 1;
    }
    if (!distance_operands || !distance_originals ||
        !distance_span_start || distance_span == 0u ||
        !VirtualProtect(
            distance_span_start, distance_span,
            PAGE_EXECUTE_READWRITE,
            &observed_protection)) {
        return 0;
    }
    target_protection = distance_original_protection != 0u ?
        distance_original_protection : observed_protection;
    for (index = 0u; index < 6u; ++index) {
        write_u32(
            distance_operands[index],
            distance_originals[index]);
    }
    for (index = 0u; index < 6u; ++index) {
        if (read_u32(distance_operands[index]) !=
                distance_originals[index]) {
            bytes_ok = 0;
        }
    }
    flush_ok = flush_patch_span_retry(
        distance_span_start, distance_span);
    protection_ok = restore_patch_page_protection_retry(
        distance_span_start, distance_span,
        target_protection);
    return bytes_ok && flush_ok && protection_ok;
}

static int rollback_prior_features_after_universal_failure(
    BYTE *capacity_immediate,
    BYTE **distance_operands,
    const DWORD *distance_originals,
    BYTE *distance_span_start, DWORD distance_span,
    DWORD distance_original_protection,
    int distance_patch_attempted)
{
    int ok = 1;
    if (!rollback_generator_after_downstream_failure()) {
        ok = 0;
    }
    if (!rollback_provider_after_downstream_failure()) {
        ok = 0;
    }
    if (!rollback_distance_after_downstream_failure(
            distance_operands, distance_originals,
            distance_span_start, distance_span,
            distance_original_protection,
            distance_patch_attempted)) {
        ok = 0;
    }
    if (!rollback_capacity_after_downstream_failure(
            capacity_immediate)) {
        ok = 0;
    }
    /* The startup game thread is not yet proved quiescent here.  Even after
     * exact code-byte rollback, it may already be executing a provider or
     * generator stub, or holding an address into relocated storage.  Every
     * failure on this path terminates the process immediately, so retain all
     * prior-feature code and data until process teardown instead of creating
     * a rollback-after-entry use-after-free window. */
    return ok;
}

#ifndef GTAIVPOOL_TEST_NO_FATAL
static void release_unpublished_prior_feature_allocations(void)
{
    if (g_generator_extra_records) {
        VirtualFree(
            g_generator_extra_records, 0u, MEM_RELEASE);
        g_generator_extra_records = NULL;
    }
    g_generator_extra_record_count = 0u;
    g_generator_render_capacity =
        VANILLA_RENDERED_OBJECT_CAPACITY;
    if (g_provider_storage) {
        VirtualFree(g_provider_storage, 0u, MEM_RELEASE);
        g_provider_storage = NULL;
    }
    release_provider_rebuild_stub();
    g_provider_records = NULL;
    g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
}
#endif

static void run_patch(void)
{
    BYTE *image_base;
    PIMAGE_DOS_HEADER dos_header;
    PIMAGE_NT_HEADERS32 nt_headers;
    PIMAGE_SECTION_HEADER section;
    BYTE *match = NULL;
    BYTE *distance_init = NULL;
    BYTE *distance_update = NULL;
    BYTE *manager = NULL;
    DWORD section_index;
    DWORD requested;
    DWORD rendered_object_capacity;
    DWORD provider_capacity;
    DWORD automatic_surface_capacity = 0u;
    DWORD automatic_rendered_capacity = 0u;
    DWORD automatic_provider_capacity = 0u;
    DWORD resolved_surface_capacity;
    DWORD resolved_rendered_capacity;
    DWORD resolved_provider_capacity;
    DWORD distance_multiplier;
    float corrected_distance_multiplier;
    DWORD plant_density_multiplier;
    float corrected_plant_density_multiplier;
    float procobj_density_multiplier;
    DWORD density_class_mask;
    DWORD debug_log_enabled;
    DWORD hang_watchdog_enabled;
    DWORD hang_timeout_seconds;
    DWORD hang_message_box_enabled;
    DWORD full_hang_dump_enabled;
    DWORD current_immediate;
    DWORD manager_pointer;
    DWORD distance_old_protection = 0;
    DWORD distance_unused_protection = 0;
    DWORD distance_span = 0u;
    DWORD distance_originals[6] = {0u};
    BYTE *distance_operands[6] = {NULL};
    DWORD distance_replacements[6] = {0u};
    DWORD distance_index;
    int manager_active;
    int provider_relocation_needed = 0;
    int generator_expansion_needed = 0;
    int distance_sources_valid = 0;
    const struct GuUniversalExecutableProfile *universal_profile = NULL;
#ifndef GTAIVPOOL_TEST_NO_FATAL
    struct GuStartupQuiescence startup_guard;
    const struct GuStartupExactCallContract *startup_call_contract = NULL;
    struct GuStartupPatchSpan
        startup_spans[GU_STARTUP_MAX_PATCH_SPANS];
    DWORD startup_span_count = 0u;
    DWORD startup_index;
    DWORD startup_preimage_mismatch = 0u;
    DWORD startup_interval_mismatch = 0u;
    DWORD startup_interval_bytes = 0u;
    DWORD startup_interval_relocations = 0u;
    int startup_guard_active = 0;
    int startup_write_phase_started = 0;
    int distance_patch_attempted = 0;
    int universal_prepared = 0;
#endif

    ZeroMemory(&g_report, sizeof(g_report));
#ifndef GTAIVPOOL_TEST_NO_FATAL
    ZeroMemory(&startup_guard, sizeof(startup_guard));
    ZeroMemory(startup_spans, sizeof(startup_spans));
#endif
    /*
     * The native fixture uses the Patch 8 byte layout. Production startup
     * replaces this with an exact match from the append-only registry below.
     */
    g_build_profile = &g_build_profiles[2];
    g_executable_identity = NULL;
    g_self_sha256[0] = '\0';
    g_ini_sha256[0] = '\0';
    g_main_sha256[0] = '\0';
    ZeroMemory(g_config_error_key, sizeof(g_config_error_key));
    ZeroMemory(g_config_error_value, sizeof(g_config_error_value));
    g_config_error_kind = CONFIG_ERROR_NONE;
    g_config_error_min = 0u;
    g_config_error_max = 0u;
    g_config_error_float_range = 0u;
    InterlockedExchange(&g_compatibility_probe_prompted, 0);
    InterlockedExchange(&g_compatibility_probe_authorized, 0);
    InterlockedExchange(&g_compatibility_probe_safe_continue, 0);
    ZeroMemory(
        g_config_error_min_text, sizeof(g_config_error_min_text));
    ZeroMemory(
        g_config_error_max_text, sizeof(g_config_error_max_text));
    g_startup_error_message[0] = '\0';
    g_compatibility_probe_message[0] = '\0';
    g_report.startup_worker_started =
        InterlockedCompareExchange(
            &g_startup_worker_state, 0, 0) >= 2 ? 1u : 0u;
    if (!acquire_locked_file_identity(
            g_self_path, &g_self_file_identity)) {
        g_report.last_error = GetLastError();
        g_report.status = STATUS_ASI_IDENTITY_READ_FAILED;
        return;
    }
    g_report.asi_identity_lock_held = 1u;
    g_report.asi_hash_valid = 1u;
    g_report.asi_file_size = g_self_file_identity.file_size;
    copy_string(
        g_self_sha256, sizeof(g_self_sha256),
        g_self_file_identity.sha256);
    if (!acquire_locked_file_identity(
            g_main_path, &g_main_file_identity)) {
        g_report.last_error = GetLastError();
        g_report.status = STATUS_EXECUTABLE_HASH_READ_FAILED;
        return;
    }
    g_report.executable_identity_lock_held = 1u;
    g_report.executable_hash_valid = 1u;
    g_report.executable_file_size =
        g_main_file_identity.file_size;
    copy_string(
        g_main_sha256, sizeof(g_main_sha256),
        g_main_file_identity.sha256);
    if (!acquire_locked_file_identity(
            g_ini_path, &g_ini_file_identity)) {
        g_report.last_error = GetLastError();
        g_report.status = STATUS_INI_IDENTITY_READ_FAILED;
        return;
    }
    g_report.ini_identity_lock_held = 1u;
    g_report.ini_hash_valid = 1u;
    g_report.ini_file_size = g_ini_file_identity.file_size;
    copy_string(
        g_ini_sha256, sizeof(g_ini_sha256),
        g_ini_file_identity.sha256);
    ZeroMemory(
        g_provider_patch_manifest,
        sizeof(g_provider_patch_manifest));
    g_provider_manifest_configured = 0u;
    ZeroMemory(
        g_generator_patch_manifest,
        sizeof(g_generator_patch_manifest));
    g_generator_manifest_configured = 0u;
    g_generator_manager = NULL;
    g_generator_extra_records = NULL;
    g_generator_extra_record_count = 0u;
    g_generator_render_capacity =
        VANILLA_RENDERED_OBJECT_CAPACITY;
    g_generator_builtin_pop = NULL;
    InterlockedExchange(&g_generator_extra_issued, 0);
    InterlockedExchange(
        &g_generator_extra_exhaustion_observed, 0);
    InterlockedExchange(&g_generator_fallback_calls, 0);
    InterlockedExchange(&g_provider_rebuilds_verified, 0);
    InterlockedExchange(&g_provider_rebuild_failures, 0);
    requested = DEFAULT_CAPACITY;
    rendered_object_capacity =
        DEFAULT_RENDERED_OBJECT_CAPACITY;
    provider_capacity = DEFAULT_PROVIDER_CAPACITY;
    distance_multiplier = DEFAULT_DISTANCE_MULTIPLIER;
    corrected_distance_multiplier =
        DEFAULT_CORRECTED_DISTANCE_MULTIPLIER;
    plant_density_multiplier =
        DEFAULT_PLANT_DENSITY_MULTIPLIER;
    corrected_plant_density_multiplier =
        DEFAULT_CORRECTED_PLANT_DENSITY_MULTIPLIER;
    procobj_density_multiplier =
        DEFAULT_PROCOBJ_DENSITY_MULTIPLIER;
    density_class_mask = DEFAULT_DENSITY_CLASS_MASK;
    debug_log_enabled = DEFAULT_DEBUG_LOG;
    hang_watchdog_enabled = DEFAULT_HANG_WATCHDOG;
    hang_timeout_seconds = DEFAULT_HANG_TIMEOUT_SECONDS;
    hang_message_box_enabled = DEFAULT_HANG_MESSAGE_BOX;
    full_hang_dump_enabled = DEFAULT_FULL_HANG_DUMP;
    if (!read_ini_float_strict_default(
            "DistanceMultiplier", "1.25",
            MIN_CORRECTED_DISTANCE_MULTIPLIER,
            MAX_CORRECTED_DISTANCE_MULTIPLIER,
            &corrected_distance_multiplier) ||
        !read_ini_rebased_density(
            "PlantDensityMultiplier",
            &g_configured_plant_density_multiplier,
            &corrected_plant_density_multiplier) ||
        !read_ini_rebased_density(
            "ProceduralObjectDensityMultiplier",
            &g_configured_procobj_density_multiplier,
            &procobj_density_multiplier) ||
        !read_ini_u32_strict_default(
            "DensityClassMask", "3",
            MIN_DENSITY_CLASS_MASK,
            MAX_DENSITY_CLASS_MASK,
            &density_class_mask) ||
        !resolve_automatic_pool_profile(
            corrected_distance_multiplier,
            corrected_plant_density_multiplier,
            procobj_density_multiplier,
            &resolved_surface_capacity,
            &resolved_rendered_capacity,
            &resolved_provider_capacity) ||
        !read_ini_u32_or_auto_default(
            "Capacity", MIN_CAPACITY,
            MAX_OPERATIONAL_CAPACITY, &requested,
            &automatic_surface_capacity) ||
        !read_ini_u32_or_auto_default(
            "RenderedObjectCapacity",
            MIN_RENDERED_OBJECT_CAPACITY,
            MAX_RENDERED_OBJECT_CAPACITY,
            &rendered_object_capacity,
            &automatic_rendered_capacity) ||
        !read_ini_u32_or_auto_default(
            "ProviderCapacity", MIN_PROVIDER_CAPACITY,
            MAX_PROVIDER_CAPACITY, &provider_capacity,
            &automatic_provider_capacity) ||
        !read_ini_u32_strict(
            "DebugLog", MIN_DEBUG_LOG, MAX_DEBUG_LOG,
            &debug_log_enabled) ||
        !read_ini_u32_strict(
            "HangWatchdog", MIN_HANG_WATCHDOG,
            MAX_HANG_WATCHDOG, &hang_watchdog_enabled) ||
        !read_ini_u32_strict(
            "HangTimeoutSeconds", MIN_HANG_TIMEOUT_SECONDS,
            MAX_HANG_TIMEOUT_SECONDS, &hang_timeout_seconds) ||
        !read_ini_u32_strict_default(
            "HangMessageBox", "0", MIN_HANG_MESSAGE_BOX,
            MAX_HANG_MESSAGE_BOX, &hang_message_box_enabled) ||
        !read_ini_u32_strict_default(
            "FullHangDump", "0", MIN_FULL_HANG_DUMP,
            MAX_FULL_HANG_DUMP, &full_hang_dump_enabled)) {
        g_report.requested_capacity = requested;
        g_report.requested_rendered_object_capacity =
            rendered_object_capacity;
        g_report.effective_rendered_object_capacity =
            rendered_object_capacity;
        g_report.requested_provider_capacity =
            provider_capacity;
        g_report.distance_multiplier = distance_multiplier;
        g_report.corrected_distance_multiplier_bits =
            read_u32((BYTE *)&corrected_distance_multiplier);
        g_report.plant_density_multiplier =
            plant_density_multiplier;
        g_report.corrected_plant_density_multiplier_bits =
            read_u32((BYTE *)&corrected_plant_density_multiplier);
        g_report.procobj_density_multiplier_bits =
            read_u32((BYTE *)&procobj_density_multiplier);
        g_report.density_class_mask = density_class_mask;
        g_report.debug_log_enabled = debug_log_enabled;
        g_report.hang_watchdog_enabled = hang_watchdog_enabled;
        g_report.hang_timeout_seconds = hang_timeout_seconds;
        g_report.hang_message_box_enabled = hang_message_box_enabled;
        g_report.full_hang_dump_enabled = full_hang_dump_enabled;
        g_debug_log_enabled = debug_log_enabled;
        g_report.status = STATUS_INVALID_CONFIG;
        return;
    }
    if (automatic_surface_capacity) {
        requested = resolved_surface_capacity;
    }
    if (automatic_rendered_capacity) {
        rendered_object_capacity = resolved_rendered_capacity;
    }
    if (automatic_provider_capacity) {
        provider_capacity = resolved_provider_capacity;
    }
    if (rendered_object_capacity > GENERATOR_CAPACITY_WORD_MAX ||
        rendered_object_capacity >
            0xFFFFFFFFu / GENERATOR_STAGING_RECORD_SIZE ||
        provider_capacity > 0xFFFFu) {
        set_config_error(
            "RenderedObjectCapacity",
            "internal-width-overflow",
            CONFIG_ERROR_OUT_OF_RANGE,
            MIN_RENDERED_OBJECT_CAPACITY,
            MAX_RENDERED_OBJECT_CAPACITY);
        g_report.status = STATUS_INVALID_CONFIG;
        return;
    }
    g_report.requested_capacity = requested;
    g_report.requested_rendered_object_capacity =
        rendered_object_capacity;
    g_report.effective_rendered_object_capacity =
        rendered_object_capacity;
    g_report.requested_provider_capacity = provider_capacity;
    g_report.capacity_auto_selected = automatic_surface_capacity;
    g_report.rendered_capacity_auto_selected =
        automatic_rendered_capacity;
    g_report.provider_capacity_auto_selected =
        automatic_provider_capacity;
    g_report.distance_multiplier = distance_multiplier;
    g_report.corrected_distance_multiplier_bits =
        read_u32((BYTE *)&corrected_distance_multiplier);
    g_report.plant_density_multiplier =
        plant_density_multiplier;
    g_report.corrected_plant_density_multiplier_bits =
        read_u32((BYTE *)&corrected_plant_density_multiplier);
    g_report.procobj_density_multiplier_bits =
        read_u32((BYTE *)&procobj_density_multiplier);
    g_report.density_class_mask = density_class_mask;
    g_report.debug_log_enabled = debug_log_enabled;
    g_report.hang_watchdog_enabled = hang_watchdog_enabled;
    g_report.hang_timeout_seconds = hang_timeout_seconds;
    g_report.hang_message_box_enabled = hang_message_box_enabled;
    g_report.full_hang_dump_enabled = full_hang_dump_enabled;
    g_debug_log_enabled = debug_log_enabled;
    g_hang_watchdog_enabled = hang_watchdog_enabled;
    g_hang_timeout_seconds = hang_timeout_seconds;
    g_hang_message_box_enabled = hang_message_box_enabled;
    g_full_hang_dump_enabled = full_hang_dump_enabled;
    g_plant_density_multiplier = plant_density_multiplier;
    g_corrected_plant_density_multiplier =
        corrected_plant_density_multiplier;
    g_procobj_density_multiplier = procobj_density_multiplier;
    g_density_class_mask = density_class_mask;

    g_report.scaled_twenty = 20u;
    g_report.scaled_forty = 40u;
    g_scaled_half = 0.5f;
    g_scaled_twenty = 20.0f;
    g_scaled_forty = 40.0f;
    g_report.scaled_half_bits = read_u32((const BYTE *)&g_scaled_half);

    image_base = (BYTE *)GetModuleHandleA(NULL);
    if (!image_base) {
        g_report.status = STATUS_BAD_MAIN_MODULE;
        g_report.last_error = GetLastError();
        return;
    }
    dos_header = (PIMAGE_DOS_HEADER)image_base;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE ||
        dos_header->e_lfanew <= 0 ||
        (DWORD)dos_header->e_lfanew > 0x100000u) {
        g_report.status = STATUS_BAD_PE_IMAGE;
        return;
    }

    nt_headers = (PIMAGE_NT_HEADERS32)(image_base + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE ||
        nt_headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        g_report.status = STATUS_BAD_PE_IMAGE;
        return;
    }

    if (nt_headers->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        g_report.status = STATUS_UNSUPPORTED_MACHINE;
        return;
    }

    g_report.pe_timestamp = nt_headers->FileHeader.TimeDateStamp;
    g_report.size_of_image = nt_headers->OptionalHeader.SizeOfImage;
    g_report.image_base = (DWORD)image_base;
    g_report.loaded_header_preferred_image_base =
        nt_headers->OptionalHeader.ImageBase;
#ifndef GTAIVPOOL_TEST_NO_FATAL
    {
        const struct BuildProfile *selected_profile;
        g_report.preferred_image_base =
            read_disk_preferred_image_base();
        if (g_report.preferred_image_base == 0u) {
            g_report.startup_quiescence_failure_kind = 17u;
            g_report.status = STATUS_EXECUTABLE_IDENTITY_MISMATCH;
            return;
        }
        if (!g_report.executable_hash_valid ||
            !g_report.executable_identity_lock_held ||
            !g_main_sha256[0]) {
            g_report.status = STATUS_EXECUTABLE_HASH_READ_FAILED;
            return;
        }
        g_executable_identity = select_executable_identity(
            g_main_sha256, g_report.executable_file_size);
        if (!g_executable_identity) {
            /* Exact executable identity is mandatory. A similar profile never
             * authorizes writes to an unknown file, even after a user prompt. */
            g_report.status = STATUS_UNREGISTERED_EXECUTABLE_HASH;
            return;
        }
        g_report.executable_identity_id = g_executable_identity->id;
        selected_profile = find_build_profile_by_id(
            g_executable_identity->build_profile_id);
        if (!selected_profile ||
            selected_profile->pe_timestamp != g_report.pe_timestamp ||
            selected_profile->image_size != g_report.size_of_image) {
            g_report.status = STATUS_EXECUTABLE_IDENTITY_MISMATCH;
            return;
        }
        g_build_profile = selected_profile;
        startup_call_contract = startup_find_exact_call_contract(
            g_executable_identity->id);
        if (!startup_call_contract ||
            startup_call_contract->executable_identity_id !=
                g_executable_identity->id ||
            startup_call_contract->build_profile_id !=
                g_executable_identity->build_profile_id ||
            startup_call_contract->preferred_image_base !=
                g_report.preferred_image_base ||
            !strings_are_equal(
                startup_call_contract->specimen_sha256,
                g_executable_identity->sha256) ||
            !startup_exact_call_contract_is_structurally_valid(
                startup_call_contract,
                selected_profile->image_size) ||
            !startup_guard_interval_contains_rva_range(
                startup_call_contract,
                GU_STARTUP_GUARD_PROCEDURAL,
                selected_profile->procedural_code_first_rva,
                selected_profile->procedural_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                startup_call_contract,
                GU_STARTUP_GUARD_GENERATOR,
                selected_profile->generator_code_first_rva,
                selected_profile->generator_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                startup_call_contract,
                GU_STARTUP_GUARD_SOURCE,
                selected_profile->source_code_first_rva,
                selected_profile->source_code_last_rva) ||
            !startup_guard_interval_contains_rva_range(
                startup_call_contract,
                GU_STARTUP_GUARD_DEFINITION,
                selected_profile->definition_call_rva,
                selected_profile->definition_call_rva +
                    VERIFIED_DEFINITION_CALL_LENGTH) ||
            !startup_exact_return_contract_contains_rva(
                startup_call_contract,
                selected_profile->definition_call_rva + 10u) ||
            !startup_exact_return_contract_contains_rva(
                startup_call_contract,
                selected_profile->definition_call_rva + 15u)) {
            g_report.startup_quiescence_failure_kind = 18u;
            g_report.status = STATUS_EXECUTABLE_IDENTITY_MISMATCH;
            return;
        }
        g_report.startup_call_contract_identity_id =
            startup_call_contract->executable_identity_id;
        g_report.startup_call_contract_record_count =
            startup_call_contract->return_count;
        g_report.startup_guard_interval_count =
            startup_call_contract->interval_count;
        g_report.startup_definition_guard_first_rva =
            startup_call_contract->intervals[
                GU_STARTUP_GUARD_DEFINITION - 1u].guard_first_rva;
        g_report.startup_definition_guard_last_rva =
            g_report.startup_definition_guard_first_rva +
            startup_call_contract->intervals[
                GU_STARTUP_GUARD_DEFINITION - 1u].guard_length;
        g_report.startup_plant_setter_guard_first_rva =
            startup_call_contract->intervals[
                GU_STARTUP_GUARD_PLANT_SETTER - 1u].guard_first_rva;
        g_report.startup_plant_setter_guard_last_rva =
            g_report.startup_plant_setter_guard_first_rva +
            startup_call_contract->intervals[
                GU_STARTUP_GUARD_PLANT_SETTER - 1u].guard_length;
        universal_profile = gu_universal_profile_by_identity(
            g_executable_identity->id);
        if (!universal_profile ||
            lstrcmpA(
                universal_profile->sha256,
                g_executable_identity->sha256) != 0 ||
            !startup_guard_interval_contains_rva_range(
                startup_call_contract,
                GU_STARTUP_GUARD_PLANT_SETTER,
                universal_profile->plant_distance_setter.rva,
                universal_profile->plant_distance_setter.rva +
                    universal_profile->plant_distance_setter.length)) {
            g_report.status =
                STATUS_UNIVERSAL_BEHAVIOR_PROFILE_MISSING;
            return;
        }
        g_report.universal_behavior_profile_found = 1u;
    }
#else
    g_report.executable_hash_valid = 1u;
#endif
    g_report.build_profile_id = g_build_profile->id;
    g_report.generator_expansion_enabled_for_profile =
        generator_expansion_supported_for_profile(
            g_build_profile->id) ? 1u : 0u;
    if (!g_report.generator_expansion_enabled_for_profile) {
        g_report.status = STATUS_GENERATOR_UNSUPPORTED_BUILD;
        return;
    }
    if (provider_capacity > VANILLA_PROVIDER_CAPACITY &&
        !g_build_profile->provider_supported) {
        /*
         * The rest of this profile remains supported. Only the optional
         * provider relocation is unavailable unless every required site
         * is independently verified for this exact executable.
         */
        g_report.status = STATUS_PROVIDER_UNSUPPORTED_BUILD;
        return;
    }
#ifndef GTAIVPOOL_TEST_NO_FATAL
    if (g_report.pe_timestamp != VERIFIED_PE_TIMESTAMP ||
        g_report.size_of_image != VERIFIED_IMAGE_SIZE) {
        g_report.status = STATUS_UNSUPPORTED_BUILD_PROFILE;
        return;
    }
#endif
    /* Production selected this value from the retained exact-hash-locked
     * executable. Test fixtures without a locked disk image retain their
     * historical deterministic fallback. */
#ifdef GTAIVPOOL_TEST_NO_FATAL
    if (g_report.preferred_image_base == 0u) {
        g_report.preferred_image_base =
            VERIFIED_PREFERRED_IMAGE_BASE;
    }
#else
    if (g_report.preferred_image_base == 0u) {
        g_report.status = STATUS_EXECUTABLE_IDENTITY_MISMATCH;
        return;
    }
#endif
    g_report.image_rebase_delta =
        g_report.image_base - g_report.preferred_image_base;
    g_definition_manager = NULL;
    InterlockedExchange(&g_definition_callback_state, 0);
    InterlockedExchange(&g_definition_callback_calls, 0);
    InterlockedExchange(&g_tuning_governor_pressure_polls, 0);
    InterlockedExchange(&g_tuning_governor_triggered, 0);
    InterlockedExchange(
        &g_tuning_governor_state,
        TUNING_GOVERNOR_STATE_IDLE);
    InterlockedExchange(&g_tuning_governor_event_pending, 0);
    InterlockedExchange(&g_gu_behavior.enabled, 0);
    g_report.definition_scaling_status =
        DEFINITION_SCALING_WAITING;
    g_report.tuning_governor_active = 0u;
    g_report.tuning_governor_triggered = 0u;
    g_report.tuning_governor_reason = 0u;
    g_report.tuning_governor_live_writes = 0u;
    g_report.tuning_governor_pressure_polls = 0u;
    g_report.tuning_governor_state =
        TUNING_GOVERNOR_STATE_IDLE;
    g_report.tuning_governor_detection_thread_id = 0u;
    g_generator_manager = NULL;
    section = IMAGE_FIRST_SECTION(nt_headers);

    for (section_index = 0;
         section_index < nt_headers->FileHeader.NumberOfSections;
         ++section_index, ++section) {
        BYTE *start;
        DWORD span;
        DWORD offset;

        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
            continue;
        }
        if (section->VirtualAddress >= g_report.size_of_image) {
            continue;
        }

        span = section->Misc.VirtualSize;
        if (span == 0) {
            span = section->SizeOfRawData;
        }
        if (span > g_report.size_of_image - section->VirtualAddress) {
            span = g_report.size_of_image - section->VirtualAddress;
        }
        if (span < TARGET_PATTERN_LENGTH) {
            continue;
        }

        start = image_base + section->VirtualAddress;
        for (offset = 0; offset < span; ++offset) {
            BYTE *candidate = start + offset;
            DWORD remaining = span - offset;

            if (remaining >= TARGET_PATTERN_LENGTH &&
                target_bytes_match(candidate) &&
                wrapper_bytes_match(candidate, image_base)) {
                ++g_report.signature_matches;
                if (!match) {
                    match = candidate;
                }
            }

            if (remaining >= DISTANCE_INIT_PATTERN_LENGTH &&
                distance_init_bytes_match(candidate)) {
                ++g_report.distance_init_matches;
                if (!distance_init) {
                    distance_init = candidate;
                }
            }

            if (remaining >= DISTANCE_UPDATE_PATTERN_LENGTH &&
                distance_update_bytes_match(candidate)) {
                ++g_report.distance_update_matches;
                if (!distance_update) {
                    distance_update = candidate;
                }
            }
        }
    }

    if (g_report.signature_matches == 0) {
        g_report.status = STATUS_SIGNATURE_NOT_FOUND;
        return;
    }
    if (g_report.signature_matches != 1) {
        g_report.status = STATUS_SIGNATURE_AMBIGUOUS;
        return;
    }

#ifndef GTAIVPOOL_TEST_NO_FATAL
    if (!image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_DEFINITION_MANAGER_RVA,
            DEFINITION_MATERIAL_MAP_REQUIRED_SIZE) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_MANAGER_RVA,
            GENERATOR_MANAGER_MINIMUM_SIZE) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_POP_RVA, 1u) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_SPECIAL_CAP_RVA,
            GENERATOR_PATCH_MAX_LENGTH) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_RENDER_CAP_RVA,
            GENERATOR_PATCH_MAX_LENGTH) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_ALLOCATOR_RVA,
            GENERATOR_PATCH_MAX_LENGTH) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_STAGING_ALLOC_RVA, 5u) ||
        !image_rva_range_is_readable(
            image_base, g_report.size_of_image,
            VERIFIED_GENERATOR_STAGING_CAP_RVA, 5u) ||
        (g_build_profile->provider_supported &&
         provider_capacity > VANILLA_PROVIDER_CAPACITY &&
         (!image_rva_range_is_readable(
              image_base, g_report.size_of_image,
              PROVIDER_PATCH_FIRST_RVA,
              PROVIDER_PATCH_MAX_LENGTH) ||
          !image_rva_range_is_readable(
              image_base, g_report.size_of_image,
              PROVIDER_PATCH_LAST_RVA,
              PROVIDER_PATCH_MAX_LENGTH) ||
          !image_rva_range_is_readable(
              image_base, g_report.size_of_image,
              PROVIDER_HEAD_QUARANTINE_RVA,
              6u) ||
          !image_rva_range_is_readable(
              image_base, g_report.size_of_image,
              PROVIDER_REBUILD_EPILOGUE_RVA,
              9u) ||
          !image_rva_range_is_readable(
              image_base, g_report.size_of_image,
              PROVIDER_MANAGER_POINTER_RVA,
              MANAGER_MINIMUM_SIZE)))) {
        g_report.status = STATUS_REQUIRED_LAYOUT_OUT_OF_RANGE;
        return;
    }

    if (!definition_startup_call_matches(
            image_base, g_report.size_of_image)) {
        g_report.status = STATUS_DEFINITION_UNSUPPORTED_BUILD;
        return;
    }
    g_report.definition_code_verified = 1u;
    g_definition_manager =
        image_base + VERIFIED_DEFINITION_MANAGER_RVA;
    g_report.definition_manager_rva =
        VERIFIED_DEFINITION_MANAGER_RVA;
    if (!startup_bytes_are_zero(
            g_definition_manager,
            DEFINITION_MATERIAL_MAP_REQUIRED_SIZE) ||
        !definition_catalog_is_unloaded()) {
        g_report.status =
            STATUS_DEFINITION_LOAD_ALREADY_STARTED;
        return;
    }

    configure_generator_patches(
        image_base, rendered_object_capacity,
        g_generator_patch_manifest);
    g_generator_manifest_configured = 1u;
    if (!validate_generator_patches(
            g_generator_patch_manifest)) {
        g_report.status = STATUS_GENERATOR_CODE_MISMATCH;
        return;
    }
    g_generator_manager =
        image_base + VERIFIED_GENERATOR_MANAGER_RVA;
    g_generator_builtin_pop =
        (void *(*)(void))(
            image_base + VERIFIED_GENERATOR_POP_RVA);
    g_report.generator_manager_rva =
        VERIFIED_GENERATOR_MANAGER_RVA;
    g_report.generator_original_free_count =
        read_u32(
            g_generator_manager +
            GENERATOR_FREE_COUNT_OFFSET);
    g_report.generator_original_active_rendered =
        read_u32(
            g_generator_manager +
            GENERATOR_ACTIVE_RENDERED_COUNT_OFFSET);
    g_report.generator_original_staging_capacity =
        read_u32(
            g_generator_manager +
            GENERATOR_STAGING_CAPACITY_OFFSET);
    g_report.generator_original_staging_count =
        (DWORD)read_u16(
            g_generator_manager +
            GENERATOR_STAGING_COUNT_OFFSET);
    if (!startup_bytes_are_zero(
            g_generator_manager,
            GENERATOR_MANAGER_MINIMUM_SIZE) ||
        (rendered_object_capacity >
             VANILLA_RENDERED_OBJECT_CAPACITY &&
         (g_report.generator_original_staging_capacity != 0u ||
          g_report.generator_original_staging_count != 0u ||
          read_u32(
              g_generator_manager +
              GENERATOR_STAGING_BUFFER_OFFSET) != 0u ||
          read_u16(
              g_generator_manager +
              GENERATOR_STAGING_ALLOCATED_COUNT_OFFSET) != 0u))) {
        g_report.status =
            STATUS_GENERATOR_INITIALIZED_BEFORE_PATCH;
        return;
    }
#endif

    if (g_report.distance_init_matches == 1u &&
        g_report.distance_update_matches == 1u) {
        distance_sources_valid = distance_sources_are_valid(
            image_base, g_report.size_of_image,
            distance_init, distance_update);
        if (distance_sources_valid) {
            g_report.distance_init_rva =
                (DWORD)(distance_init - image_base);
            g_report.distance_update_rva =
                (DWORD)(distance_update - image_base);
            g_report.detail_distance_source = read_u32(
                distance_init + DIST_INIT_BASE_POINTER_OFFSET);
            g_report.detail_multiplier_source = read_u32(
                distance_init + DIST_INIT_DETAIL_POINTER_OFFSET);
            g_report.distance_near_source = read_u32(
                distance_init + DIST_INIT_NEAR_POINTER_OFFSET);
            g_report.distance_far_source = read_u32(
                distance_init + DIST_INIT_FAR_POINTER_OFFSET);
        }
    }

    if (LEGACY_DISTANCE_OPERAND_PATCH_ENABLED &&
        distance_multiplier != 1u) {
        if (g_report.distance_init_matches == 0) {
            g_report.status = STATUS_DISTANCE_INIT_SIGNATURE_NOT_FOUND;
            return;
        }
        if (g_report.distance_init_matches != 1) {
            g_report.status = STATUS_DISTANCE_INIT_SIGNATURE_AMBIGUOUS;
            return;
        }
        if (g_report.distance_update_matches == 0) {
            g_report.status = STATUS_DISTANCE_UPDATE_SIGNATURE_NOT_FOUND;
            return;
        }
        if (g_report.distance_update_matches != 1) {
            g_report.status = STATUS_DISTANCE_UPDATE_SIGNATURE_AMBIGUOUS;
            return;
        }
        if (!distance_sources_valid) {
            g_report.status = STATUS_DISTANCE_SOURCE_VALIDATION_FAILED;
            return;
        }
    }

    g_report.target_rva = (DWORD)(match - image_base);
    current_immediate = read_u32(match + TARGET_IMMEDIATE_OFFSET);
    g_report.original_immediate = current_immediate;
    g_report.final_immediate = current_immediate;

#ifdef GTAIVPOOL_TEST_NO_FATAL
    manager_pointer = read_u32(
        match - TARGET_WRAPPER_DISTANCE + 1u);
#else
    manager_pointer = (DWORD)(image_base + VERIFIED_MANAGER_RVA);
#endif
    if (manager_pointer < (DWORD)image_base ||
        manager_pointer > (DWORD)image_base + g_report.size_of_image - MANAGER_MINIMUM_SIZE) {
        g_report.status = STATUS_BAD_MANAGER_REFERENCE;
        return;
    }

    manager = (BYTE *)manager_pointer;
    g_manager = manager;
    g_report.manager_rva = (DWORD)(manager - image_base);
    g_report.manager_capacity = read_u32(manager + MANAGER_CAPACITY_OFFSET);
    g_report.manager_buffer0 = read_u32(manager + MANAGER_BUFFER0_OFFSET);
    g_report.manager_buffer1 = read_u32(manager + MANAGER_BUFFER1_OFFSET);
    g_report.manager_buffer2 = read_u32(manager + MANAGER_BUFFER2_OFFSET);
    g_report.provider_free_head_at_load =
        (DWORD)read_u16(
            manager + MANAGER_SOURCE_FREE_HEAD_OFFSET);
    g_report.provider_active_head_at_load =
        (DWORD)read_u16(
            manager + MANAGER_SOURCE_ACTIVE_HEAD_OFFSET);

    manager_active = (g_report.manager_buffer0 != 0 ||
                      g_report.manager_buffer1 != 0 ||
                      g_report.manager_buffer2 != 0);

    if (manager_active) {
        if (current_immediate == requested &&
            g_report.manager_capacity == requested) {
            g_report.status = STATUS_ALREADY_ACTIVE;
        } else {
            g_report.status = STATUS_TOO_LATE;
        }
        return;
    }
#ifndef GTAIVPOOL_TEST_NO_FATAL
    if (!startup_bytes_are_zero(
            manager, MANAGER_MINIMUM_SIZE)) {
        g_report.status = STATUS_TOO_LATE;
        return;
    }
#endif

#ifdef GTAIVPOOL_TEST_NO_FATAL
    /*
     * The native fixture is a small synthetic PE rather than the supplied
     * GTAIV.exe. It exercises the relocation encoder through a dedicated
     * test-only export below; runtime telemetry falls back to its embedded
     * 40-record fixture here.
     */
    g_provider_storage = NULL;
    g_provider_records =
        manager + MANAGER_SOURCE_RECORDS_OFFSET;
    g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
#else
    if (provider_capacity == VANILLA_PROVIDER_CAPACITY) {
        g_provider_storage = NULL;
        g_provider_records =
            manager + MANAGER_SOURCE_RECORDS_OFFSET;
        g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
    } else {
        if (g_report.manager_rva != VERIFIED_MANAGER_RVA) {
            g_report.status =
                STATUS_PROVIDER_UNSUPPORTED_BUILD;
            return;
        }
        if (!initialize_provider_storage(provider_capacity)) {
            g_report.status = g_provider_storage ?
                STATUS_PROVIDER_INITIALIZATION_VERIFY_FAILED :
                STATUS_PROVIDER_ALLOCATION_FAILED;
            g_report.last_error = GetLastError();
            return;
        }
        if (!initialize_provider_rebuild_stub()) {
            VirtualFree(g_provider_storage, 0u, MEM_RELEASE);
            g_provider_storage = NULL;
            g_provider_records = NULL;
            g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
            g_report.status = STATUS_PROVIDER_ALLOCATION_FAILED;
            g_report.last_error = GetLastError();
            return;
        }
        configure_provider_patches(
            image_base, g_provider_patch_manifest);
        g_provider_manifest_configured = 1u;
        if (!validate_provider_patches(
                g_provider_patch_manifest)) {
            VirtualFree(g_provider_storage, 0, MEM_RELEASE);
            release_provider_rebuild_stub();
            g_provider_storage = NULL;
            g_provider_records = NULL;
            g_provider_capacity = VANILLA_PROVIDER_CAPACITY;
            g_report.status =
                STATUS_PROVIDER_CODE_MISMATCH;
            return;
        }
        provider_relocation_needed = 1;
    }
#endif

#ifndef GTAIVPOOL_TEST_NO_FATAL
    if (rendered_object_capacity >
        VANILLA_RENDERED_OBJECT_CAPACITY) {
        if (g_report.generator_manager_rva !=
                VERIFIED_GENERATOR_MANAGER_RVA) {
            g_report.status =
                STATUS_GENERATOR_UNSUPPORTED_BUILD;
            return;
        }
        if (!initialize_generator_storage(
                rendered_object_capacity)) {
            g_report.status =
                STATUS_GENERATOR_ALLOCATION_FAILED;
            g_report.last_error = GetLastError();
            return;
        }
        generator_expansion_needed = 1;
    }
#endif

    if (requested == VANILLA_CAPACITY) {
        if (current_immediate != VANILLA_CAPACITY) {
            g_report.status = STATUS_UNEXPECTED_CAPACITY;
            return;
        }
    } else {
        if (current_immediate == requested) {
            g_report.status = STATUS_CODE_ALREADY_PATCHED;
            return;
        }

        if (current_immediate != VANILLA_CAPACITY) {
            g_report.status = STATUS_UNEXPECTED_CAPACITY;
            return;
        }
    }

    if (LEGACY_DISTANCE_OPERAND_PATCH_ENABLED &&
        distance_multiplier != 1u) {
        distance_operands[0] =
            distance_init + DIST_INIT_DETAIL_POINTER_OFFSET;
        distance_operands[1] =
            distance_init + DIST_INIT_NEAR_POINTER_OFFSET;
        distance_operands[2] =
            distance_init + DIST_INIT_FAR_POINTER_OFFSET;
        distance_operands[3] =
            distance_update + DIST_UPDATE_DETAIL_POINTER_OFFSET;
        distance_operands[4] =
            distance_update + DIST_UPDATE_NEAR_POINTER_OFFSET;
        distance_operands[5] =
            distance_update + DIST_UPDATE_FAR_POINTER_OFFSET;

        distance_replacements[0] = (DWORD)&g_scaled_half;
        distance_replacements[1] = (DWORD)&g_scaled_twenty;
        distance_replacements[2] = (DWORD)&g_scaled_forty;
        distance_replacements[3] = (DWORD)&g_scaled_half;
        distance_replacements[4] = (DWORD)&g_scaled_twenty;
        distance_replacements[5] = (DWORD)&g_scaled_forty;

        for (distance_index = 0; distance_index < 6u;
             ++distance_index) {
            distance_originals[distance_index] =
                read_u32(distance_operands[distance_index]);
        }

        distance_span = (DWORD)(
            distance_update + DISTANCE_UPDATE_PATTERN_LENGTH -
            distance_init);
    }

    g_report.compatibility_checks_passed = 1u;

#ifdef GTAIVPOOL_TEST_NO_FATAL
    if (requested == VANILLA_CAPACITY) {
        g_report.final_immediate = VANILLA_CAPACITY;
    } else if (!ensure_startup_identities_before_image_write() ||
               !apply_capacity_patch_transaction(
                   match + TARGET_IMMEDIATE_OFFSET, requested)) {
        if (InterlockedCompareExchange(
                &g_capacity_patch_transaction_dirty, 0, 0) != 0 &&
            !rollback_capacity_after_downstream_failure(
                match + TARGET_IMMEDIATE_OFFSET)) {
            g_report.status = STATUS_CAPACITY_ROLLBACK_FAILED;
        }
        return;
    }
    g_report.status = STATUS_PATCH_APPLIED;
    return;
#else
    if (!gu_universal_prepare(
            image_base, universal_profile,
            corrected_distance_multiplier,
            corrected_plant_density_multiplier,
            plant_visible_density_selector,
            &g_corrected_plant_density_multiplier,
            gu_manager_prebuild_hook,
            definition_loaded_engine_callback)) {
        g_report.universal_behavior_failure_stage =
            (DWORD)InterlockedCompareExchange(
                &g_gu_install.failure_stage, 0, 0);
        g_report.status =
            InterlockedCompareExchange(
                &g_gu_install_lifecycle_state, 0, 0) ==
                    GU_INSTALL_LIFECYCLE_FAILED_DIRTY ?
                STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED :
                STATUS_UNIVERSAL_BEHAVIOR_INSTALL_FAILED;
        release_unpublished_prior_feature_allocations();
        return;
    }
    universal_prepared = 1;

    if (requested != VANILLA_CAPACITY &&
        !startup_patch_span_add(
            startup_spans, &startup_span_count,
            match + TARGET_IMMEDIATE_OFFSET, 4u,
            (const BYTE *)(const void *)&current_immediate)) {
        g_report.startup_quiescence_failure_kind = 14u;
        g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }
    if (LEGACY_DISTANCE_OPERAND_PATCH_ENABLED &&
        distance_multiplier != 1u) {
        for (startup_index = 0u; startup_index < 6u;
             ++startup_index) {
            if (!startup_patch_span_add(
                    startup_spans, &startup_span_count,
                    distance_operands[startup_index], 4u,
                    (const BYTE *)(const void *)&
                        distance_originals[startup_index])) {
                g_report.startup_quiescence_failure_kind = 14u;
                g_report.status =
                    STATUS_STARTUP_QUIESCENCE_FAILED;
                goto startup_prewrite_failure;
            }
        }
    }
    if (provider_relocation_needed) {
        for (startup_index = 0u;
             startup_index < PROVIDER_PATCH_COUNT;
             ++startup_index) {
            if (!startup_patch_span_add(
                    startup_spans, &startup_span_count,
                    g_provider_patch_manifest[startup_index].address,
                    g_provider_patch_manifest[startup_index].length,
                    g_provider_patch_manifest[startup_index].original)) {
                g_report.startup_quiescence_failure_kind = 14u;
                g_report.status =
                    STATUS_STARTUP_QUIESCENCE_FAILED;
                goto startup_prewrite_failure;
            }
        }
    }
    if (generator_expansion_needed) {
        for (startup_index = 0u;
             startup_index < GENERATOR_PATCH_COUNT;
             ++startup_index) {
            if (!startup_patch_span_add(
                    startup_spans, &startup_span_count,
                    g_generator_patch_manifest[startup_index].address,
                    g_generator_patch_manifest[startup_index].length,
                    g_generator_patch_manifest[startup_index].original)) {
                g_report.startup_quiescence_failure_kind = 14u;
                g_report.status =
                    STATUS_STARTUP_QUIESCENCE_FAILED;
                goto startup_prewrite_failure;
            }
        }
    }
    if (g_gu_install.transaction.count == 0u) {
        g_report.startup_quiescence_failure_kind = 14u;
        g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }
    for (startup_index = 0u;
         startup_index < g_gu_install.transaction.count;
         ++startup_index) {
        const struct GuInstalledPatch *patch =
            &g_gu_install.transaction.patches[startup_index];
        if (!startup_patch_span_add(
                startup_spans, &startup_span_count,
                patch->address, patch->length,
                patch->original)) {
            g_report.startup_quiescence_failure_kind = 14u;
            g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
            goto startup_prewrite_failure;
        }
    }
    if (!startup_patch_spans_are_disjoint_in_image(
            startup_spans, startup_span_count,
            image_base, g_report.size_of_image)) {
        g_report.startup_quiescence_failure_kind = 14u;
        g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }

    /* This is the final disk identity check. All allocations, module pins,
     * loader lookups, and patch-byte preparation are already complete. */
    if (!ensure_startup_identities_before_image_write()) {
        goto startup_prewrite_failure;
    }
    if (!definition_catalog_is_unloaded()) {
        g_report.status = STATUS_DEFINITION_LOAD_ALREADY_STARTED;
        goto startup_prewrite_failure;
    }
    if (!startup_quiescence_begin(
            &startup_guard, image_base, g_build_profile,
            startup_call_contract,
            startup_spans, startup_span_count)) {
        g_report.status =
            g_report.startup_quiescence_failure_kind >= 10u &&
            g_report.startup_quiescence_failure_kind <= 13u ?
                STATUS_STARTUP_QUIESCENCE_IN_FLIGHT :
                STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }
    startup_guard_active = 1;

    /* Re-read every authoritative preimage only after all discoverable game
     * threads are stopped. This closes validation-to-write races with other
     * loaded modules without invoking any loader, heap, or file API. */
    if (!startup_patch_spans_match_preimages(
            startup_spans, startup_span_count,
            &startup_preimage_mismatch)) {
        g_report.startup_quiescence_failure_kind = 15u;
        g_report.startup_quiescence_unsafe_eip =
            startup_preimage_mismatch;
        g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }
    if (!startup_bytes_are_zero(
            manager, MANAGER_MINIMUM_SIZE) ||
        !startup_bytes_are_zero(
            g_definition_manager,
            DEFINITION_MATERIAL_MAP_REQUIRED_SIZE) ||
        !startup_bytes_are_zero(
            g_generator_manager,
            GENERATOR_MANAGER_MINIMUM_SIZE) ||
        !definition_catalog_is_unloaded()) {
        g_report.startup_quiescence_failure_kind = 16u;
        g_report.status = STATUS_STARTUP_QUIESCENCE_IN_FLIGHT;
        goto startup_prewrite_failure;
    }
    /* Seal the complete exact-target code authority a second time at the
     * narrowest possible point before the first write.  Peer threads remain
     * suspended, so a mismatch is a fail-closed external-write/co-mod signal,
     * never a partial-install condition. */
    if (!startup_guard_intervals_match_preimages(
            startup_call_contract, image_base,
            g_build_profile->image_size,
            &startup_interval_mismatch,
            &startup_interval_bytes,
            &startup_interval_relocations) ||
        startup_interval_bytes !=
            g_report.startup_guard_interval_bytes_verified ||
        startup_interval_relocations !=
            g_report.startup_guard_interval_relocations_verified) {
        g_report.startup_call_contract_mismatch_address =
            startup_interval_mismatch;
        g_report.startup_quiescence_unsafe_eip =
            startup_interval_mismatch;
        g_report.startup_quiescence_failure_kind = 19u;
        g_report.status = STATUS_STARTUP_QUIESCENCE_FAILED;
        goto startup_prewrite_failure;
    }
    g_report.startup_guard_interval_validation_passes = 2u;
    if (InterlockedCompareExchange(
            &g_definition_callback_state,
            DEFINITION_CALLBACK_ARMED,
            DEFINITION_CALLBACK_UNARMED) !=
            DEFINITION_CALLBACK_UNARMED) {
        g_report.definition_callback_state = (DWORD)
            InterlockedCompareExchange(
                &g_definition_callback_state, 0, 0);
        g_report.definition_scaling_status =
            DEFINITION_SCALING_CALLBACK_BEFORE_ACTIVATION;
        g_report.startup_quiescence_failure_kind = 17u;
        g_report.status =
            STATUS_DEFINITION_CALLBACK_BEFORE_ACTIVATION;
        goto startup_prewrite_failure;
    }
    g_report.definition_callback_state =
        DEFINITION_CALLBACK_ARMED;

    if (requested == VANILLA_CAPACITY) {
        g_report.final_immediate = VANILLA_CAPACITY;
    } else {
        startup_write_phase_started = 1;
        if (!apply_capacity_patch_transaction(
                match + TARGET_IMMEDIATE_OFFSET, requested)) {
            goto startup_write_failure;
        }
    }

    if (LEGACY_DISTANCE_OPERAND_PATCH_ENABLED &&
        distance_multiplier != 1u) {
        startup_write_phase_started = 1;
        if (!VirtualProtect(
                distance_init, distance_span,
                PAGE_EXECUTE_READWRITE,
                &distance_old_protection)) {
            g_report.last_error = GetLastError();
            g_report.status =
                STATUS_DISTANCE_VIRTUAL_PROTECT_FAILED;
            goto startup_write_failure;
        }
        g_report.distance_old_protection =
            distance_old_protection;
        distance_patch_attempted = 1;

        for (distance_index = 0u; distance_index < 6u;
             ++distance_index) {
            write_u32(
                distance_operands[distance_index],
                distance_replacements[distance_index]);
        }
        for (distance_index = 0u; distance_index < 6u;
             ++distance_index) {
            if (read_u32(distance_operands[distance_index]) !=
                    distance_replacements[distance_index]) {
                g_report.status =
                    STATUS_DISTANCE_WRITE_VERIFY_FAILED;
                goto startup_write_failure;
            }
        }
        g_report.distance_flush_result =
            (DWORD)FlushInstructionCache(
                GetCurrentProcess(), distance_init,
                distance_span);
        if (!g_report.distance_flush_result) {
            g_report.last_error = GetLastError();
            g_report.status =
                STATUS_DISTANCE_FLUSH_FAILED_REVERTED;
            goto startup_write_failure;
        }
        g_report.distance_protection_restore_result =
            (DWORD)VirtualProtect(
                distance_init, distance_span,
                distance_old_protection,
                &distance_unused_protection);
        if (!g_report.distance_protection_restore_result) {
            g_report.last_error = GetLastError();
            g_report.status =
                STATUS_DISTANCE_PROTECTION_RESTORE_FAILED;
            goto startup_write_failure;
        }
    }

    if (provider_relocation_needed) {
        startup_write_phase_started = 1;
        if (!apply_provider_patches(
                g_provider_patch_manifest)) {
            goto startup_write_failure;
        }
    }
    if (generator_expansion_needed) {
        startup_write_phase_started = 1;
        if (!apply_generator_patches(
                g_generator_patch_manifest)) {
            goto startup_write_failure;
        }
    }

    startup_write_phase_started = 1;
    if (!gu_universal_commit_prepared()) {
        g_report.universal_behavior_failure_stage =
            (DWORD)InterlockedCompareExchange(
                &g_gu_install.failure_stage, 0, 0);
        g_report.status =
            STATUS_UNIVERSAL_BEHAVIOR_INSTALL_FAILED;
        universal_prepared = 0;
        goto startup_write_failure;
    }
    universal_prepared = 0;
    g_report.universal_behavior_installed = 1u;
    g_report.universal_behavior_failure_stage =
        (DWORD)InterlockedCompareExchange(
            &g_gu_install.failure_stage, 0, 0);
    if (!startup_quiescence_end(&startup_guard)) {
        return;
    }
    startup_guard_active = 0;
    g_report.status = STATUS_PATCH_APPLIED;
    return;

startup_write_failure:
    {
        DWORD failure_status = g_report.status;
        int universal_restored =
            !gu_patch_transaction_has_unrestored(
                &g_gu_install.transaction);
        int prior_restored =
            rollback_prior_features_after_universal_failure(
                match + TARGET_IMMEDIATE_OFFSET,
                distance_operands, distance_originals,
                distance_init, distance_span,
                distance_old_protection,
                distance_patch_attempted);
        InterlockedExchange(
            &g_definition_callback_state,
            DEFINITION_CALLBACK_FAILED);
        g_report.definition_callback_state =
            DEFINITION_CALLBACK_FAILED;
        g_report.universal_behavior_rollback_verified =
            universal_restored && prior_restored;
        if (!universal_restored || !prior_restored) {
            startup_quiescence_terminate_now(
                STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED);
        }
        if (startup_guard_active &&
            !startup_quiescence_end(&startup_guard)) {
            return;
        }
        startup_guard_active = 0;
        if (universal_prepared &&
            InterlockedCompareExchange(
                &g_gu_install_lifecycle_state, 0, 0) ==
                    GU_INSTALL_LIFECYCLE_PREPARING) {
            if (!gu_universal_cancel_prepared()) {
                g_report.status =
                    STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED;
                return;
            }
            universal_prepared = 0;
        }
        g_report.status = failure_status;
        return;
    }

startup_prewrite_failure:
    if (startup_guard_active) {
        if (!startup_quiescence_end(&startup_guard)) {
            return;
        }
        startup_guard_active = 0;
    }
    if (universal_prepared &&
        InterlockedCompareExchange(
            &g_gu_install_lifecycle_state, 0, 0) ==
                GU_INSTALL_LIFECYCLE_PREPARING) {
        g_report.universal_behavior_failure_stage =
            (DWORD)InterlockedCompareExchange(
                &g_gu_install.failure_stage, 0, 0);
        if (!gu_universal_cancel_prepared()) {
            g_report.status =
                STATUS_UNIVERSAL_BEHAVIOR_ROLLBACK_FAILED;
        }
        universal_prepared = 0;
    }
    if (!startup_write_phase_started) {
        release_unpublished_prior_feature_allocations();
    }
    return;
#endif
}

const char *__cdecl GTAIVEFLCProceduralFixes_GetVersion(void)
{
    return PLUGIN_VERSION;
}

__declspec(dllexport) DWORD __cdecl
GTAIVEFLCProceduralFixes_GetLastStatus(void)
{
    return g_report.status;
}

const char *__cdecl
GTAIVEFLCProceduralFixes_GetStartupErrorMessage(void)
{
    return build_startup_error_message();
}

#ifdef GTAIVPOOL_TEST_NO_FATAL
static DWORD build_profile_hash_byte(DWORD hash, BYTE value)
{
    return (hash ^ (DWORD)value) * 16777619u;
}

static DWORD build_profile_hash_u32(DWORD hash, DWORD value)
{
    hash = build_profile_hash_byte(hash, (BYTE)(value & 0xFFu));
    hash = build_profile_hash_byte(hash, (BYTE)((value >> 8) & 0xFFu));
    hash = build_profile_hash_byte(hash, (BYTE)((value >> 16) & 0xFFu));
    return build_profile_hash_byte(
        hash, (BYTE)((value >> 24) & 0xFFu));
}

static DWORD build_profile_contract_hash(
    const struct BuildProfile *profile)
{
    DWORD hash = 2166136261u;
    DWORD index;
    const BYTE *text = (const BYTE *)profile->name;

#define HASH_PROFILE_DWORD(field) \
    hash = build_profile_hash_u32(hash, profile->field)
    while (*text) {
        hash = build_profile_hash_byte(hash, *text++);
    }
    hash = build_profile_hash_byte(hash, 0u);
    HASH_PROFILE_DWORD(id);
    HASH_PROFILE_DWORD(pe_timestamp);
    HASH_PROFILE_DWORD(image_size);
    HASH_PROFILE_DWORD(code_variant);
    HASH_PROFILE_DWORD(target_rva);
    HASH_PROFILE_DWORD(target_pattern_length);
    HASH_PROFILE_DWORD(target_immediate_offset);
    HASH_PROFILE_DWORD(target_wrapper_distance);
    HASH_PROFILE_DWORD(distance_init_pattern_length);
    HASH_PROFILE_DWORD(distance_update_pattern_length);
    for (index = 0u; index < 4u; ++index) {
        hash = build_profile_hash_u32(
            hash, profile->distance_init_offsets[index]);
    }
    HASH_PROFILE_DWORD(manager_rva);
    HASH_PROFILE_DWORD(definition_manager_rva);
    HASH_PROFILE_DWORD(model_info_table_rva);
    HASH_PROFILE_DWORD(definition_call_rva);
    for (index = 0u;
         index < sizeof(profile->definition_call_tail);
         ++index) {
        hash = build_profile_hash_byte(
            hash, profile->definition_call_tail[index]);
    }
    HASH_PROFILE_DWORD(generator_manager_rva);
    HASH_PROFILE_DWORD(generator_pop_rva);
    HASH_PROFILE_DWORD(generator_allocator_rva);
    HASH_PROFILE_DWORD(generator_special_cap_rva);
    HASH_PROFILE_DWORD(generator_render_cap_rva);
    HASH_PROFILE_DWORD(generator_staging_alloc_rva);
    HASH_PROFILE_DWORD(generator_staging_cap_rva);
    for (index = 0u;
         index < sizeof(profile->generator_allocator_original);
         ++index) {
        hash = build_profile_hash_byte(
            hash, profile->generator_allocator_original[index]);
    }
    HASH_PROFILE_DWORD(provider_supported);
    HASH_PROFILE_DWORD(provider_patch_variant);
    HASH_PROFILE_DWORD(provider_patch_count);
    HASH_PROFILE_DWORD(provider_manager_rva);
    HASH_PROFILE_DWORD(provider_next_index_rva);
    HASH_PROFILE_DWORD(provider_record_index_rva);
    for (index = 0u;
         index < profile->provider_patch_count;
         ++index) {
        hash = build_profile_hash_u32(
            hash, profile->provider_sites[index]);
    }
    HASH_PROFILE_DWORD(provider_head_quarantine_rva);
    HASH_PROFILE_DWORD(provider_rebuild_epilogue_rva);
    HASH_PROFILE_DWORD(procedural_code_first_rva);
    HASH_PROFILE_DWORD(procedural_code_last_rva);
    HASH_PROFILE_DWORD(generator_code_first_rva);
    HASH_PROFILE_DWORD(generator_code_last_rva);
    HASH_PROFILE_DWORD(source_code_first_rva);
    HASH_PROFILE_DWORD(source_code_last_rva);
#undef HASH_PROFILE_DWORD
    return hash;
}

DWORD __cdecl
GTAIVEFLCProceduralFixes_GetBuildProfileCount(void)
{
    return g_build_profile_count;
}

DWORD __cdecl
GTAIVEFLCProceduralFixes_GetBuildProfileContractHash(DWORD index)
{
    if (index >= g_build_profile_count) {
        return 0u;
    }
    return build_profile_contract_hash(&g_build_profiles[index]);
}


#endif /* GTAIVPOOL_TEST_NO_FATAL */

static DWORD WINAPI startup_worker_main(LPVOID parameter)
{
    HMODULE pinned_module = NULL;
    DWORD pin_error = NO_ERROR;

    (void)parameter;
    InterlockedExchange(&g_startup_worker_state, 2);
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(const void *)startup_worker_main,
            &pinned_module)) {
        pin_error = GetLastError();
        capture_module_paths();
        ZeroMemory(&g_report, sizeof(g_report));
        g_report.startup_worker_started = 1u;
        g_report.status = STATUS_STARTUP_WORKER_PIN_FAILED;
        g_report.last_error = pin_error;
        InterlockedExchange(&g_compatibility_probe_safe_continue, 1);
        g_report.compatibility_probe_safe_continue = 1u;
        write_log();
        InterlockedExchange(&g_startup_worker_state, 3);
#ifndef GTAIVPOOL_TEST_NO_FATAL
        show_disabled_result();
#endif
        return g_report.status;
    }

    capture_module_paths();
    /* Install before optional diagnostic module loading: no crash/hang
     * writer or background thread is active until the transaction succeeds. */
    run_patch();
    /* Any unrecoverable partial-write, rollback, or thread-resume failure
     * terminates synchronously inside run_patch. Every non-success that
     * returns here is therefore inert and may safely leave only this mod
     * disabled while GTA IV continues. */
    if (g_report.status != STATUS_PATCH_APPLIED ||
        InterlockedCompareExchange(
            &g_compatibility_probe_prompted, 0, 0) != 0) {
        InterlockedExchange(&g_compatibility_probe_safe_continue, 1);
        g_report.compatibility_probe_safe_continue = 1u;
    }
    if (g_report.status != STATUS_PATCH_APPLIED) {
        close_locked_file_identity(&g_self_file_identity);
        close_locked_file_identity(&g_ini_file_identity);
        close_locked_file_identity(&g_main_file_identity);
        g_report.asi_identity_lock_held = 0u;
        g_report.ini_identity_lock_held = 0u;
        g_report.executable_identity_lock_held = 0u;
    }
    if (g_report.status == STATUS_PATCH_APPLIED) {
        (void)initialize_minidump_writer();
        install_crash_dump_handler();
        install_targeted_exception_logger();
        start_hang_watchdog();
        write_log();
        /*
         * Both background entries wait on this interlocked publication. The
         * complete initial log, including truthful thread-start fields, is on
         * disk before either entry can touch the shared runtime log buffer.
         */
        InterlockedExchange(&g_background_threads_ready, 1);
    } else {
        write_log();
    }
    /*
     * Patch selection, transaction completion/refusal, and the durable log are
     * complete before the optional informational result box is shown.  The
     * game and status consumers must never wait for that box to be dismissed.
     */
    InterlockedExchange(&g_startup_worker_state, 3);
#ifndef GTAIVPOOL_TEST_NO_FATAL
    show_compatibility_probe_result();
    show_disabled_result();
#endif
    return g_report.status;
}

/* Ultimate ASI Loader invokes this optional export after loading the ASI.
 * Keep installation on the existing worker while holding the calling game
 * thread here until the transaction has completed. This removes the loader-
 * return race without running patch work or waiting inside DllMain.
 * A loader may itself call from a DLL notification; defer in that case.
 * An absent native callout query also falls back to the existing worker.
 * The wait is bounded and never changes any identity/state/rollback gate. */
__declspec(dllexport) void __cdecl InitializeASI(void)
{
    HMODULE ntdll;
    GuRtlIsThreadWithinLoaderCalloutFn in_loader_callout;
    DWORD started;
    LONG state;

    InterlockedIncrement(&g_startup_loader_callback_calls);
    ntdll = GetModuleHandleA("ntdll.dll");
    in_loader_callout = ntdll ?
        (GuRtlIsThreadWithinLoaderCalloutFn)GetProcAddress(
            ntdll, "RtlIsThreadWithinLoaderCallout") : NULL;
    if (!in_loader_callout || in_loader_callout()) {
        InterlockedIncrement(&g_startup_loader_callback_deferred);
        return;
    }
    started = GetTickCount();
    for (;;) {
        state = InterlockedCompareExchange(&g_startup_worker_state, 0, 0);
        if (state == 0 || state == 3) {
            return;
        }
        if ((DWORD)(GetTickCount() - started) >= GU_STARTUP_LOADER_WAIT_MS) {
            InterlockedIncrement(&g_startup_loader_callback_timeouts);
            return;
        }
        Sleep(1u);
    }
}

__declspec(dllexport) DWORD __cdecl
GTAIVEFLCProceduralFixes_GetStartupWorkerState(void)
{
    return (DWORD)InterlockedCompareExchange(
        &g_startup_worker_state, 0, 0);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE startup_thread;

        g_self_module = (HMODULE)instance;
        InterlockedExchange(&g_background_threads_ready, 0);
        InterlockedExchange(&g_startup_worker_state, 1);
        startup_thread = CreateThread(
            NULL, 0u, startup_worker_main, NULL, 0u, NULL);
        if (!startup_thread) {
            InterlockedExchange(&g_startup_worker_state, 0);
            /* Failure to start this optional mod must not make LoadLibrary
             * fail and block GTA IV itself. The ASI remains inert. */
            return TRUE;
        }
        CloseHandle(startup_thread);
    } else if (reason == DLL_THREAD_ATTACH) {
        /* A newly created ordinary Win32 thread cannot pass this module's
         * loader notification while the startup transaction owns the gate.
         * The stopped-world writer therefore performs no loader operation
         * and guarantees gate release on every recoverable exit. */
        if (InterlockedCompareExchange(
                &g_startup_patch_thread_attach_gate, 0, 0) != 0) {
            InterlockedIncrement(
                &g_startup_thread_attach_gate_waits);
            while (InterlockedCompareExchange(
                    &g_startup_patch_thread_attach_gate,
                    0, 0) != 0) {
                SwitchToThread();
            }
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        /*
         * DllMain runs under the loader lock. Never wait for workers,
         * unhook code, restore pages, close identity files, or remove
         * exception handlers here. The worker pins the module before any
         * patch and normal process teardown reclaims all remaining handles.
         */
        InterlockedExchange(&g_telemetry_stop, 1);
        InterlockedExchange(&g_hang_watchdog_stop, 1);
        InterlockedExchange(&g_background_threads_ready, 1);
        (void)reserved;
    }
    return TRUE;
}
