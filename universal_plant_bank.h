#ifndef GTAIV_GRASS_UNIVERSAL_PLANT_BANK_H
#define GTAIV_GRASS_UNIVERSAL_PLANT_BANK_H

#include <windows.h>
#include "universal_hooks.h"

#define GU_PLANT_BANK_COUNT 4u
#define GU_PLANT_BANK_CAPACITY 128u
#define GU_PLANT_BANK_BYTES 0x5B00u
#define GU_PLANT_BANK_CANDIDATE_RECORDS_OFFSET 0x0000u
#define GU_PLANT_BANK_OUTPUT_RECORDS_OFFSET 0x1000u
#define GU_PLANT_BANK_CANDIDATE_POINTERS_OFFSET 0x4000u
#define GU_PLANT_BANK_OUTPUT_POINTERS_OFFSET 0x5900u
#define GU_PLANT_BANK_POINTER_DELTA 0x1900u
#define GU_PLANT_BANK_CANDIDATE_STRIDE 0x20u
#define GU_PLANT_BANK_OUTPUT_STRIDE 0x60u
#define GU_PLANT_BANK_TRANSACTION_PATCHES 7u
#define GU_PLANT_BANK_MAX_PROOF_SITES 10u

enum GuPlantBankAbi {
    GU_PLANT_BANK_ABI_NONE = 0,
    GU_PLANT_BANK_ABI_CE_LEG = 1,
    GU_PLANT_BANK_ABI_PATCH = 2
};

/*
 * The callback returns the IEEE-754 bits of a finite multiplier.  Returning
 * 1.0f is the fail-closed answer for UNKNOWN, clutter, ordinary placed, or
 * otherwise unproved content.  This component deliberately owns no global
 * PLANT multiplier and never classifies a target from a name substring.
 * The callback runs in the generator hook: it must use fixed read-only data
 * and must not allocate, format strings, resolve symbols, or write files.
 */
struct GuPlantSelectionContext {
    DWORD abi;
    const void *source_entity;
    const void *source_model_info;
    const void *descriptor;
    const void *geometry_entry;
    DWORD source_model_index;
    DWORD source_model_hash;
    DWORD output_model_registry_key;
    DWORD submesh_index;
    DWORD geometry_type;
    DWORD descriptor_field_14_bits;
    DWORD descriptor_field_1c_bits;
    DWORD authored_float_bits;
    DWORD current_total;
};

typedef DWORD (__stdcall *GuPlantSelectionCallback)(
    const struct GuPlantSelectionContext *context,
    void *user_context);

struct GuPlantBankFamilyContract {
    DWORD abi;
    DWORD total_stack_offset;
    DWORD source_model_info_stack_offset;
    DWORD saved_descriptor_stack_offset;
    DWORD generation_entry_rva;
    struct GuPatchContract count_site;
    struct GuPatchContract candidate_record_site;
    struct GuPatchContract candidate_pointer_site;
    struct GuPatchContract paired_output_read_site;
    struct GuPatchContract projector_output_site;
    struct GuPatchContract projector_candidate_site;
    struct GuPatchContract drain_output_site;
    struct GuPatchContract generation_call_site;
    struct GuPatchContract proof_sites[GU_PLANT_BANK_MAX_PROOF_SITES];
    DWORD proof_site_count;
};

struct GuPlantBankTargetContract {
    const char *target_id;
    const char *sha256;
    const struct GuPlantBankFamilyContract *family;
    DWORD model_info_table_rva;
    struct GuPatchContract model_info_table_load_site;
};

struct GuPlantBankPrepared {
    BYTE *count_stub;
    BYTE *candidate_record_stub;
    BYTE *candidate_pointer_stub;
    BYTE *projector_output_stub;
    BYTE *projector_candidate_stub;
    BYTE *drain_output_stub;
    BYTE *generation_wrapper_stub;
};

struct GuPlantBankCounters {
    DWORD count_calls;
    DWORD invalid_calls;
    DWORD selected_calls;
    DWORD authored_count_total;
    DWORD emitted_count_total;
    DWORD clamp_events;
    DWORD capacity_saturations;
    DWORD maximum_total_after;
};

DWORD gu_plant_bank_target_contract_count(void);
const struct GuPlantBankTargetContract *gu_plant_bank_target_contract_at(
    DWORD index);
const struct GuPlantBankTargetContract *gu_plant_bank_contract_for_target(
    const char *target_id);

int gu_plant_bank_initialize(void);
int gu_plant_bank_shutdown(void);
int gu_plant_bank_set_selector(
    GuPlantSelectionCallback callback, void *user_context);
void gu_plant_bank_clear_selector(void);
void gu_plant_bank_get_counters(
    struct GuPlantBankCounters *counters);

/* These are public for focused ABI/concurrency tests and wrapper generation. */
int __stdcall gu_plant_bank_enter_frame(void);
void __stdcall gu_plant_bank_leave_frame(void);
DWORD gu_plant_bank_active_capacity(void);
void *gu_plant_bank_active_candidate_records(void);
void *gu_plant_bank_active_candidate_pointers(void);
void *gu_plant_bank_active_output_pointers(void);

DWORD __stdcall gu_plant_bank_compute_selected_count(
    DWORD abi, const void *source_entity,
    const void *source_model_info, const void *descriptor,
    const void *saved_descriptor, const void *geometry_entry,
    DWORD submesh_index, DWORD authored_float_bits,
    DWORD current_total);

int gu_plant_bank_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPlantBankTargetContract *contract,
    struct GuPlantBankPrepared *prepared);
void gu_plant_bank_release_prepared(
    struct GuPlantBankPrepared *prepared);

/* Read-only layout inspection for focused tests and deferred telemetry. */
void *gu_plant_bank_storage_base(void);
void *gu_plant_bank_base(DWORD bank_index);

#endif
