#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_hooks.h"
#include "universal_behavior_hooks.h"

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

static int gu_finalize_stub(BYTE *stub, DWORD size)
{
    DWORD old_protection = 0u;
    return stub && size != 0u &&
           VirtualProtect(
               stub, size, PAGE_EXECUTE_READ,
               &old_protection) != 0 &&
           FlushInstructionCache(
               GetCurrentProcess(), stub, size) != 0;
}

static int gu_emit_rel32(
    BYTE *stub, DWORD *cursor, BYTE opcode,
    const BYTE *destination)
{
    DWORD relative;
    DWORD instruction = *cursor;
    stub[(*cursor)++] = opcode;
    if (!gu_make_rel32(
            stub + instruction, destination, &relative)) {
        return 0;
    }
    gu_write_u32(stub + *cursor, relative);
    *cursor += 4u;
    return 1;
}

/* Preserve the complete x86 callback-visible machine state.  pushad/pushfd
 * alone do not protect MXCSR, the x87 environment/stack, or XMM0-7 at a
 * mid-function detour.  The 528-byte reservation always contains one aligned
 * 512-byte FXSAVE area without overlapping the integer save frame. */
static void gu_emit_callback_state_save(BYTE *stub, DWORD *cursor)
{
    stub[(*cursor)++] = 0x9Cu; /* pushfd */
    stub[(*cursor)++] = 0x60u; /* pushad */
    stub[(*cursor)++] = 0x81u;
    stub[(*cursor)++] = 0xECu; /* sub esp,528 */
    gu_write_u32(stub + *cursor, 528u);
    *cursor += 4u;
    stub[(*cursor)++] = 0x8Du;
    stub[(*cursor)++] = 0x44u;
    stub[(*cursor)++] = 0x24u;
    stub[(*cursor)++] = 0x0Fu; /* lea eax,[esp+15] */
    stub[(*cursor)++] = 0x83u;
    stub[(*cursor)++] = 0xE0u;
    stub[(*cursor)++] = 0xF0u; /* and eax,-16 */
    stub[(*cursor)++] = 0x0Fu;
    stub[(*cursor)++] = 0xAEu;
    stub[(*cursor)++] = 0x00u; /* fxsave [eax] */
    stub[(*cursor)++] = 0x8Bu;
    stub[(*cursor)++] = 0x84u;
    stub[(*cursor)++] = 0x24u;
    gu_write_u32(stub + *cursor, 556u);
    *cursor += 4u; /* mov eax,[esp+528+pushad.eax] */
    stub[(*cursor)++] = 0xFCu; /* cld: Win32 C ABI requires DF clear */
    /* Run C under the deterministic masked/default x87 and MXCSR state.
     * The exact interrupted environment remains in the FXSAVE image and is
     * restored after the callback.  This prevents an engine rounding mode or
     * pending unmasked exception from changing callback arithmetic. */
    stub[(*cursor)++] = 0xDBu;
    stub[(*cursor)++] = 0xE3u; /* fninit */
    stub[(*cursor)++] = 0x68u; /* push 0x00001F80 */
    gu_write_u32(stub + *cursor, 0x00001F80u);
    *cursor += 4u;
    stub[(*cursor)++] = 0x0Fu;
    stub[(*cursor)++] = 0xAEu;
    stub[(*cursor)++] = 0x14u;
    stub[(*cursor)++] = 0x24u; /* ldmxcsr [esp] */
    stub[(*cursor)++] = 0x83u;
    stub[(*cursor)++] = 0xC4u;
    stub[(*cursor)++] = 0x04u; /* add esp,4 */
}

static void gu_emit_callback_state_restore(BYTE *stub, DWORD *cursor)
{
    stub[(*cursor)++] = 0x8Du;
    stub[(*cursor)++] = 0x44u;
    stub[(*cursor)++] = 0x24u;
    stub[(*cursor)++] = 0x0Fu; /* lea eax,[esp+15] */
    stub[(*cursor)++] = 0x83u;
    stub[(*cursor)++] = 0xE0u;
    stub[(*cursor)++] = 0xF0u; /* and eax,-16 */
    stub[(*cursor)++] = 0x0Fu;
    stub[(*cursor)++] = 0xAEu;
    stub[(*cursor)++] = 0x08u; /* fxrstor [eax] */
    stub[(*cursor)++] = 0x81u;
    stub[(*cursor)++] = 0xC4u; /* add esp,528 */
    gu_write_u32(stub + *cursor, 528u);
    *cursor += 4u;
    stub[(*cursor)++] = 0x61u; /* popad */
    stub[(*cursor)++] = 0x9Du; /* popfd */
}

int gu_manager_prebuild_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuManagerPrebuildContract *contract,
    const void *fastcall_prebuild_hook,
    BYTE **stub_out)
{
    BYTE *stub;
    DWORD cursor = 0u;
    if (!transaction || !image_base || !contract ||
        !fastcall_prebuild_hook ||
        !stub_out ||
        contract->call_site.length != 5u ||
        !gu_patch_contract_validate(
            image_base, &contract->rebuild_entry)) {
        return 0;
    }
    stub = (BYTE *)VirtualAlloc(
        NULL, 128u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!stub) {
        return 0;
    }
    gu_emit_callback_state_save(stub, &cursor);
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = 0xCEu; /* mov ecx,esi */
    stub[cursor++] = 0x31u;
    stub[cursor++] = 0xD2u; /* xor edx,edx */
    if (!gu_emit_rel32(
            stub, &cursor, 0xE8u,
            (const BYTE *)fastcall_prebuild_hook)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_restore(stub, &cursor);
    if (!gu_emit_rel32(
            stub, &cursor, 0xE9u,
            image_base + contract->rebuild_entry.rva)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    if (!gu_finalize_stub(stub, cursor) ||
        !gu_patch_transaction_add_call(
            transaction, image_base,
            &contract->call_site, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    *stub_out = stub;
    return 1;
}

static int gu_emit_commit_success(
    BYTE *stub, DWORD *cursor, DWORD abi)
{
    if (abi == GU_COMMIT_ABI_CE_PLANT) {
        stub[(*cursor)++] = 0x47u; /* inc edi */
        stub[(*cursor)++] = 0x89u;
        stub[(*cursor)++] = 0x68u;
        stub[(*cursor)++] = 0x10u; /* mov [eax+10],ebp */
    } else if (abi == GU_COMMIT_ABI_PATCH_PLANT) {
        stub[(*cursor)++] = 0x83u;
        stub[(*cursor)++] = 0x44u;
        stub[(*cursor)++] = 0x24u;
        stub[(*cursor)++] = 0x4Cu;
        stub[(*cursor)++] = 0x01u;
        stub[(*cursor)++] = 0x89u;
        stub[(*cursor)++] = 0x78u;
        stub[(*cursor)++] = 0x10u; /* mov [eax+10],edi */
    } else if (abi == GU_COMMIT_ABI_CE_PROCOBJ) {
        stub[(*cursor)++] = 0x89u;
        stub[(*cursor)++] = 0x68u;
        stub[(*cursor)++] = 0x0Cu; /* mov [eax+0c],ebp */
    } else if (abi == GU_COMMIT_ABI_PATCH_PROCOBJ) {
        stub[(*cursor)++] = 0x89u;
        stub[(*cursor)++] = 0x58u;
        stub[(*cursor)++] = 0x0Cu; /* mov [eax+0c],ebx */
    } else {
        return 0;
    }
    return 1;
}

int gu_commit_hook_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuCommitHookContract *contract,
    const void *fastcall_commit_hook,
    BYTE **stub_out)
{
    BYTE *stub;
    DWORD cursor = 0u;
    DWORD null_jump;
    DWORD null_target;
    LONG short_relative;
    if (!transaction || !image_base || !contract ||
        !fastcall_commit_hook || !stub_out ||
        contract->site.length < 7u) {
        return 0;
    }
    stub = (BYTE *)VirtualAlloc(
        NULL, 128u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!stub) {
        return 0;
    }
    stub[cursor++] = 0x85u;
    stub[cursor++] = 0xC0u; /* test eax,eax */
    stub[cursor++] = 0x74u;
    null_jump = cursor++;
    if (!gu_emit_commit_success(stub, &cursor, contract->abi)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_save(stub, &cursor);
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = 0xC8u; /* mov ecx,eax */
    stub[cursor++] = 0x8Bu;
    if (contract->abi == GU_COMMIT_ABI_CE_PLANT ||
        contract->abi == GU_COMMIT_ABI_CE_PROCOBJ) {
        stub[cursor++] = 0xD5u; /* mov edx,ebp */
    } else if (contract->abi == GU_COMMIT_ABI_PATCH_PLANT) {
        stub[cursor++] = 0xD7u; /* mov edx,edi */
    } else {
        stub[cursor++] = 0xD3u; /* mov edx,ebx */
    }
    if (!gu_emit_rel32(
            stub, &cursor, 0xE8u,
            (const BYTE *)fastcall_commit_hook)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_restore(stub, &cursor);
    null_target = cursor;
    short_relative = (LONG)null_target -
                     (LONG)(null_jump + 1u);
    if (short_relative < -128 || short_relative > 127) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    stub[null_jump] = (BYTE)(char)short_relative;
    if (!gu_emit_rel32(
            stub, &cursor, 0xE9u,
            image_base + contract->site.rva +
                contract->site.length) ||
        !gu_finalize_stub(stub, cursor) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base,
            &contract->site, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    *stub_out = stub;
    return 1;
}

int gu_release_observer_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuReleaseObserverContract *contract,
    const void *fastcall_release_observer,
    BYTE **stub_out)
{
    BYTE *stub;
    DWORD cursor = 0u;
    BYTE second_mov_modrm;
    if (!transaction || !image_base || !contract ||
        !fastcall_release_observer || !stub_out ||
        contract->site.length != 6u ||
        contract->site.expected[0] != 0x8Bu ||
        contract->site.expected[1] != 0x46u ||
        contract->site.expected[2] != 0x08u ||
        contract->site.expected[3] != 0x8Bu ||
        contract->site.expected[5] != 0x28u ||
        !gu_patch_contract_validate(
            image_base, &contract->site)) {
        return 0;
    }
    if (contract->abi == GU_RELEASE_OBSERVER_ABI_CE) {
        second_mov_modrm = 0x40u; /* mov eax,[eax+28] */
    } else if (contract->abi == GU_RELEASE_OBSERVER_ABI_PATCH) {
        second_mov_modrm = 0x48u; /* mov ecx,[eax+28] */
    } else {
        return 0;
    }
    if (contract->site.expected[4] != second_mov_modrm) {
        return 0;
    }
    stub = (BYTE *)VirtualAlloc(
        NULL, 128u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!stub) {
        return 0;
    }
    gu_emit_callback_state_save(stub, &cursor);
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = 0xCEu; /* mov ecx,esi: exact wrapper */
    stub[cursor++] = 0x31u;
    stub[cursor++] = 0xD2u; /* xor edx,edx */
    if (!gu_emit_rel32(
            stub, &cursor, 0xE8u,
            (const BYTE *)fastcall_release_observer)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_restore(stub, &cursor);
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = 0x46u;
    stub[cursor++] = 0x08u; /* mov eax,[esi+08] */
    stub[cursor++] = 0x8Bu;
    stub[cursor++] = second_mov_modrm;
    stub[cursor++] = 0x28u; /* family-exact second MOV */
    if (!gu_emit_rel32(
            stub, &cursor, 0xE9u,
            image_base + contract->site.rva +
                contract->site.length) ||
        !gu_finalize_stub(stub, cursor) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base,
            &contract->site, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    *stub_out = stub;
    return 1;
}

int gu_definition_loaded_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPatchContract *call_contract,
    GuDefinitionLoadedCallback callback,
    BYTE **stub_out)
{
    BYTE *stub;
    BYTE *original_loader;
    DWORD cursor = 0u;
    DWORD encoded_relative;
    LONG original_relative;
    if (!transaction || !image_base || !call_contract ||
        !callback || !stub_out ||
        call_contract->length != 5u ||
        call_contract->expected[0] != 0xE8u ||
        !gu_patch_contract_validate(image_base, call_contract)) {
        return 0;
    }

    encoded_relative =
        (DWORD)call_contract->expected[1] |
        ((DWORD)call_contract->expected[2] << 8) |
        ((DWORD)call_contract->expected[3] << 16) |
        ((DWORD)call_contract->expected[4] << 24);
    original_relative = (LONG)encoded_relative;
    original_loader = image_base + call_contract->rva + 5u +
        original_relative;

    stub = (BYTE *)VirtualAlloc(
        NULL, 128u, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (!stub) {
        return 0;
    }
    /*
     * Replay the exact original loader call on GTA IV's calling thread.
     * Preserve its full integer return state while the no-argument callback
     * validates and scales the now-complete procedural definition catalog.
     */
    if (!gu_emit_rel32(
            stub, &cursor, 0xE8u, original_loader)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_save(stub, &cursor);
    if (!gu_emit_rel32(
            stub, &cursor, 0xE8u,
            (const BYTE *)(const void *)callback)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    gu_emit_callback_state_restore(stub, &cursor);
    stub[cursor++] = 0xC3u; /* ret to the original call continuation */

    if (!gu_finalize_stub(stub, cursor) ||
        !gu_patch_transaction_add_call(
            transaction, image_base, call_contract, stub)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return 0;
    }
    *stub_out = stub;
    return 1;
}

void gu_behavior_hook_release_stub(BYTE *stub)
{
    if (stub) {
        VirtualFree(stub, 0u, MEM_RELEASE);
    }
}
