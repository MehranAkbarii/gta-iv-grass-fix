#ifndef GTAIV_GRASS_UNIVERSAL_HOOKS_H
#define GTAIV_GRASS_UNIVERSAL_HOOKS_H

#include <windows.h>

#define GU_PATCH_MAX_BYTES 32u
#define GU_TRANSACTION_MAX_PATCHES 64u

enum GuSourceArrayAbi {
    GU_SOURCE_ARRAY_ABI_NONE = 0,
    GU_SOURCE_ARRAY_ABI_CE = 1,
    GU_SOURCE_ARRAY_ABI_PATCH = 2
};

enum GuRadiusPatchKind {
    GU_RADIUS_PATCH_DIRECT_FLOAT = 1,
    GU_RADIUS_PATCH_POINTER_OPERAND = 2
};

struct GuPatchContract {
    DWORD rva;
    DWORD length;
    BYTE expected[GU_PATCH_MAX_BYTES];
    DWORD preferred_image_base;
    DWORD relocation_count;
    BYTE relocation_offsets[4];
};

struct GuInstalledPatch {
    BYTE *address;
    DWORD length;
    BYTE original[GU_PATCH_MAX_BYTES];
    BYTE replacement[GU_PATCH_MAX_BYTES];
    DWORD original_protection;
    DWORD protection_known;
    /* 0=restored/not written, 1=replacement live, 2=restore incomplete. */
    DWORD installed;
};

struct GuDetour {
    struct GuInstalledPatch patch;
    BYTE *gateway;
    DWORD gateway_size;
};

struct GuPatchTransaction {
    struct GuInstalledPatch patches[GU_TRANSACTION_MAX_PATCHES];
    DWORD count;
    DWORD committed;
};

struct GuSourceArrayContract {
    DWORD abi;
    struct GuPatchContract writer;
    struct GuPatchContract reader;
    struct GuPatchContract cap;
    struct GuPatchContract radius_sites[3];
    DWORD radius_site_count;
    DWORD radius_float_offset;
    DWORD radius_patch_kind;
};

int gu_encode_rel32(
    DWORD instruction_address, DWORD instruction_length,
    DWORD destination_address, DWORD *relative_out);

void gu_patch_transaction_initialize(
    struct GuPatchTransaction *transaction);
int gu_patch_contract_validate(
    BYTE *image_base, const struct GuPatchContract *contract);
int gu_patch_transaction_add(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *replacement);
int gu_patch_transaction_add_jump(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *stub);
int gu_patch_transaction_add_call(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *stub);
int gu_patch_transaction_add_prepared_detour(
    struct GuPatchTransaction *transaction,
    const struct GuDetour *detour);
int gu_patch_transaction_commit(
    struct GuPatchTransaction *transaction);
int gu_patch_transaction_rollback(
    struct GuPatchTransaction *transaction);
int gu_patch_transaction_has_unrestored(
    const struct GuPatchTransaction *transaction);

int gu_detour_prepare(
    struct GuDetour *detour, BYTE *image_base,
    const struct GuPatchContract *contract, const void *hook);
int gu_detour_install(struct GuDetour *detour);
int gu_detour_remove(struct GuDetour *detour);
void gu_detour_release(struct GuDetour *detour);

int gu_source_array_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuSourceArrayContract *contract,
    float radius, DWORD capacity,
    BYTE **writer_stub_out, BYTE **reader_stub_out);
void gu_source_array_release_stubs(
    BYTE *writer_stub, BYTE *reader_stub);

int gu_manager_terminal_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const void *fastcall_postscale_hook, BYTE **stub_out);
void gu_manager_terminal_release_stub(BYTE *stub);

#endif
