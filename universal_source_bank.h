#ifndef GTAIV_GRASS_UNIVERSAL_SOURCE_BANK_H
#define GTAIV_GRASS_UNIVERSAL_SOURCE_BANK_H

#include <windows.h>
#include "universal_hooks.h"

#define GU_SOURCE_BANK_COUNT 4u
#define GU_SOURCE_BANK_CAPACITY 8192u
#define GU_SOURCE_BANK_STOCK_CAPACITY 128u
#define GU_SOURCE_BANK_MAX_RADIUS_SITES 3u
#define GU_SOURCE_BANK_MAX_PROOF_SITES 8u

/*
 * Preparation is deliberately more specific than the outer universal-install
 * failure stage.  The outer stage remains stable for callers, while this leaf
 * record identifies the exact fail-closed boundary and preserves the first
 * live preimage conflict without accepting it.
 */
enum GuSourcePrepareStage {
    GU_SOURCE_PREPARE_NOT_RUN = 0,
    GU_SOURCE_PREPARE_ARGUMENT = 1,
    GU_SOURCE_PREPARE_TARGET_IDENTITY = 2,
    GU_SOURCE_PREPARE_RUNTIME_INITIALIZE = 3,
    GU_SOURCE_PREPARE_CONTRACT_SHAPE = 4,
    GU_SOURCE_PREPARE_TRANSACTION_CAPACITY = 5,
    GU_SOURCE_PREPARE_LIVE_PREIMAGE = 6,
    GU_SOURCE_PREPARE_ENTER_STUB = 7,
    GU_SOURCE_PREPARE_LEAVE_STUB = 8,
    GU_SOURCE_PREPARE_CAP_STUB = 9,
    GU_SOURCE_PREPARE_WRITER_STUB = 10,
    GU_SOURCE_PREPARE_READER_STUB = 11,
    GU_SOURCE_PREPARE_ADD_ENTER = 12,
    GU_SOURCE_PREPARE_ADD_LEAVE = 13,
    GU_SOURCE_PREPARE_ADD_CAP = 14,
    GU_SOURCE_PREPARE_ADD_WRITER = 15,
    GU_SOURCE_PREPARE_ADD_READER = 16,
    GU_SOURCE_PREPARE_ADD_RADIUS = 17,
    GU_SOURCE_PREPARE_CONFIGURE = 18,
    GU_SOURCE_PREPARE_COMPLETE = 19
};

enum GuSourcePrepareSiteKind {
    GU_SOURCE_PREPARE_SITE_NONE = 0,
    GU_SOURCE_PREPARE_SITE_ENTER_POST_CALL = 1,
    GU_SOURCE_PREPARE_SITE_LEAVE_CALL = 2,
    GU_SOURCE_PREPARE_SITE_CAP = 3,
    GU_SOURCE_PREPARE_SITE_WRITER = 4,
    GU_SOURCE_PREPARE_SITE_READER = 5,
    GU_SOURCE_PREPARE_SITE_RADIUS = 6,
    GU_SOURCE_PREPARE_SITE_PROOF = 7
};

struct GuSourcePrepareDiagnostic {
    DWORD stage;
    DWORD site_kind;
    DWORD site_index;
    DWORD site_rva;
    DWORD site_length;
    DWORD first_mismatch_offset;
    DWORD image_rebase_delta;
    DWORD tls_index;
    DWORD win32_error;
    DWORD transaction_count_before;
    DWORD transaction_count_partial;
    DWORD transaction_count_after;
    DWORD transaction_committed;
    BYTE expected[GU_PATCH_MAX_BYTES];
    BYTE observed[GU_PATCH_MAX_BYTES];
};

enum GuSourceBankAbi {
    GU_SOURCE_BANK_ABI_NONE = 0,
    GU_SOURCE_BANK_ABI_CE_LEG = 1,
    GU_SOURCE_BANK_ABI_PATCH_7_8 = 2,
    GU_SOURCE_BANK_ABI_PATCH_4 = 3
};

enum GuSourceBankRegister {
    GU_SOURCE_BANK_REGISTER_NONE = 0,
    GU_SOURCE_BANK_REGISTER_EBP = 1,
    GU_SOURCE_BANK_REGISTER_EDI = 2,
    GU_SOURCE_BANK_REGISTER_EAX = 3
};

/*
 * One of these contracts is selected only after the root identity layer has
 * matched both target_id and the complete executable SHA-256.  Equal RVAs in
 * different target rows do not imply transferable support.
 */
struct GuSourceBankFamilyContract {
    DWORD abi;
    DWORD critical_object_rva;
    DWORD engine_leave_helper_rva;
    DWORD enter_guard_stack_offset;
    DWORD stock_writer_base_offset;
    DWORD stock_reader_base_offset;
    DWORD writer_count_stack_offset;
    DWORD writer_count_register;
    DWORD cap_count_register;

    /* Installed as one all-or-nothing transaction. */
    struct GuPatchContract enter_post_call;
    struct GuPatchContract leave_call;
    struct GuPatchContract cap;
    struct GuPatchContract writer;
    struct GuPatchContract reader;
    struct GuPatchContract radius_sites[GU_SOURCE_BANK_MAX_RADIUS_SITES];
    DWORD radius_site_count;
    DWORD radius_float_offset;
    DWORD radius_patch_kind;

    /* Read-only lifetime/context proofs validated before any patch is added. */
    struct GuPatchContract proof_sites[GU_SOURCE_BANK_MAX_PROOF_SITES];
    DWORD proof_site_count;
};

struct GuSourceBankTargetContract {
    const char *target_id;
    const char *sha256;
    const struct GuSourceBankFamilyContract *family;
};

struct GuSourceBankPrepared {
    BYTE *enter_stub;
    BYTE *leave_stub;
    BYTE *cap_stub;
    BYTE *writer_stub;
    BYTE *reader_stub;
};

struct GuSourceBankCounters {
    DWORD active_frames;
    DWORD stock_fallback_frames;
    DWORD bank_exhaustions;
    DWORD bounds_refusals;
    DWORD frame_mismatches;
    DWORD tls_failures;
};

DWORD gu_source_bank_target_contract_count(void);
const struct GuSourceBankTargetContract *gu_source_bank_target_contract_at(
    DWORD index);
const struct GuSourceBankTargetContract *gu_source_bank_contract_for_identity(
    const char *target_id, const char *sha256);

int gu_source_bank_initialize(void);
/* Call only after patch rollback and external engine-thread quiescence. */
int gu_source_bank_shutdown(void);
int gu_source_bank_configure(
    DWORD capacity, float distance_multiplier);

/*
 * The first argument must describe the engine lock state after the exact
 * EnterCriticalSection helper has returned.  A false value creates a STOCK
 * frame.  The generated entry bridges are the production callers.
 */
int __stdcall gu_source_bank_enter_frame(DWORD engine_lock_entered);
int __stdcall gu_source_bank_leave_frame(void);

DWORD gu_source_bank_active_capacity(void);
DWORD *gu_source_bank_active_buffer(void);
float gu_source_bank_radius(void);
void gu_source_bank_get_counters(
    struct GuSourceBankCounters *counters);
void gu_source_bank_get_prepare_diagnostic(
    struct GuSourcePrepareDiagnostic *diagnostic);
const char *gu_source_prepare_stage_name(DWORD stage);
const char *gu_source_prepare_site_name(DWORD site_kind);

/* Public for the generated x86 bridges and focused invariant tests. */
int __stdcall gu_source_bank_append(
    DWORD pointer_value, DWORD index, DWORD *stock_buffer);
DWORD __stdcall gu_source_bank_read(
    DWORD index, const DWORD *stock_buffer);

int gu_source_bank_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuSourceBankTargetContract *target,
    DWORD capacity,
    float distance_multiplier,
    struct GuSourceBankPrepared *prepared);
void gu_source_bank_release_prepared(
    struct GuSourceBankPrepared *prepared);

#if defined(GTAIV_GRASS_SOURCE_BANK_TESTING)
DWORD gu_source_bank_test_lease_owner(DWORD bank_index);
DWORD gu_source_bank_test_frame_depth(void);
DWORD gu_source_bank_test_fallback_depth(void);
DWORD gu_source_bank_test_tls_index(void);
#endif

#endif
