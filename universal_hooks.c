#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_pipeline.h"
#include "universal_hooks.h"

static DWORD gu_read_u32(const BYTE *address)
{
    return (DWORD)address[0] |
           ((DWORD)address[1] << 8) |
           ((DWORD)address[2] << 16) |
           ((DWORD)address[3] << 24);
}

static void gu_write_u32(BYTE *address, DWORD value)
{
    address[0] = (BYTE)(value & 0xFFu);
    address[1] = (BYTE)((value >> 8) & 0xFFu);
    address[2] = (BYTE)((value >> 16) & 0xFFu);
    address[3] = (BYTE)((value >> 24) & 0xFFu);
}

static int gu_bytes_equal(
    const BYTE *left, const BYTE *right, DWORD length)
{
    DWORD index;
    for (index = 0u; index < length; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

static void gu_copy_bytes(
    BYTE *destination, const BYTE *source, DWORD length)
{
    DWORD index;
    for (index = 0u; index < length; ++index) {
        destination[index] = source[index];
    }
}

static int gu_materialize_expected(
    BYTE *image_base, const struct GuPatchContract *contract,
    BYTE *materialized)
{
    DWORD index;
    DWORD delta;
    if (!image_base || !contract || !materialized ||
        contract->length == 0u ||
        contract->length > GU_PATCH_MAX_BYTES ||
        contract->relocation_count > 4u ||
        (contract->relocation_count != 0u &&
         contract->preferred_image_base == 0u)) {
        return 0;
    }
    gu_copy_bytes(
        materialized, contract->expected, contract->length);
    delta = (DWORD)image_base - contract->preferred_image_base;
    for (index = 0u;
         index < contract->relocation_count; ++index) {
        DWORD offset = contract->relocation_offsets[index];
        if (offset + 4u > contract->length) {
            return 0;
        }
        gu_write_u32(
            materialized + offset,
            gu_read_u32(contract->expected + offset) + delta);
    }
    return 1;
}

static int gu_make_rel32(
    const BYTE *instruction, const BYTE *destination,
    DWORD instruction_length, DWORD *relative_out)
{
    if (!instruction || !destination || !relative_out) {
        return 0;
    }
    return gu_encode_rel32(
        (DWORD)instruction, instruction_length,
        (DWORD)destination, relative_out);
}

int gu_encode_rel32(
    DWORD instruction_address, DWORD instruction_length,
    DWORD destination_address, DWORD *relative_out)
{
    if (!relative_out || instruction_length == 0u) {
        return 0;
    }
    /* x86 EIP-relative addition is modulo 2^32.  Therefore every pair of
     * 32-bit addresses has an exact rel32 encoding, including pairs that are
     * more than 2 GiB apart when interpreted as unsigned linear values. */
    *relative_out = destination_address -
                    (instruction_address + instruction_length);
    return 1;
}

static int gu_write_patch(struct GuInstalledPatch *patch)
{
    DWORD old_protection = 0u;
    DWORD unused_protection = 0u;
    DWORD index;
    int write_ok;
    int restore_bytes_ok;
    int restore_flush_ok;
    int restore_protection_ok;

    if (!patch || !patch->address || patch->length == 0u ||
        patch->length > GU_PATCH_MAX_BYTES ||
        !gu_bytes_equal(
            patch->address, patch->original, patch->length)) {
        return 0;
    }
    if (!VirtualProtect(
            patch->address, patch->length,
            PAGE_EXECUTE_READWRITE, &old_protection)) {
        return 0;
    }
    patch->original_protection = old_protection;
    patch->protection_known = 1u;
    for (index = 0u; index < patch->length; ++index) {
        patch->address[index] = patch->replacement[index];
    }
    write_ok = gu_bytes_equal(
        patch->address, patch->replacement, patch->length);
    if (write_ok) {
        write_ok = FlushInstructionCache(
            GetCurrentProcess(), patch->address,
            patch->length) != 0;
    }
    if (write_ok && VirtualProtect(
            patch->address, patch->length,
            old_protection, &unused_protection)) {
        patch->installed = 1u;
        return 1;
    }

    /*
     * A failed application is not allowed to return while a replacement may
     * still branch into mod-owned memory.  Restore bytes, cache state, and
     * page protection independently and retain a pending state if any proof
     * fails so callers cannot free gateways or banks.
     */
    restore_bytes_ok = 1;
    if (!gu_bytes_equal(
            patch->address, patch->original, patch->length)) {
        for (index = 0u; index < patch->length; ++index) {
            patch->address[index] = patch->original[index];
        }
        restore_bytes_ok = gu_bytes_equal(
            patch->address, patch->original, patch->length);
    }
    restore_flush_ok = restore_bytes_ok && FlushInstructionCache(
        GetCurrentProcess(), patch->address, patch->length) != 0;
    restore_protection_ok = VirtualProtect(
        patch->address, patch->length,
        old_protection, &unused_protection) != 0;
    patch->installed =
        restore_bytes_ok && restore_flush_ok && restore_protection_ok ?
            0u :
        gu_bytes_equal(
            patch->address, patch->replacement, patch->length) ? 1u : 2u;
    return 0;
}

static int gu_restore_patch(struct GuInstalledPatch *patch)
{
    DWORD old_protection = 0u;
    DWORD unused_protection = 0u;
    DWORD index;
    int bytes_ok;
    int flush_ok;
    int protection_ok;

    if (!patch || !patch->installed || !patch->address ||
        patch->length == 0u ||
        patch->length > GU_PATCH_MAX_BYTES ||
        !patch->protection_known) {
        return 0;
    }
    bytes_ok = gu_bytes_equal(
        patch->address, patch->original, patch->length);
    if (!bytes_ok) {
        if (!gu_bytes_equal(
                patch->address, patch->replacement,
                patch->length) ||
            !VirtualProtect(
                patch->address, patch->length,
                PAGE_EXECUTE_READWRITE, &old_protection)) {
            patch->installed = 2u;
            return 0;
        }
        for (index = 0u; index < patch->length; ++index) {
            patch->address[index] = patch->original[index];
        }
        bytes_ok = gu_bytes_equal(
            patch->address, patch->original, patch->length);
    }
    flush_ok = bytes_ok && FlushInstructionCache(
        GetCurrentProcess(), patch->address, patch->length) != 0;
    protection_ok = VirtualProtect(
            patch->address, patch->length,
            patch->original_protection,
            &unused_protection) != 0;
    if (bytes_ok && flush_ok && protection_ok) {
        patch->installed = 0u;
        return 1;
    }
    patch->installed = gu_bytes_equal(
        patch->address, patch->replacement,
        patch->length) ? 1u : 2u;
    return 0;
}

void gu_patch_transaction_initialize(
    struct GuPatchTransaction *transaction)
{
    if (transaction) {
        ZeroMemory(transaction, sizeof(*transaction));
    }
}

int gu_patch_contract_validate(
    BYTE *image_base, const struct GuPatchContract *contract)
{
    BYTE materialized[GU_PATCH_MAX_BYTES];
    return gu_materialize_expected(
               image_base, contract, materialized) &&
           gu_bytes_equal(
               image_base + contract->rva,
               materialized, contract->length);
}

int gu_patch_transaction_add(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *replacement)
{
    struct GuInstalledPatch *patch;
    BYTE materialized[GU_PATCH_MAX_BYTES];
    DWORD index;
    DWORD candidate_start;
    DWORD candidate_end;
    if (!transaction || !image_base || !contract || !replacement ||
        transaction->count >= GU_TRANSACTION_MAX_PATCHES ||
        contract->length == 0u ||
        contract->length > GU_PATCH_MAX_BYTES) {
        return 0;
    }
    if (!gu_materialize_expected(
            image_base, contract, materialized)) {
        return 0;
    }
    candidate_start = (DWORD)(image_base + contract->rva);
    if (candidate_start > 0xFFFFFFFFu - contract->length) {
        return 0;
    }
    candidate_end = candidate_start + contract->length;
    for (index = 0u; index < transaction->count; ++index) {
        DWORD existing_start =
            (DWORD)transaction->patches[index].address;
        DWORD existing_end;
        if (existing_start > 0xFFFFFFFFu -
                transaction->patches[index].length) {
            return 0;
        }
        existing_end = existing_start +
                       transaction->patches[index].length;
        if (candidate_start < existing_end &&
            existing_start < candidate_end) {
            return 0;
        }
    }
    patch = &transaction->patches[transaction->count];
    patch->address = image_base + contract->rva;
    patch->length = contract->length;
    gu_copy_bytes(
        patch->original, materialized, contract->length);
    gu_copy_bytes(
        patch->replacement, replacement, contract->length);
    if (!gu_bytes_equal(
            patch->address, patch->original,
            patch->length)) {
        ZeroMemory(patch, sizeof(*patch));
        return 0;
    }
    ++transaction->count;
    return 1;
}

int gu_patch_transaction_commit(
    struct GuPatchTransaction *transaction)
{
    DWORD index;
    if (!transaction || transaction->committed) {
        return 0;
    }
    /* Validate the whole transaction again immediately before any write. */
    for (index = 0u; index < transaction->count; ++index) {
        struct GuInstalledPatch *patch = &transaction->patches[index];
        if (!gu_bytes_equal(
                patch->address, patch->original,
                patch->length)) {
            return 0;
        }
    }
    for (index = 0u; index < transaction->count; ++index) {
        if (!gu_write_patch(&transaction->patches[index])) {
            int rollback_ok = 1;
            while (index > 0u) {
                --index;
                if (transaction->patches[index].installed != 0u &&
                    !gu_restore_patch(&transaction->patches[index])) {
                    rollback_ok = 0;
                }
            }
            transaction->committed =
                gu_patch_transaction_has_unrestored(transaction) ? 1u : 0u;
            (void)rollback_ok;
            return 0;
        }
    }
    transaction->committed = 1u;
    return 1;
}

int gu_patch_transaction_rollback(
    struct GuPatchTransaction *transaction)
{
    DWORD index;
    int ok = 1;
    if (!transaction) {
        return 0;
    }
    index = transaction->count;
    while (index > 0u) {
        --index;
        struct GuInstalledPatch *patch = &transaction->patches[index];
        if (patch->installed == 0u &&
            !gu_bytes_equal(
                patch->original, patch->replacement,
                patch->length) &&
            gu_bytes_equal(
                patch->address, patch->replacement,
                patch->length)) {
            patch->installed = 1u;
        }
        if (patch->installed != 0u &&
            !gu_restore_patch(patch)) {
            ok = 0;
        }
    }
    transaction->committed =
        gu_patch_transaction_has_unrestored(transaction) ? 1u : 0u;
    return ok && !transaction->committed;
}

int gu_patch_transaction_has_unrestored(
    const struct GuPatchTransaction *transaction)
{
    DWORD index;
    if (!transaction) {
        return 0;
    }
    for (index = 0u; index < transaction->count; ++index) {
        const struct GuInstalledPatch *patch =
            &transaction->patches[index];
        if (patch->installed != 0u ||
            (patch->address && patch->length != 0u &&
             patch->length <= GU_PATCH_MAX_BYTES &&
             !gu_bytes_equal(
                 patch->original, patch->replacement,
                 patch->length) &&
             gu_bytes_equal(
                 patch->address, patch->replacement,
                 patch->length))) {
            return 1;
        }
    }
    return 0;
}

int gu_detour_prepare(
    struct GuDetour *detour, BYTE *image_base,
    const struct GuPatchContract *contract, const void *hook)
{
    BYTE *gateway;
    BYTE materialized[GU_PATCH_MAX_BYTES];
    DWORD relative;
    DWORD index;

    if (!detour || !image_base || !contract || !hook ||
        contract->length < 5u ||
        contract->length > GU_PATCH_MAX_BYTES) {
        return 0;
    }
    if (!gu_materialize_expected(
            image_base, contract, materialized)) {
        return 0;
    }
    ZeroMemory(detour, sizeof(*detour));
    detour->patch.address = image_base + contract->rva;
    detour->patch.length = contract->length;
    gu_copy_bytes(
        detour->patch.original, materialized,
        contract->length);
    if (!gu_bytes_equal(
            detour->patch.address, detour->patch.original,
            contract->length)) {
        return 0;
    }

    gateway = (BYTE *)VirtualAlloc(
        NULL, contract->length + 5u,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!gateway) {
        return 0;
    }
    gu_copy_bytes(gateway, materialized, contract->length);
    gateway[contract->length] = 0xE9u;
    if (!gu_make_rel32(
            gateway + contract->length,
            detour->patch.address + contract->length,
            5u, &relative)) {
        VirtualFree(gateway, 0u, MEM_RELEASE);
        return 0;
    }
    gu_write_u32(gateway + contract->length + 1u, relative);
    {
        DWORD old_protection = 0u;
        if (!VirtualProtect(
                gateway, contract->length + 5u,
                PAGE_EXECUTE_READ, &old_protection) ||
            !FlushInstructionCache(
                GetCurrentProcess(), gateway,
                contract->length + 5u)) {
            VirtualFree(gateway, 0u, MEM_RELEASE);
            return 0;
        }
    }

    detour->patch.replacement[0] = 0xE9u;
    if (!gu_make_rel32(
            detour->patch.address, (const BYTE *)hook,
            5u, &relative)) {
        VirtualFree(gateway, 0u, MEM_RELEASE);
        return 0;
    }
    gu_write_u32(detour->patch.replacement + 1u, relative);
    for (index = 5u; index < contract->length; ++index) {
        detour->patch.replacement[index] = 0x90u;
    }
    detour->gateway = gateway;
    detour->gateway_size = contract->length + 5u;
    return 1;
}

int gu_detour_install(struct GuDetour *detour)
{
    return detour && gu_write_patch(&detour->patch);
}

int gu_detour_remove(struct GuDetour *detour)
{
    return detour && gu_restore_patch(&detour->patch);
}

void gu_detour_release(struct GuDetour *detour)
{
    if (!detour) {
        return;
    }
    if (detour->patch.installed) {
        gu_detour_remove(detour);
    }
    if (detour->gateway) {
        VirtualFree(detour->gateway, 0u, MEM_RELEASE);
    }
    ZeroMemory(detour, sizeof(*detour));
}

static BYTE *gu_allocate_stub(DWORD size)
{
    return (BYTE *)VirtualAlloc(
        NULL, size, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
}

static int gu_finalize_stub(BYTE *stub, DWORD size)
{
    DWORD old_protection = 0u;
    if (!stub || size == 0u) {
        return 0;
    }
    return VirtualProtect(
               stub, size, PAGE_EXECUTE_READ,
               &old_protection) != 0 &&
           FlushInstructionCache(
               GetCurrentProcess(), stub, size) != 0;
}

static BYTE *gu_build_source_writer_stub(
    DWORD abi, BYTE *continuation)
{
    BYTE *stub = gu_allocate_stub(32u);
    DWORD cursor = 0u;
    DWORD relative;
    if (!stub) {
        return NULL;
    }
    if (abi == GU_SOURCE_ARRAY_ABI_CE) {
        /* mov [g_gu_source_buffer + ebp*4],edi; inc ebp */
        stub[cursor++] = 0x89u;
        stub[cursor++] = 0x3Cu;
        stub[cursor++] = 0xADu;
        gu_write_u32(stub + cursor, (DWORD)g_gu_source_buffer);
        cursor += 4u;
        stub[cursor++] = 0x45u;
    } else if (abi == GU_SOURCE_ARRAY_ABI_PATCH) {
        /* mov [g_gu_source_buffer + eax*4],edi; add eax,1 */
        stub[cursor++] = 0x89u;
        stub[cursor++] = 0x3Cu;
        stub[cursor++] = 0x85u;
        gu_write_u32(stub + cursor, (DWORD)g_gu_source_buffer);
        cursor += 4u;
        stub[cursor++] = 0x83u;
        stub[cursor++] = 0xC0u;
        stub[cursor++] = 0x01u;
    } else {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    stub[cursor++] = 0xE9u;
    if (!gu_make_rel32(
            stub + cursor - 1u, continuation,
            5u, &relative)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    gu_write_u32(stub + cursor, relative);
    cursor += 4u;
    if (!gu_finalize_stub(stub, cursor)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static BYTE *gu_build_source_reader_stub(
    DWORD abi, BYTE *continuation)
{
    BYTE *stub = gu_allocate_stub(32u);
    DWORD cursor = 0u;
    DWORD relative;
    if (!stub) {
        return NULL;
    }
    if (abi == GU_SOURCE_ARRAY_ABI_CE) {
        /* push [g_gu_source_buffer + esi*4]; mov ecx,ebx */
        stub[cursor++] = 0xFFu;
        stub[cursor++] = 0x34u;
        stub[cursor++] = 0xB5u;
        gu_write_u32(stub + cursor, (DWORD)g_gu_source_buffer);
        cursor += 4u;
        stub[cursor++] = 0x8Bu;
        stub[cursor++] = 0xCBu;
    } else if (abi == GU_SOURCE_ARRAY_ABI_PATCH) {
        /* mov ecx,[g_gu_source_buffer + esi*4]; push ecx */
        stub[cursor++] = 0x8Bu;
        stub[cursor++] = 0x0Cu;
        stub[cursor++] = 0xB5u;
        gu_write_u32(stub + cursor, (DWORD)g_gu_source_buffer);
        cursor += 4u;
        stub[cursor++] = 0x51u;
    } else {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    stub[cursor++] = 0xE9u;
    if (!gu_make_rel32(
            stub + cursor - 1u, continuation,
            5u, &relative)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    gu_write_u32(stub + cursor, relative);
    cursor += 4u;
    if (!gu_finalize_stub(stub, cursor)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

int gu_patch_transaction_add_jump(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *stub)
{
    BYTE replacement[GU_PATCH_MAX_BYTES];
    DWORD relative;
    DWORD index;
    if (!contract || contract->length < 5u ||
        contract->length > GU_PATCH_MAX_BYTES) {
        return 0;
    }
    ZeroMemory(replacement, sizeof(replacement));
    replacement[0] = 0xE9u;
    if (!gu_make_rel32(
            image_base + contract->rva, stub,
            5u, &relative)) {
        return 0;
    }
    gu_write_u32(replacement + 1u, relative);
    for (index = 5u; index < contract->length; ++index) {
        replacement[index] = 0x90u;
    }
    return gu_patch_transaction_add(
        transaction, image_base, contract, replacement);
}

int gu_patch_transaction_add_call(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const BYTE *stub)
{
    BYTE replacement[GU_PATCH_MAX_BYTES];
    DWORD relative;
    DWORD index;
    if (!contract || contract->length < 5u ||
        contract->length > GU_PATCH_MAX_BYTES) {
        return 0;
    }
    ZeroMemory(replacement, sizeof(replacement));
    replacement[0] = 0xE8u;
    if (!gu_make_rel32(
            image_base + contract->rva, stub,
            5u, &relative)) {
        return 0;
    }
    gu_write_u32(replacement + 1u, relative);
    for (index = 5u; index < contract->length; ++index) {
        replacement[index] = 0x90u;
    }
    return gu_patch_transaction_add(
        transaction, image_base, contract, replacement);
}

int gu_patch_transaction_add_prepared_detour(
    struct GuPatchTransaction *transaction,
    const struct GuDetour *detour)
{
    struct GuPatchContract contract;
    if (!transaction || !detour || !detour->patch.address ||
        !detour->gateway || detour->patch.installed ||
        detour->patch.length < 5u ||
        detour->patch.length > GU_PATCH_MAX_BYTES) {
        return 0;
    }
    ZeroMemory(&contract, sizeof(contract));
    contract.rva = 0u;
    contract.length = detour->patch.length;
    gu_copy_bytes(
        contract.expected, detour->patch.original,
        detour->patch.length);
    return gu_patch_transaction_add(
        transaction, detour->patch.address,
        &contract, detour->patch.replacement);
}

int gu_source_array_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuSourceArrayContract *contract,
    float radius, DWORD capacity,
    BYTE **writer_stub_out, BYTE **reader_stub_out)
{
    BYTE *writer_stub;
    BYTE *reader_stub;
    BYTE cap_replacement[GU_PATCH_MAX_BYTES];
    BYTE radius_replacement[GU_PATCH_MAX_BYTES];
    DWORD radius_bits;
    DWORD index;

    if (!transaction || !image_base || !contract ||
        !writer_stub_out || !reader_stub_out ||
        capacity <= 128u || capacity > GU_SOURCE_BUFFER_CAPACITY ||
        radius < 30.0f || radius > 90.0f ||
        contract->cap.length != 6u ||
        contract->radius_site_count > 3u) {
        return 0;
    }
    writer_stub = gu_build_source_writer_stub(
        contract->abi,
        image_base + contract->writer.rva +
            contract->writer.length);
    if (!writer_stub) {
        return 0;
    }
    reader_stub = gu_build_source_reader_stub(
        contract->abi,
        image_base + contract->reader.rva +
            contract->reader.length);
    if (!reader_stub) {
        VirtualFree(writer_stub, 0u, MEM_RELEASE);
        return 0;
    }

    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &contract->writer, writer_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base,
            &contract->reader, reader_stub)) {
        VirtualFree(writer_stub, 0u, MEM_RELEASE);
        VirtualFree(reader_stub, 0u, MEM_RELEASE);
        return 0;
    }

    gu_copy_bytes(
        cap_replacement, contract->cap.expected,
        contract->cap.length);
    gu_write_u32(cap_replacement + 2u, capacity);
    if (!gu_patch_transaction_add(
            transaction, image_base,
            &contract->cap, cap_replacement)) {
        VirtualFree(writer_stub, 0u, MEM_RELEASE);
        VirtualFree(reader_stub, 0u, MEM_RELEASE);
        return 0;
    }

    g_gu_source_radius = radius;
    if (contract->radius_patch_kind ==
            GU_RADIUS_PATCH_DIRECT_FLOAT) {
        radius_bits = *(const DWORD *)(const void *)&radius;
    } else if (contract->radius_patch_kind ==
                   GU_RADIUS_PATCH_POINTER_OPERAND) {
        radius_bits = (DWORD)&g_gu_source_radius;
    } else {
        VirtualFree(writer_stub, 0u, MEM_RELEASE);
        VirtualFree(reader_stub, 0u, MEM_RELEASE);
        return 0;
    }
    for (index = 0u; index < contract->radius_site_count; ++index) {
        const struct GuPatchContract *site =
            &contract->radius_sites[index];
        if (contract->radius_float_offset + 4u > site->length) {
            VirtualFree(writer_stub, 0u, MEM_RELEASE);
            VirtualFree(reader_stub, 0u, MEM_RELEASE);
            return 0;
        }
        gu_copy_bytes(
            radius_replacement, site->expected,
            site->length);
        gu_write_u32(
            radius_replacement +
                contract->radius_float_offset,
            radius_bits);
        if (!gu_patch_transaction_add(
                transaction, image_base,
                site, radius_replacement)) {
            VirtualFree(writer_stub, 0u, MEM_RELEASE);
            VirtualFree(reader_stub, 0u, MEM_RELEASE);
            return 0;
        }
    }
    *writer_stub_out = writer_stub;
    *reader_stub_out = reader_stub;
    return 1;
}

void gu_source_array_release_stubs(
    BYTE *writer_stub, BYTE *reader_stub)
{
    if (writer_stub) {
        VirtualFree(writer_stub, 0u, MEM_RELEASE);
    }
    if (reader_stub) {
        VirtualFree(reader_stub, 0u, MEM_RELEASE);
    }
}

int gu_manager_terminal_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base, const struct GuPatchContract *contract,
    const void *fastcall_postscale_hook, BYTE **stub_out)
{
    BYTE *stub;
    DWORD cursor = 0u;
    DWORD relative;
    if (!transaction || !image_base || !contract ||
        !fastcall_postscale_hook || !stub_out ||
        contract->length != 6u ||
        contract->expected[0] != 0xF3u ||
        contract->expected[1] != 0x0Fu ||
        contract->expected[2] != 0x11u ||
        (contract->expected[3] != 0x41u &&
         contract->expected[3] != 0x49u) ||
        contract->expected[4] != 0x20u ||
        contract->expected[5] != 0xC3u) {
        return 0;
    }
    stub = gu_allocate_stub(96u);
    if (!stub) {
        return 0;
    }
    /* Preserve the exact target's original XMM store before post-scaling. */
    gu_copy_bytes(stub + cursor, contract->expected, 5u);
    cursor += 5u;
    stub[cursor++] = 0x9Cu; /* pushfd */
    stub[cursor++] = 0x60u; /* pushad */
    stub[cursor++] = 0x81u;
    stub[cursor++] = 0xECu; /* sub esp,528 */
    gu_write_u32(stub + cursor, 528u);
    cursor += 4u;
    stub[cursor++] = 0x8Du;
    stub[cursor++] = 0x44u;
    stub[cursor++] = 0x24u;
    stub[cursor++] = 0x0Fu; /* lea eax,[esp+15] */
    stub[cursor++] = 0x83u;
    stub[cursor++] = 0xE0u;
    stub[cursor++] = 0xF0u; /* and eax,-16 */
    stub[cursor++] = 0x0Fu;
    stub[cursor++] = 0xAEu;
    stub[cursor++] = 0x00u; /* fxsave [eax] */
    stub[cursor++] = 0xFCu; /* cld: Win32 C ABI requires DF clear */
    /* Isolate the C callback from the interrupted engine's rounding modes
     * and pending FP exceptions; FXRSTOR below reinstates the exact state. */
    stub[cursor++] = 0xDBu;
    stub[cursor++] = 0xE3u; /* fninit */
    stub[cursor++] = 0x68u; /* push 0x00001F80 */
    gu_write_u32(stub + cursor, 0x00001F80u);
    cursor += 4u;
    stub[cursor++] = 0x0Fu;
    stub[cursor++] = 0xAEu;
    stub[cursor++] = 0x14u;
    stub[cursor++] = 0x24u; /* ldmxcsr [esp] */
    stub[cursor++] = 0x83u;
    stub[cursor++] = 0xC4u;
    stub[cursor++] = 0x04u; /* add esp,4 */
    stub[cursor++] = 0xE8u;
    if (!gu_make_rel32(
            stub + cursor - 1u,
            (const BYTE *)fastcall_postscale_hook,
            5u, &relative)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_write_u32(stub + cursor, relative);
    cursor += 4u;
    stub[cursor++] = 0x8Du;
    stub[cursor++] = 0x44u;
    stub[cursor++] = 0x24u;
    stub[cursor++] = 0x0Fu; /* lea eax,[esp+15] */
    stub[cursor++] = 0x83u;
    stub[cursor++] = 0xE0u;
    stub[cursor++] = 0xF0u; /* and eax,-16 */
    stub[cursor++] = 0x0Fu;
    stub[cursor++] = 0xAEu;
    stub[cursor++] = 0x08u; /* fxrstor [eax] */
    stub[cursor++] = 0x81u;
    stub[cursor++] = 0xC4u; /* add esp,528 */
    gu_write_u32(stub + cursor, 528u);
    cursor += 4u;
    stub[cursor++] = 0x61u; /* popad */
    stub[cursor++] = 0x9Du; /* popfd */
    stub[cursor++] = 0xC3u; /* original return */
    if (!gu_finalize_stub(stub, cursor) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, contract, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    *stub_out = stub;
    return 1;
}

void gu_manager_terminal_release_stub(BYTE *stub)
{
    if (stub) {
        VirtualFree(stub, 0u, MEM_RELEASE);
    }
}
