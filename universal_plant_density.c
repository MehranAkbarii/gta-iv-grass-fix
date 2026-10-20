#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_hooks.h"
#include "universal_plant_density.h"

volatile LONG g_gu_plant_density_multiplier_bits = 0x3F800000L;

static void gu_write_u32(BYTE *address, DWORD value)
{
    address[0] = (BYTE)(value & 0xFFu);
    address[1] = (BYTE)((value >> 8) & 0xFFu);
    address[2] = (BYTE)((value >> 16) & 0xFFu);
    address[3] = (BYTE)((value >> 24) & 0xFFu);
}

static int gu_make_rel32(
    const BYTE *instruction, const BYTE *destination,
    DWORD *relative_out)
{
    if (!instruction || !destination || !relative_out) {
        return 0;
    }
    return gu_encode_rel32(
        (DWORD)instruction, 5u,
        (DWORD)destination, relative_out);
}

static int gu_patch_rel8(
    BYTE *stub, DWORD displacement_index, DWORD target)
{
    LONG relative = (LONG)target -
                    (LONG)(displacement_index + 1u);
    if (relative < -128 || relative > 127) {
        return 0;
    }
    stub[displacement_index] = (BYTE)(char)relative;
    return 1;
}

static int gu_multiplier_valid(float value)
{
    DWORD bits = *(const DWORD *)(const void *)&value;
    return (bits & 0x80000000u) == 0u &&
           (bits & 0x7F800000u) != 0x7F800000u &&
           value >= 0.1f && value <= 2.0f;
}

static BYTE *gu_build_plant_count_stub(
    const struct GuPlantCountContract *contract,
    BYTE *continuation)
{
    BYTE *stub = (BYTE *)VirtualAlloc(
        NULL, 128u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    DWORD cursor = 0u;
    DWORD jle_zero;
    DWORD jae_zero;
    DWORD jbe_done;
    DWORD jmp_done;
    DWORD zero_label;
    DWORD done_label;
    DWORD relative;
    DWORD old_protection = 0u;
    if (!stub || !contract ||
        contract->count_site.length != 8u ||
        contract->candidate_capacity == 0u ||
        contract->candidate_capacity > 128u) {
        if (stub) {
            VirtualFree(stub, 0u, MEM_RELEASE);
        }
        return NULL;
    }

    /* Preserve the exact target's raw floating count calculation. */
    stub[cursor++] = contract->count_site.expected[0];
    stub[cursor++] = contract->count_site.expected[1];
    stub[cursor++] = contract->count_site.expected[2];
    stub[cursor++] = contract->count_site.expected[3];
    if (contract->abi == GU_PLANT_COUNT_ABI_CE) {
        /* mulss xmm1,[g_gu_plant_density_multiplier_bits] */
        stub[cursor++] = 0xF3u;
        stub[cursor++] = 0x0Fu;
        stub[cursor++] = 0x59u;
        stub[cursor++] = 0x0Du;
    } else if (contract->abi == GU_PLANT_COUNT_ABI_PATCH) {
        /* mulss xmm0,[g_gu_plant_density_multiplier_bits] */
        stub[cursor++] = 0xF3u;
        stub[cursor++] = 0x0Fu;
        stub[cursor++] = 0x59u;
        stub[cursor++] = 0x05u;
    } else {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    gu_write_u32(
        stub + cursor,
        (DWORD)&g_gu_plant_density_multiplier_bits);
    cursor += 4u;
    stub[cursor++] = contract->count_site.expected[4];
    stub[cursor++] = contract->count_site.expected[5];
    stub[cursor++] = contract->count_site.expected[6];
    stub[cursor++] = contract->count_site.expected[7];

    stub[cursor++] = 0x9Cu; /* pushfd */
    if (contract->abi == GU_PLANT_COUNT_ABI_CE) {
        stub[cursor++] = 0x50u; /* push eax */
        stub[cursor++] = 0x8Bu;
        stub[cursor++] = 0x84u;
        stub[cursor++] = 0x24u;
        gu_write_u32(
            stub + cursor,
            contract->total_stack_offset + 8u);
        cursor += 4u;
        stub[cursor++] = 0x85u;
        stub[cursor++] = 0xFFu; /* test edi,edi */
        stub[cursor++] = 0x7Eu;
        jle_zero = cursor++;
        stub[cursor++] = 0x81u;
        stub[cursor++] = 0xF8u; /* cmp eax,capacity */
        gu_write_u32(
            stub + cursor, contract->candidate_capacity);
        cursor += 4u;
        stub[cursor++] = 0x73u;
        jae_zero = cursor++;
        stub[cursor++] = 0xF7u;
        stub[cursor++] = 0xD8u; /* neg eax */
        stub[cursor++] = 0x81u;
        stub[cursor++] = 0xC0u; /* add eax,capacity */
        gu_write_u32(
            stub + cursor, contract->candidate_capacity);
        cursor += 4u;
        stub[cursor++] = 0x39u;
        stub[cursor++] = 0xC7u; /* cmp edi,eax */
        stub[cursor++] = 0x76u;
        jbe_done = cursor++;
        stub[cursor++] = 0x89u;
        stub[cursor++] = 0xC7u; /* mov edi,eax */
        stub[cursor++] = 0xEBu;
        jmp_done = cursor++;
        zero_label = cursor;
        stub[cursor++] = 0x31u;
        stub[cursor++] = 0xFFu; /* xor edi,edi */
        done_label = cursor;
        stub[cursor++] = 0x58u; /* pop eax */
    } else {
        stub[cursor++] = 0x52u; /* push edx */
        stub[cursor++] = 0x8Bu;
        stub[cursor++] = 0x94u;
        stub[cursor++] = 0x24u;
        gu_write_u32(
            stub + cursor,
            contract->total_stack_offset + 8u);
        cursor += 4u;
        stub[cursor++] = 0x85u;
        stub[cursor++] = 0xC0u; /* test eax,eax */
        stub[cursor++] = 0x7Eu;
        jle_zero = cursor++;
        stub[cursor++] = 0x81u;
        stub[cursor++] = 0xFAu; /* cmp edx,capacity */
        gu_write_u32(
            stub + cursor, contract->candidate_capacity);
        cursor += 4u;
        stub[cursor++] = 0x73u;
        jae_zero = cursor++;
        stub[cursor++] = 0xF7u;
        stub[cursor++] = 0xDAu; /* neg edx */
        stub[cursor++] = 0x81u;
        stub[cursor++] = 0xC2u; /* add edx,capacity */
        gu_write_u32(
            stub + cursor, contract->candidate_capacity);
        cursor += 4u;
        stub[cursor++] = 0x39u;
        stub[cursor++] = 0xD0u; /* cmp eax,edx */
        stub[cursor++] = 0x76u;
        jbe_done = cursor++;
        stub[cursor++] = 0x89u;
        stub[cursor++] = 0xD0u; /* mov eax,edx */
        stub[cursor++] = 0xEBu;
        jmp_done = cursor++;
        zero_label = cursor;
        stub[cursor++] = 0x31u;
        stub[cursor++] = 0xC0u; /* xor eax,eax */
        done_label = cursor;
        stub[cursor++] = 0x5Au; /* pop edx */
    }
    stub[cursor++] = 0x9Du; /* popfd */
    stub[cursor++] = 0xE9u;
    if (!gu_make_rel32(
            stub + cursor - 1u, continuation, &relative)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    gu_write_u32(stub + cursor, relative);
    cursor += 4u;
    if (!gu_patch_rel8(stub, jle_zero, zero_label) ||
        !gu_patch_rel8(stub, jae_zero, zero_label) ||
        !gu_patch_rel8(stub, jbe_done, done_label) ||
        !gu_patch_rel8(stub, jmp_done, done_label) ||
        !VirtualProtect(
            stub, cursor, PAGE_EXECUTE_READ,
            &old_protection) ||
        !FlushInstructionCache(
            GetCurrentProcess(), stub, cursor)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

int gu_plant_count_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPlantCountContract *contract,
    float multiplier, BYTE **stub_out)
{
    BYTE *stub;
    if (!transaction || !image_base || !contract || !stub_out ||
        !gu_multiplier_valid(multiplier)) {
        return 0;
    }
    stub = gu_build_plant_count_stub(
        contract,
        image_base + contract->count_site.rva +
            contract->count_site.length);
    if (!stub) {
        return 0;
    }
    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &contract->count_site, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    /*
     * Keep the hot count hook behaviorally neutral until procedural.dat has
     * passed its exact catalog fingerprint and the coupled PLANT +0x2c
     * eligibility fields have been written and read back.  The definition
     * worker arms the requested multiplier only after that transaction.
     */
    gu_plant_count_reset_active_multiplier();
    *stub_out = stub;
    return 1;
}

void gu_plant_count_release_stub(BYTE *stub)
{
    if (stub) {
        VirtualFree(stub, 0u, MEM_RELEASE);
    }
}

int gu_plant_count_set_active_multiplier(float multiplier)
{
    DWORD bits;
    if (!gu_multiplier_valid(multiplier)) {
        return 0;
    }
    bits = *(const DWORD *)(const void *)&multiplier;
    InterlockedExchange(
        &g_gu_plant_density_multiplier_bits, (LONG)bits);
    return 1;
}

void gu_plant_count_reset_active_multiplier(void)
{
    InterlockedExchange(
        &g_gu_plant_density_multiplier_bits, 0x3F800000L);
}
