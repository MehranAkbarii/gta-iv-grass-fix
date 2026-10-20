#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include "universal_source_bank.h"
#include "universal_pipeline.h"
#include "grass_compat.generated.h"

#define GU_SOURCE_TLS_BANK_DEPTH_MASK 0x00000007u
#define GU_SOURCE_TLS_BANK_INDEX_SHIFT 3u
#define GU_SOURCE_TLS_FALLBACK_SHIFT 11u
#define GU_SOURCE_TLS_FALLBACK_MAX 0x000FFFFFu
#define GU_SOURCE_STUB_BYTES 512u
#define GU_SOURCE_FP_SAVE_BYTES 528u
#define GU_SOURCE_FULL_FRAME_BYTES 564u
#define GU_SOURCE_SAVED_ESP 540u
#define GU_SOURCE_SAVED_EAX 556u

static DWORD g_gu_source_banks[GU_SOURCE_BANK_COUNT][
    GU_SOURCE_BANK_CAPACITY];
static volatile LONG g_gu_source_bank_leases[GU_SOURCE_BANK_COUNT];
static volatile LONG g_gu_source_bank_capacity =
    (LONG)GU_SOURCE_BANK_STOCK_CAPACITY;
static float g_gu_source_bank_radius_value = 30.0f;
static DWORD g_gu_source_bank_tls = TLS_OUT_OF_INDEXES;
static DWORD g_gu_source_bank_init_win32_error;
static volatile LONG g_gu_source_bank_init_state;
static volatile LONG g_gu_source_active_frames;
static volatile LONG g_gu_source_stock_fallback_frames;
static volatile LONG g_gu_source_bank_exhaustions;
static volatile LONG g_gu_source_bounds_refusals;
static volatile LONG g_gu_source_frame_mismatches;
static volatile LONG g_gu_source_tls_failures;
static struct GuSourcePrepareDiagnostic g_gu_source_prepare_diagnostic;

static struct GuSourceBankFamilyContract g_gu_source_family_ce;
static struct GuSourceBankFamilyContract g_gu_source_family_ce_rebased;
static struct GuSourceBankFamilyContract g_gu_source_family_leg;
static struct GuSourceBankFamilyContract g_gu_source_family_p8;
static struct GuSourceBankFamilyContract g_gu_source_family_p7;
static struct GuSourceBankFamilyContract g_gu_source_family_p4;
static volatile LONG g_gu_source_contract_state;

static const struct GuSourceBankTargetContract kGuSourceTargets[] = {
#include "grass_compat_source.generated.inc"
};

static void gu_source_copy(
    BYTE *destination, const BYTE *source, DWORD length)
{
    DWORD index;
    for (index = 0u; index < length; ++index) {
        destination[index] = source[index];
    }
}

static DWORD gu_source_read_u32(const BYTE *address)
{
    return (DWORD)address[0] |
           ((DWORD)address[1] << 8) |
           ((DWORD)address[2] << 16) |
           ((DWORD)address[3] << 24);
}

static void gu_source_write_u32(BYTE *address, DWORD value)
{
    address[0] = (BYTE)(value & 0xFFu);
    address[1] = (BYTE)((value >> 8) & 0xFFu);
    address[2] = (BYTE)((value >> 16) & 0xFFu);
    address[3] = (BYTE)((value >> 24) & 0xFFu);
}

static void gu_source_prepare_begin(
    const struct GuPatchTransaction *transaction, BYTE *image_base)
{
    ZeroMemory(
        &g_gu_source_prepare_diagnostic,
        sizeof(g_gu_source_prepare_diagnostic));
    g_gu_source_prepare_diagnostic.stage = GU_SOURCE_PREPARE_ARGUMENT;
    g_gu_source_prepare_diagnostic.site_index = 0xFFFFFFFFu;
    g_gu_source_prepare_diagnostic.first_mismatch_offset = 0xFFFFFFFFu;
    g_gu_source_prepare_diagnostic.tls_index = g_gu_source_bank_tls;
    if (image_base) {
        g_gu_source_prepare_diagnostic.image_rebase_delta =
            (DWORD)(ULONG_PTR)image_base - 0x00400000u;
    }
    if (transaction) {
        g_gu_source_prepare_diagnostic.transaction_count_before =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_count_partial =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_count_after =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_committed =
            transaction->committed;
    }
}

static int gu_source_prepare_fail(
    DWORD stage, const struct GuPatchTransaction *transaction)
{
    g_gu_source_prepare_diagnostic.stage = stage;
    g_gu_source_prepare_diagnostic.win32_error = ERROR_SUCCESS;
    g_gu_source_prepare_diagnostic.tls_index = g_gu_source_bank_tls;
    if (stage == GU_SOURCE_PREPARE_RUNTIME_INITIALIZE) {
        g_gu_source_prepare_diagnostic.win32_error =
            g_gu_source_bank_init_win32_error;
    } else if ((stage >= GU_SOURCE_PREPARE_ENTER_STUB &&
                stage <= GU_SOURCE_PREPARE_READER_STUB) ||
               (stage >= GU_SOURCE_PREPARE_ADD_ENTER &&
                stage <= GU_SOURCE_PREPARE_CONFIGURE)) {
        g_gu_source_prepare_diagnostic.win32_error = GetLastError();
    }
    if (transaction) {
        g_gu_source_prepare_diagnostic.transaction_count_partial =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_count_after =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_committed =
            transaction->committed;
    }
    return 0;
}

static void gu_source_prepare_after_discard(
    const struct GuPatchTransaction *transaction)
{
    if (transaction) {
        g_gu_source_prepare_diagnostic.transaction_count_after =
            transaction->count;
        g_gu_source_prepare_diagnostic.transaction_committed =
            transaction->committed;
    }
}

static int gu_source_materialize_expected(
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
    gu_source_copy(materialized, contract->expected, contract->length);
    delta = (DWORD)(ULONG_PTR)image_base - contract->preferred_image_base;
    for (index = 0u; index < contract->relocation_count; ++index) {
        DWORD offset = contract->relocation_offsets[index];
        if (offset > contract->length ||
            sizeof(DWORD) > contract->length - offset) {
            return 0;
        }
        gu_source_write_u32(
            materialized + offset,
            gu_source_read_u32(contract->expected + offset) + delta);
    }
    return 1;
}

static int gu_source_validate_site_with_diagnostic(
    BYTE *image_base, const struct GuPatchContract *contract,
    DWORD site_kind, DWORD site_index)
{
    BYTE materialized[GU_PATCH_MAX_BYTES];
    DWORD index;
    if (!gu_source_materialize_expected(
            image_base, contract, materialized)) {
        g_gu_source_prepare_diagnostic.site_kind = site_kind;
        g_gu_source_prepare_diagnostic.site_index = site_index;
        g_gu_source_prepare_diagnostic.site_rva =
            contract ? contract->rva : 0u;
        g_gu_source_prepare_diagnostic.site_length =
            contract ? contract->length : 0u;
        return 0;
    }
    for (index = 0u; index < contract->length; ++index) {
        if (image_base[contract->rva + index] != materialized[index]) {
            g_gu_source_prepare_diagnostic.site_kind = site_kind;
            g_gu_source_prepare_diagnostic.site_index = site_index;
            g_gu_source_prepare_diagnostic.site_rva = contract->rva;
            g_gu_source_prepare_diagnostic.site_length = contract->length;
            g_gu_source_prepare_diagnostic.first_mismatch_offset = index;
            g_gu_source_prepare_diagnostic.image_rebase_delta =
                contract->preferred_image_base == 0u ? 0u :
                (DWORD)(ULONG_PTR)image_base -
                    contract->preferred_image_base;
            gu_source_copy(
                g_gu_source_prepare_diagnostic.expected,
                materialized, contract->length);
            gu_source_copy(
                g_gu_source_prepare_diagnostic.observed,
                image_base + contract->rva, contract->length);
            return 0;
        }
    }
    return 1;
}

static int gu_source_ascii_equal(const char *left, const char *right)
{
    if (!left || !right) {
        return 0;
    }
    while (*left && *right && *left == *right) {
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static void gu_source_set_patch(
    struct GuPatchContract *contract,
    DWORD rva, const BYTE *bytes, DWORD length)
{
    ZeroMemory(contract, sizeof(*contract));
    if (!bytes || length == 0u || length > GU_PATCH_MAX_BYTES) {
        return;
    }
    contract->rva = rva;
    contract->length = length;
    gu_source_copy(contract->expected, bytes, length);
}

static void gu_source_set_one_relocation(
    struct GuPatchContract *contract,
    DWORD preferred_image_base, DWORD operand_offset)
{
    if (!contract || operand_offset > contract->length ||
        sizeof(DWORD) > contract->length - operand_offset) {
        if (contract) {
            contract->length = 0u;
        }
        return;
    }
    contract->preferred_image_base = preferred_image_base;
    contract->relocation_count = 1u;
    contract->relocation_offsets[0] = operand_offset;
}

static void gu_source_set_common(
    struct GuSourceBankFamilyContract *family,
    DWORD abi, DWORD critical_object_rva,
    DWORD leave_helper_rva, DWORD guard_stack_offset,
    DWORD writer_base_offset, DWORD reader_base_offset,
    DWORD writer_count_stack_offset,
    DWORD writer_count_register, DWORD cap_count_register)
{
    ZeroMemory(family, sizeof(*family));
    family->abi = abi;
    family->critical_object_rva = critical_object_rva;
    family->engine_leave_helper_rva = leave_helper_rva;
    family->enter_guard_stack_offset = guard_stack_offset;
    family->stock_writer_base_offset = writer_base_offset;
    family->stock_reader_base_offset = reader_base_offset;
    family->writer_count_stack_offset = writer_count_stack_offset;
    family->writer_count_register = writer_count_register;
    family->cap_count_register = cap_count_register;
}

static void gu_source_initialize_ce(
    struct GuSourceBankFamilyContract *family, int rebased)
{
    DWORD preferred_image_base = rebased ? 0x00D90000u : 0x00400000u;
    static const BYTE enter_post[] = {
        0x8Du, 0x8Cu, 0x24u, 0x1Cu, 0x02u, 0x00u, 0x00u
    };
    static const BYTE leave_call[] = {
        0xE8u, 0x8Du, 0x92u, 0x7Fu, 0xFFu
    };
    static const BYTE cap[] = {
        0x81u, 0xFDu, 0x80u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE writer[] = {
        0x89u, 0x7Cu, 0xACu, 0x1Cu, 0x45u
    };
    static const BYTE reader[] = {
        0xFFu, 0x74u, 0xB4u, 0x1Cu, 0x8Bu, 0xCBu
    };
    static const BYTE radius0[] = {
        0xC7u, 0x84u, 0x24u, 0x38u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE radius1[] = {
        0xC7u, 0x84u, 0x24u, 0x34u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE radius2[] = {
        0xC7u, 0x84u, 0x24u, 0x30u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE enter_context[] = {
        0xB9u, 0xB0u, 0xA8u, 0xB4u, 0x01u,
        0x89u, 0x6Cu, 0x24u, 0x10u,
        0xC6u, 0x84u, 0x24u, 0xC0u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0x48u, 0x94u, 0x7Fu, 0xFFu
    };
    static const BYTE enter_context_rebased[] = {
        0xB9u, 0xB0u, 0xA8u, 0x4Du, 0x02u,
        0x89u, 0x6Cu, 0x24u, 0x10u,
        0xC6u, 0x84u, 0x24u, 0xC0u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0x48u, 0x94u, 0x7Fu, 0xFFu
    };
    static const BYTE cleanup[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0xB0u, 0xA8u, 0xB4u, 0x01u,
        0xE8u, 0x8Du, 0x92u, 0x7Fu, 0xFFu
    };
    static const BYTE cleanup_rebased[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0xB0u, 0xA8u, 0x4Du, 0x02u,
        0xE8u, 0x8Du, 0x92u, 0x7Fu, 0xFFu
    };
    static const BYTE ret[] = {0xC3u};

    gu_source_set_common(
        family, GU_SOURCE_BANK_ABI_CE_LEG,
        0x0174A8B0u, 0x00003F90u, 0x000002C0u,
        0x1Cu, 0x1Cu, 0u,
        GU_SOURCE_BANK_REGISTER_EBP,
        GU_SOURCE_BANK_REGISTER_EBP);
    gu_source_set_patch(&family->enter_post_call,
                        0x0080AB18u, enter_post, sizeof(enter_post));
    gu_source_set_patch(&family->leave_call,
                        0x0080ACFEu, leave_call, sizeof(leave_call));
    gu_source_set_patch(&family->cap,
                        0x0080ABF0u, cap, sizeof(cap));
    gu_source_set_patch(&family->writer,
                        0x0080AD6Bu, writer, sizeof(writer));
    gu_source_set_patch(&family->reader,
                        0x0080ACA0u, reader, sizeof(reader));
    gu_source_set_patch(&family->radius_sites[0],
                        0x0080ABA0u, radius0, sizeof(radius0));
    gu_source_set_patch(&family->radius_sites[1],
                        0x0080ABABu, radius1, sizeof(radius1));
    gu_source_set_patch(&family->radius_sites[2],
                        0x0080ABB6u, radius2, sizeof(radius2));
    family->radius_site_count = 3u;
    family->radius_float_offset = 7u;
    family->radius_patch_kind = GU_RADIUS_PATCH_DIRECT_FLOAT;
    gu_source_set_patch(
        &family->proof_sites[0], 0x0080AB02u,
        rebased ? enter_context_rebased : enter_context,
        rebased ? sizeof(enter_context_rebased) : sizeof(enter_context));
    gu_source_set_patch(
        &family->proof_sites[1], 0x0080ACEDu,
        rebased ? cleanup_rebased : cleanup,
        rebased ? sizeof(cleanup_rebased) : sizeof(cleanup));
    gu_source_set_patch(&family->proof_sites[2],
                        0x0080AD17u, ret, sizeof(ret));
    gu_source_set_one_relocation(
        &family->proof_sites[0], preferred_image_base, 1u);
    gu_source_set_one_relocation(
        &family->proof_sites[1], preferred_image_base, 13u);
    family->proof_site_count = 3u;
}

static void gu_source_initialize_leg(void)
{
    struct GuSourceBankFamilyContract *family = &g_gu_source_family_leg;
    static const BYTE enter_post[] = {
        0x8Du, 0x8Cu, 0x24u, 0x1Cu, 0x02u, 0x00u, 0x00u
    };
    static const BYTE leave_call[] = {
        0xE8u, 0x7Du, 0x99u, 0x7Fu, 0xFFu
    };
    static const BYTE cap[] = {
        0x81u, 0xFDu, 0x80u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE writer[] = {
        0x89u, 0x7Cu, 0xACu, 0x1Cu, 0x45u
    };
    static const BYTE reader[] = {
        0xFFu, 0x74u, 0xB4u, 0x1Cu, 0x8Bu, 0xCBu
    };
    static const BYTE radius0[] = {
        0xC7u, 0x84u, 0x24u, 0x38u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE radius1[] = {
        0xC7u, 0x84u, 0x24u, 0x34u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE radius2[] = {
        0xC7u, 0x84u, 0x24u, 0x30u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xF0u, 0x41u
    };
    static const BYTE enter_context[] = {
        0xB9u, 0xB0u, 0xA8u, 0xB4u, 0x01u,
        0x89u, 0x6Cu, 0x24u, 0x10u,
        0xC6u, 0x84u, 0x24u, 0xC0u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0x38u, 0x9Bu, 0x7Fu, 0xFFu
    };
    static const BYTE cleanup[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0xB0u, 0xA8u, 0xB4u, 0x01u,
        0xE8u, 0x7Du, 0x99u, 0x7Fu, 0xFFu
    };
    static const BYTE ret[] = {0xC3u};

    gu_source_set_common(
        family, GU_SOURCE_BANK_ABI_CE_LEG,
        0x0174A8B0u, 0x00003F90u, 0x000002C0u,
        0x1Cu, 0x1Cu, 0u,
        GU_SOURCE_BANK_REGISTER_EBP,
        GU_SOURCE_BANK_REGISTER_EBP);
    gu_source_set_patch(&family->enter_post_call,
                        0x0080A428u, enter_post, sizeof(enter_post));
    gu_source_set_patch(&family->leave_call,
                        0x0080A60Eu, leave_call, sizeof(leave_call));
    gu_source_set_patch(&family->cap,
                        0x0080A500u, cap, sizeof(cap));
    gu_source_set_patch(&family->writer,
                        0x0080A67Bu, writer, sizeof(writer));
    gu_source_set_patch(&family->reader,
                        0x0080A5B0u, reader, sizeof(reader));
    gu_source_set_patch(&family->radius_sites[0],
                        0x0080A4B0u, radius0, sizeof(radius0));
    gu_source_set_patch(&family->radius_sites[1],
                        0x0080A4BBu, radius1, sizeof(radius1));
    gu_source_set_patch(&family->radius_sites[2],
                        0x0080A4C6u, radius2, sizeof(radius2));
    family->radius_site_count = 3u;
    family->radius_float_offset = 7u;
    family->radius_patch_kind = GU_RADIUS_PATCH_DIRECT_FLOAT;
    gu_source_set_patch(&family->proof_sites[0],
                        0x0080A412u, enter_context,
                        sizeof(enter_context));
    gu_source_set_patch(&family->proof_sites[1],
                        0x0080A5FDu, cleanup, sizeof(cleanup));
    gu_source_set_patch(&family->proof_sites[2],
                        0x0080A627u, ret, sizeof(ret));
    gu_source_set_one_relocation(
        &family->proof_sites[0], 0x00400000u, 1u);
    gu_source_set_one_relocation(
        &family->proof_sites[1], 0x00400000u, 13u);
    family->proof_site_count = 3u;
}

static void gu_source_initialize_patch_family(
    struct GuSourceBankFamilyContract *family,
    DWORD critical_object_rva,
    DWORD leave_helper_rva,
    DWORD enter_post_rva,
    DWORD leave_call_rva,
    const BYTE leave_call[5],
    DWORD cap_rva,
    DWORD writer_rva,
    DWORD reader_rva,
    DWORD radius_load_rva,
    const BYTE radius_load[8],
    const DWORD radius_store_rvas[3],
    const BYTE enter_context[22],
    DWORD enter_context_rva,
    const BYTE cleanup[23],
    DWORD cleanup_rva,
    DWORD ret_rva)
{
    static const BYTE enter_post[] = {
        0x83u, 0xC8u, 0xFFu, 0xBEu,
        0x07u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE cap[] = {
        0x81u, 0xFFu, 0x80u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE writer[] = {
        0x89u, 0x7Cu, 0x84u, 0x18u,
        0x83u, 0xC0u, 0x01u
    };
    static const BYTE reader[] = {
        0x8Bu, 0x4Cu, 0xB4u, 0x14u, 0x51u
    };
    static const BYTE radius_store0[] = {
        0xF3u, 0x0Fu, 0x11u, 0x84u, 0x24u,
        0x30u, 0x02u, 0x00u, 0x00u
    };
    static const BYTE radius_store1[] = {
        0xF3u, 0x0Fu, 0x11u, 0x84u, 0x24u,
        0x2Cu, 0x02u, 0x00u, 0x00u
    };
    static const BYTE radius_store2[] = {
        0xF3u, 0x0Fu, 0x11u, 0x84u, 0x24u,
        0x28u, 0x02u, 0x00u, 0x00u
    };
    static const BYTE ret[] = {0xC3u};

    gu_source_set_common(
        family, GU_SOURCE_BANK_ABI_PATCH_7_8,
        critical_object_rva, leave_helper_rva, 0x000002B8u,
        0x18u, 0x14u, 0u,
        GU_SOURCE_BANK_REGISTER_EAX,
        GU_SOURCE_BANK_REGISTER_EDI);
    gu_source_set_patch(&family->enter_post_call,
                        enter_post_rva, enter_post, sizeof(enter_post));
    gu_source_set_patch(&family->leave_call,
                        leave_call_rva, leave_call, 5u);
    gu_source_set_patch(&family->cap,
                        cap_rva, cap, sizeof(cap));
    gu_source_set_patch(&family->writer,
                        writer_rva, writer, sizeof(writer));
    gu_source_set_patch(&family->reader,
                        reader_rva, reader, sizeof(reader));
    gu_source_set_patch(&family->radius_sites[0],
                        radius_load_rva, radius_load, 8u);
    family->radius_site_count = 1u;
    family->radius_float_offset = 4u;
    family->radius_patch_kind = GU_RADIUS_PATCH_POINTER_OPERAND;
    gu_source_set_patch(&family->proof_sites[0],
                        enter_context_rva, enter_context, 22u);
    gu_source_set_patch(&family->proof_sites[1],
                        cleanup_rva, cleanup, 23u);
    gu_source_set_patch(&family->proof_sites[2],
                        ret_rva, ret, sizeof(ret));
    gu_source_set_patch(&family->proof_sites[3],
                        radius_store_rvas[0], radius_store0,
                        sizeof(radius_store0));
    gu_source_set_patch(&family->proof_sites[4],
                        radius_store_rvas[1], radius_store1,
                        sizeof(radius_store1));
    gu_source_set_patch(&family->proof_sites[5],
                        radius_store_rvas[2], radius_store2,
                        sizeof(radius_store2));
    gu_source_set_one_relocation(
        &family->radius_sites[0], 0x00400000u, 4u);
    gu_source_set_one_relocation(
        &family->proof_sites[0], 0x00400000u, 1u);
    gu_source_set_one_relocation(
        &family->proof_sites[1], 0x00400000u, 14u);
    family->proof_site_count = 6u;
}

static void gu_source_initialize_p8(void)
{
    static const BYTE leave_call[] = {
        0xE8u, 0xBCu, 0xC7u, 0xA5u, 0xFFu
    };
    static const BYTE radius_load[] = {
        0xF3u, 0x0Fu, 0x10u, 0x05u,
        0xB8u, 0xBFu, 0xD8u, 0x00u
    };
    static const DWORD radius_stores[] = {
        0x005FA42Du, 0x005FA436u, 0x005FA43Fu
    };
    static const BYTE enter_context[] = {
        0xB9u, 0xB8u, 0xF2u, 0x90u, 0x01u,
        0x89u, 0x7Cu, 0x24u, 0x0Cu,
        0xC6u, 0x84u, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0xBBu, 0xC9u, 0xA5u, 0xFFu
    };
    static const BYTE cleanup[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Fu, 0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0xB8u, 0xF2u, 0x90u, 0x01u,
        0xE8u, 0xBCu, 0xC7u, 0xA5u, 0xFFu
    };
    gu_source_initialize_patch_family(
        &g_gu_source_family_p8,
        0x0150F2B8u, 0x00056D90u,
        0x005FA3A5u, 0x005FA5CFu, leave_call,
        0x005FA470u, 0x005FA531u, 0x005FA570u,
        0x005FA424u, radius_load, radius_stores,
        enter_context, 0x005FA38Fu,
        cleanup, 0x005FA5BDu, 0x005FA5E8u);
}

static void gu_source_initialize_p7(void)
{
    static const BYTE leave_call[] = {
        0xE8u, 0x2Cu, 0xD2u, 0xC9u, 0xFFu
    };
    static const BYTE radius_load[] = {
        0xF3u, 0x0Fu, 0x10u, 0x05u,
        0x78u, 0x09u, 0xD5u, 0x00u
    };
    static const DWORD radius_stores[] = {
        0x0051EEBDu, 0x0051EEC6u, 0x0051EECFu
    };
    static const BYTE enter_context[] = {
        0xB9u, 0x6Cu, 0xA0u, 0x9Au, 0x01u,
        0x89u, 0x7Cu, 0x24u, 0x0Cu,
        0xC6u, 0x84u, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0x2Bu, 0xD4u, 0xC9u, 0xFFu
    };
    static const BYTE cleanup[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Fu, 0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0x6Cu, 0xA0u, 0x9Au, 0x01u,
        0xE8u, 0x2Cu, 0xD2u, 0xC9u, 0xFFu
    };
    gu_source_initialize_patch_family(
        &g_gu_source_family_p7,
        0x015AA06Cu, 0x001BC290u,
        0x0051EE35u, 0x0051F05Fu, leave_call,
        0x0051EF00u, 0x0051EFC1u, 0x0051F000u,
        0x0051EEB4u, radius_load, radius_stores,
        enter_context, 0x0051EE1Fu,
        cleanup, 0x0051F04Du, 0x0051F078u);
}

static void gu_source_initialize_p4(void)
{
    struct GuSourceBankFamilyContract *family = &g_gu_source_family_p4;
    static const BYTE enter_post[] = {
        0x83u, 0xC8u, 0xFFu, 0xBEu,
        0x07u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE leave_call[] = {
        0xE8u, 0x6Cu, 0x3Au, 0xB1u, 0xFFu
    };
    static const BYTE cap[] = {
        0x81u, 0xFFu, 0x80u, 0x00u, 0x00u, 0x00u
    };
    static const BYTE writer[] = {
        0x8Bu, 0x44u, 0x24u, 0x10u,
        0x89u, 0x7Cu, 0x84u, 0x18u,
        0x83u, 0xC0u, 0x01u,
        0x89u, 0x44u, 0x24u, 0x10u
    };
    static const BYTE reader[] = {
        0x8Bu, 0x4Cu, 0xB4u, 0x14u, 0x51u
    };
    static const BYTE radius_load[] = {
        0xF3u, 0x0Fu, 0x10u, 0x05u,
        0x8Cu, 0x15u, 0xD4u, 0x00u
    };
    static const BYTE enter_context[] = {
        0xB9u, 0xA8u, 0xF7u, 0x79u, 0x01u,
        0x89u, 0x7Cu, 0x24u, 0x0Cu,
        0xC6u, 0x84u, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x01u,
        0xE8u, 0x6Bu, 0x3Cu, 0xB1u, 0xFFu
    };
    static const BYTE cleanup[] = {
        0x80u, 0xBCu, 0x24u, 0xB8u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x5Fu, 0x5Eu, 0x5Bu, 0x74u, 0x0Au,
        0xB9u, 0xA8u, 0xF7u, 0x79u, 0x01u,
        0xE8u, 0x6Cu, 0x3Au, 0xB1u, 0xFFu
    };
    static const BYTE ret[] = {0xC3u};

    gu_source_set_common(
        family, GU_SOURCE_BANK_ABI_PATCH_4,
        0x0139F7A8u, 0x00053950u, 0x000002B8u,
        0x18u, 0x14u, 0x10u,
        GU_SOURCE_BANK_REGISTER_NONE,
        GU_SOURCE_BANK_REGISTER_EDI);
    gu_source_set_patch(&family->enter_post_call,
                        0x0053FCB5u, enter_post, sizeof(enter_post));
    gu_source_set_patch(&family->leave_call,
                        0x0053FEDFu, leave_call, sizeof(leave_call));
    gu_source_set_patch(&family->cap,
                        0x0053FD80u, cap, sizeof(cap));
    gu_source_set_patch(&family->writer,
                        0x0053FE3Du, writer, sizeof(writer));
    gu_source_set_patch(&family->reader,
                        0x0053FE80u, reader, sizeof(reader));
    gu_source_set_patch(&family->radius_sites[0],
                        0x0053FD34u, radius_load, sizeof(radius_load));
    family->radius_site_count = 1u;
    family->radius_float_offset = 4u;
    family->radius_patch_kind = GU_RADIUS_PATCH_POINTER_OPERAND;
    gu_source_set_patch(&family->proof_sites[0],
                        0x0053FC9Fu, enter_context,
                        sizeof(enter_context));
    gu_source_set_patch(&family->proof_sites[1],
                        0x0053FECDu, cleanup, sizeof(cleanup));
    gu_source_set_patch(&family->proof_sites[2],
                        0x0053FEF8u, ret, sizeof(ret));
    gu_source_set_one_relocation(
        &family->radius_sites[0], 0x00400000u, 4u);
    gu_source_set_one_relocation(
        &family->proof_sites[0], 0x00400000u, 1u);
    gu_source_set_one_relocation(
        &family->proof_sites[1], 0x00400000u, 14u);
    family->proof_site_count = 3u;
}

static void gu_source_contracts_initialize(void)
{
    LONG state = InterlockedCompareExchange(
        &g_gu_source_contract_state, 1, 0);
    if (state == 0) {
        gu_source_initialize_ce(&g_gu_source_family_ce, 0);
        gu_source_initialize_ce(&g_gu_source_family_ce_rebased, 1);
        gu_source_initialize_leg();
        gu_source_initialize_p8();
        gu_source_initialize_p7();
        gu_source_initialize_p4();
        InterlockedExchange(&g_gu_source_contract_state, 2);
        return;
    }
    while (InterlockedCompareExchange(
               &g_gu_source_contract_state, 0, 0) == 1) {
        Sleep(0u);
    }
}

DWORD gu_source_bank_target_contract_count(void)
{
    gu_source_contracts_initialize();
    return (DWORD)(sizeof(kGuSourceTargets) /
                   sizeof(kGuSourceTargets[0]));
}

const struct GuSourceBankTargetContract *gu_source_bank_target_contract_at(
    DWORD index)
{
    gu_source_contracts_initialize();
    if (index >= gu_source_bank_target_contract_count()) {
        return NULL;
    }
    return &kGuSourceTargets[index];
}

const struct GuSourceBankTargetContract *gu_source_bank_contract_for_identity(
    const char *target_id, const char *sha256)
{
    DWORD index;
    const struct GuSourceBankTargetContract *match = NULL;
    DWORD matches = 0u;
    gu_source_contracts_initialize();
    for (index = 0u; index < gu_source_bank_target_contract_count(); ++index) {
        const struct GuSourceBankTargetContract *candidate =
            &kGuSourceTargets[index];
        if (gu_source_ascii_equal(candidate->target_id, target_id) &&
            gu_source_ascii_equal(candidate->sha256, sha256)) {
            match = candidate;
            ++matches;
        }
    }
    return matches == 1u ? match : NULL;
}

static int gu_source_float_multiplier_valid(float multiplier)
{
    DWORD bits = *(const DWORD *)(const void *)&multiplier;
    return (bits & 0x7F800000u) != 0x7F800000u &&
           multiplier >= GU_DISTANCE_MULTIPLIER_MIN &&
           multiplier <= GU_DISTANCE_MULTIPLIER_MAX;
}

static DWORD gu_source_tls_raw(void)
{
    ULONG_PTR encoded;
    if (g_gu_source_bank_tls == TLS_OUT_OF_INDEXES) {
        return 0u;
    }
    encoded = (ULONG_PTR)TlsGetValue(g_gu_source_bank_tls);
    return encoded == 0u ? 0u : (DWORD)(encoded - 1u);
}

static int gu_source_tls_set_raw(DWORD raw)
{
    if (g_gu_source_bank_tls == TLS_OUT_OF_INDEXES) {
        return 0;
    }
    return TlsSetValue(
        g_gu_source_bank_tls,
        (LPVOID)(ULONG_PTR)(raw + 1u)) != 0;
}

static DWORD gu_source_bank_depth(DWORD raw)
{
    return raw & GU_SOURCE_TLS_BANK_DEPTH_MASK;
}

static DWORD gu_source_fallback_depth(DWORD raw)
{
    return raw >> GU_SOURCE_TLS_FALLBACK_SHIFT;
}

static DWORD gu_source_top_bank(DWORD raw)
{
    DWORD depth = gu_source_bank_depth(raw);
    if (depth == 0u || depth > GU_SOURCE_BANK_COUNT) {
        return GU_SOURCE_BANK_COUNT;
    }
    return (raw >> (GU_SOURCE_TLS_BANK_INDEX_SHIFT +
                    ((depth - 1u) * 2u))) & 3u;
}

static int gu_source_push_fallback(DWORD raw)
{
    DWORD fallback = gu_source_fallback_depth(raw);
    if (fallback >= GU_SOURCE_TLS_FALLBACK_MAX) {
        /* Saturation is deliberately sticky and remains STOCK/fail-closed. */
        InterlockedIncrement(&g_gu_source_tls_failures);
        return 0;
    }
    raw += 1u << GU_SOURCE_TLS_FALLBACK_SHIFT;
    if (!gu_source_tls_set_raw(raw)) {
        InterlockedIncrement(&g_gu_source_tls_failures);
        return 0;
    }
    InterlockedIncrement(&g_gu_source_stock_fallback_frames);
    InterlockedIncrement(&g_gu_source_active_frames);
    return 1;
}

int gu_source_bank_initialize(void)
{
    LONG state;
    DWORD tls_index;
    DWORD index;
    if (sizeof(void *) != 4u) {
        return 0;
    }
    state = InterlockedCompareExchange(
        &g_gu_source_bank_init_state, 1, 0);
    if (state == 0) {
        g_gu_source_bank_init_win32_error = ERROR_SUCCESS;
        tls_index = TlsAlloc();
        /*
         * Any index other than TLS_OUT_OF_INDEXES is a valid Win32 TLS slot.
         * BLD0045 incorrectly rejected expansion slots (index 64+), making
         * startup depend on unrelated modules' prior TLS allocations.  The
         * hot path already checks every TlsSetValue and falls back to STOCK on
         * failure, so accepting the OS-provided expansion range is fail-safe.
         */
        if (tls_index == TLS_OUT_OF_INDEXES) {
            g_gu_source_bank_init_win32_error = GetLastError();
            InterlockedExchange(&g_gu_source_bank_init_state, 0);
            return 0;
        }
        g_gu_source_bank_tls = tls_index;
        for (index = 0u; index < GU_SOURCE_BANK_COUNT; ++index) {
            InterlockedExchange(&g_gu_source_bank_leases[index], 0);
        }
        InterlockedExchange(
            &g_gu_source_bank_capacity,
            (LONG)GU_SOURCE_BANK_STOCK_CAPACITY);
        g_gu_source_bank_radius_value = 30.0f;
        InterlockedExchange(&g_gu_source_active_frames, 0);
        InterlockedExchange(&g_gu_source_stock_fallback_frames, 0);
        InterlockedExchange(&g_gu_source_bank_exhaustions, 0);
        InterlockedExchange(&g_gu_source_bounds_refusals, 0);
        InterlockedExchange(&g_gu_source_frame_mismatches, 0);
        InterlockedExchange(&g_gu_source_tls_failures, 0);
        InterlockedExchange(&g_gu_source_bank_init_state, 2);
        return 1;
    }
    while ((state = InterlockedCompareExchange(
                &g_gu_source_bank_init_state, 0, 0)) == 1) {
        Sleep(0u);
    }
    return state == 2;
}

int gu_source_bank_shutdown(void)
{
    DWORD index;
    DWORD tls_index;
    if (InterlockedCompareExchange(
            &g_gu_source_bank_init_state, 3, 2) != 2) {
        return InterlockedCompareExchange(
                   &g_gu_source_bank_init_state, 0, 0) == 0;
    }
    if (gu_source_tls_raw() != 0u ||
        InterlockedCompareExchange(
            &g_gu_source_active_frames, 0, 0) != 0) {
        InterlockedExchange(&g_gu_source_bank_init_state, 2);
        return 0;
    }
    for (index = 0u; index < GU_SOURCE_BANK_COUNT; ++index) {
        if (InterlockedCompareExchange(
                &g_gu_source_bank_leases[index], 0, 0) != 0) {
            InterlockedExchange(&g_gu_source_bank_init_state, 2);
            return 0;
        }
    }
    tls_index = g_gu_source_bank_tls;
    g_gu_source_bank_tls = TLS_OUT_OF_INDEXES;
    if (tls_index != TLS_OUT_OF_INDEXES && !TlsFree(tls_index)) {
        g_gu_source_bank_tls = tls_index;
        InterlockedExchange(&g_gu_source_bank_init_state, 2);
        return 0;
    }
    InterlockedExchange(
        &g_gu_source_bank_capacity,
        (LONG)GU_SOURCE_BANK_STOCK_CAPACITY);
    g_gu_source_bank_radius_value = 30.0f;
    g_gu_source_bank_init_win32_error = ERROR_SUCCESS;
    InterlockedExchange(&g_gu_source_bank_init_state, 0);
    return 1;
}

int gu_source_bank_configure(
    DWORD capacity, float distance_multiplier)
{
    DWORD index;
    float radius;
    DWORD radius_bits;
    if (!gu_source_bank_initialize() ||
        capacity <= GU_SOURCE_BANK_STOCK_CAPACITY ||
        capacity > GU_SOURCE_BANK_CAPACITY ||
        !gu_source_float_multiplier_valid(distance_multiplier) ||
        gu_source_tls_raw() != 0u) {
        return 0;
    }
    for (index = 0u; index < GU_SOURCE_BANK_COUNT; ++index) {
        if (InterlockedCompareExchange(
                &g_gu_source_bank_leases[index], 0, 0) != 0) {
            return 0;
        }
    }
    radius = 30.0f * distance_multiplier;
    if (!gu_source_float_multiplier_valid(radius / 30.0f)) {
        return 0;
    }
    radius_bits = *(DWORD *)(void *)&radius;
    InterlockedExchange(
        (volatile LONG *)(void *)&g_gu_source_bank_radius_value,
        (LONG)radius_bits);
    InterlockedExchange(
        &g_gu_source_bank_capacity, (LONG)capacity);
    return 1;
}

int __stdcall gu_source_bank_enter_frame(DWORD engine_lock_entered)
{
    DWORD raw;
    DWORD depth;
    DWORD thread_id;
    DWORD index;
    /* Initialization is a prepare-time obligation, never hot-path work. */
    if (InterlockedCompareExchange(
            &g_gu_source_bank_init_state, 0, 0) != 2 ||
        g_gu_source_bank_tls == TLS_OUT_OF_INDEXES) {
        return 0;
    }
    /*
     * The stock guard-zero path can skip the outer Leave helper entirely.
     * It therefore uses implicit STOCK mode and must not create a frame that
     * no normal cleanup site will balance.
     */
    if (engine_lock_entered == 0u) {
        return 0;
    }
    raw = gu_source_tls_raw();
    depth = gu_source_bank_depth(raw);
    if (depth > GU_SOURCE_BANK_COUNT ||
        gu_source_fallback_depth(raw) != 0u ||
        depth >= GU_SOURCE_BANK_COUNT) {
        if (depth > GU_SOURCE_BANK_COUNT ||
            depth >= GU_SOURCE_BANK_COUNT) {
            InterlockedIncrement(&g_gu_source_bank_exhaustions);
        }
        gu_source_push_fallback(raw);
        return 0;
    }
    thread_id = GetCurrentThreadId();
    if (thread_id == 0u) {
        gu_source_push_fallback(raw);
        return 0;
    }
    for (index = 0u; index < GU_SOURCE_BANK_COUNT; ++index) {
        DWORD shift;
        DWORD updated;
        if ((DWORD)InterlockedCompareExchange(
                &g_gu_source_bank_leases[index],
                (LONG)thread_id, 0) != 0u) {
            continue;
        }
        shift = GU_SOURCE_TLS_BANK_INDEX_SHIFT + depth * 2u;
        updated = raw | (index << shift);
        updated = (updated & ~GU_SOURCE_TLS_BANK_DEPTH_MASK) |
                  (depth + 1u);
        if (gu_source_tls_set_raw(updated)) {
            InterlockedIncrement(&g_gu_source_active_frames);
            return 1;
        }
        InterlockedCompareExchange(
            &g_gu_source_bank_leases[index], 0, (LONG)thread_id);
        InterlockedIncrement(&g_gu_source_tls_failures);
        gu_source_push_fallback(raw);
        return 0;
    }
    InterlockedIncrement(&g_gu_source_bank_exhaustions);
    gu_source_push_fallback(raw);
    return 0;
}

int __stdcall gu_source_bank_leave_frame(void)
{
    DWORD raw;
    DWORD fallback;
    DWORD depth;
    DWORD bank;
    DWORD shift;
    DWORD updated;
    DWORD thread_id;
    LONG released;
    if (InterlockedCompareExchange(
            &g_gu_source_bank_init_state, 0, 0) != 2) {
        return 0;
    }
    raw = gu_source_tls_raw();
    fallback = gu_source_fallback_depth(raw);
    if (fallback != 0u) {
        if (!gu_source_tls_set_raw(
                raw - (1u << GU_SOURCE_TLS_FALLBACK_SHIFT))) {
            InterlockedIncrement(&g_gu_source_tls_failures);
            return 0;
        }
        InterlockedDecrement(&g_gu_source_active_frames);
        return 1;
    }
    depth = gu_source_bank_depth(raw);
    if (depth == 0u || depth > GU_SOURCE_BANK_COUNT) {
        InterlockedIncrement(&g_gu_source_frame_mismatches);
        return 0;
    }
    bank = gu_source_top_bank(raw);
    if (bank >= GU_SOURCE_BANK_COUNT) {
        InterlockedIncrement(&g_gu_source_frame_mismatches);
        return 0;
    }
    thread_id = GetCurrentThreadId();
    shift = GU_SOURCE_TLS_BANK_INDEX_SHIFT + (depth - 1u) * 2u;
    updated = raw & ~(3u << shift);
    updated = (updated & ~GU_SOURCE_TLS_BANK_DEPTH_MASK) | (depth - 1u);
    /*
     * Pop the owning TLS frame first.  If that clear fails, retain the lease
     * so a later retry cannot expose this bank to another invocation.
     */
    if (!gu_source_tls_set_raw(updated)) {
        InterlockedIncrement(&g_gu_source_tls_failures);
        return 0;
    }
    InterlockedDecrement(&g_gu_source_active_frames);
    released = InterlockedCompareExchange(
        &g_gu_source_bank_leases[bank], 0, (LONG)thread_id);
    if ((DWORD)released != thread_id) {
        InterlockedIncrement(&g_gu_source_frame_mismatches);
    }
    return (DWORD)released == thread_id;
}

DWORD *gu_source_bank_active_buffer(void)
{
    DWORD raw;
    DWORD bank;
    DWORD thread_id;
    if (InterlockedCompareExchange(
            &g_gu_source_bank_init_state, 0, 0) != 2) {
        return NULL;
    }
    raw = gu_source_tls_raw();
    if (gu_source_fallback_depth(raw) != 0u) {
        return NULL;
    }
    bank = gu_source_top_bank(raw);
    if (bank >= GU_SOURCE_BANK_COUNT) {
        return NULL;
    }
    thread_id = GetCurrentThreadId();
    if ((DWORD)InterlockedCompareExchange(
            &g_gu_source_bank_leases[bank], 0, 0) != thread_id) {
        return NULL;
    }
    return g_gu_source_banks[bank];
}

DWORD gu_source_bank_active_capacity(void)
{
    LONG capacity;
    if (!gu_source_bank_active_buffer()) {
        return GU_SOURCE_BANK_STOCK_CAPACITY;
    }
    capacity = InterlockedCompareExchange(
        &g_gu_source_bank_capacity, 0, 0);
    if (capacity <= (LONG)GU_SOURCE_BANK_STOCK_CAPACITY ||
        capacity > (LONG)GU_SOURCE_BANK_CAPACITY) {
        return GU_SOURCE_BANK_STOCK_CAPACITY;
    }
    return (DWORD)capacity;
}

float gu_source_bank_radius(void)
{
    DWORD bits = (DWORD)InterlockedCompareExchange(
        (volatile LONG *)(void *)&g_gu_source_bank_radius_value, 0, 0);
    return *(float *)(void *)&bits;
}

void gu_source_bank_get_counters(
    struct GuSourceBankCounters *counters)
{
    if (!counters) {
        return;
    }
    counters->active_frames =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_active_frames, 0, 0);
    counters->stock_fallback_frames =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_stock_fallback_frames, 0, 0);
    counters->bank_exhaustions =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_bank_exhaustions, 0, 0);
    counters->bounds_refusals =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_bounds_refusals, 0, 0);
    counters->frame_mismatches =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_frame_mismatches, 0, 0);
    counters->tls_failures =
        (DWORD)InterlockedCompareExchange(
            &g_gu_source_tls_failures, 0, 0);
}

void gu_source_bank_get_prepare_diagnostic(
    struct GuSourcePrepareDiagnostic *diagnostic)
{
    if (!diagnostic) {
        return;
    }
    *diagnostic = g_gu_source_prepare_diagnostic;
}

const char *gu_source_prepare_stage_name(DWORD stage)
{
    switch (stage) {
    case GU_SOURCE_PREPARE_NOT_RUN: return "NOT_RUN";
    case GU_SOURCE_PREPARE_ARGUMENT: return "ARGUMENT";
    case GU_SOURCE_PREPARE_TARGET_IDENTITY: return "TARGET_IDENTITY";
    case GU_SOURCE_PREPARE_RUNTIME_INITIALIZE: return "RUNTIME_INITIALIZE";
    case GU_SOURCE_PREPARE_CONTRACT_SHAPE: return "CONTRACT_SHAPE";
    case GU_SOURCE_PREPARE_TRANSACTION_CAPACITY:
        return "TRANSACTION_CAPACITY";
    case GU_SOURCE_PREPARE_LIVE_PREIMAGE: return "LIVE_PREIMAGE_CONFLICT";
    case GU_SOURCE_PREPARE_ENTER_STUB: return "ENTER_STUB";
    case GU_SOURCE_PREPARE_LEAVE_STUB: return "LEAVE_STUB";
    case GU_SOURCE_PREPARE_CAP_STUB: return "CAP_STUB";
    case GU_SOURCE_PREPARE_WRITER_STUB: return "WRITER_STUB";
    case GU_SOURCE_PREPARE_READER_STUB: return "READER_STUB";
    case GU_SOURCE_PREPARE_ADD_ENTER: return "ADD_ENTER";
    case GU_SOURCE_PREPARE_ADD_LEAVE: return "ADD_LEAVE";
    case GU_SOURCE_PREPARE_ADD_CAP: return "ADD_CAP";
    case GU_SOURCE_PREPARE_ADD_WRITER: return "ADD_WRITER";
    case GU_SOURCE_PREPARE_ADD_READER: return "ADD_READER";
    case GU_SOURCE_PREPARE_ADD_RADIUS: return "ADD_RADIUS";
    case GU_SOURCE_PREPARE_CONFIGURE: return "CONFIGURE";
    case GU_SOURCE_PREPARE_COMPLETE: return "COMPLETE";
    default: return "UNKNOWN";
    }
}

const char *gu_source_prepare_site_name(DWORD site_kind)
{
    switch (site_kind) {
    case GU_SOURCE_PREPARE_SITE_NONE: return "NONE";
    case GU_SOURCE_PREPARE_SITE_ENTER_POST_CALL: return "ENTER_POST_CALL";
    case GU_SOURCE_PREPARE_SITE_LEAVE_CALL: return "LEAVE_CALL";
    case GU_SOURCE_PREPARE_SITE_CAP: return "CAP";
    case GU_SOURCE_PREPARE_SITE_WRITER: return "WRITER";
    case GU_SOURCE_PREPARE_SITE_READER: return "READER";
    case GU_SOURCE_PREPARE_SITE_RADIUS: return "RADIUS";
    case GU_SOURCE_PREPARE_SITE_PROOF: return "PROOF";
    default: return "UNKNOWN";
    }
}

int __stdcall gu_source_bank_append(
    DWORD pointer_value, DWORD index, DWORD *stock_buffer)
{
    DWORD *active = gu_source_bank_active_buffer();
    DWORD capacity;
    if (active) {
        capacity = gu_source_bank_active_capacity();
        if (capacity <= GU_SOURCE_BANK_STOCK_CAPACITY ||
            capacity > GU_SOURCE_BANK_CAPACITY || index >= capacity) {
            InterlockedIncrement(&g_gu_source_bounds_refusals);
            return 0;
        }
        active[index] = pointer_value;
        return 1;
    }
    if (!stock_buffer || index >= GU_SOURCE_BANK_STOCK_CAPACITY) {
        InterlockedIncrement(&g_gu_source_bounds_refusals);
        return 0;
    }
    stock_buffer[index] = pointer_value;
    return 1;
}

DWORD __stdcall gu_source_bank_read(
    DWORD index, const DWORD *stock_buffer)
{
    DWORD *active = gu_source_bank_active_buffer();
    DWORD capacity;
    if (active) {
        capacity = gu_source_bank_active_capacity();
        if (capacity <= GU_SOURCE_BANK_STOCK_CAPACITY ||
            capacity > GU_SOURCE_BANK_CAPACITY || index >= capacity) {
            InterlockedIncrement(&g_gu_source_bounds_refusals);
            return 0u;
        }
        return active[index];
    }
    if (!stock_buffer || index >= GU_SOURCE_BANK_STOCK_CAPACITY) {
        InterlockedIncrement(&g_gu_source_bounds_refusals);
        return 0u;
    }
    return stock_buffer[index];
}

#if defined(GTAIV_GRASS_SOURCE_BANK_TESTING)
DWORD gu_source_bank_test_lease_owner(DWORD bank_index)
{
    if (bank_index >= GU_SOURCE_BANK_COUNT) {
        return 0u;
    }
    return (DWORD)InterlockedCompareExchange(
        &g_gu_source_bank_leases[bank_index], 0, 0);
}

DWORD gu_source_bank_test_frame_depth(void)
{
    return gu_source_bank_depth(gu_source_tls_raw());
}

DWORD gu_source_bank_test_fallback_depth(void)
{
    return gu_source_fallback_depth(gu_source_tls_raw());
}

DWORD gu_source_bank_test_tls_index(void)
{
    return g_gu_source_bank_tls;
}
#endif

struct GuSourceCodeBuilder {
    BYTE *code;
    DWORD capacity;
    DWORD cursor;
    int ok;
};

static BYTE *gu_source_allocate_stub(void)
{
    return (BYTE *)VirtualAlloc(
        NULL, GU_SOURCE_STUB_BYTES,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void gu_source_emit_byte(
    struct GuSourceCodeBuilder *builder, BYTE value)
{
    if (!builder || !builder->ok ||
        builder->cursor >= builder->capacity) {
        if (builder) {
            builder->ok = 0;
        }
        return;
    }
    builder->code[builder->cursor++] = value;
}

static void gu_source_emit_u32(
    struct GuSourceCodeBuilder *builder, DWORD value)
{
    gu_source_emit_byte(builder, (BYTE)(value & 0xFFu));
    gu_source_emit_byte(builder, (BYTE)((value >> 8) & 0xFFu));
    gu_source_emit_byte(builder, (BYTE)((value >> 16) & 0xFFu));
    gu_source_emit_byte(builder, (BYTE)((value >> 24) & 0xFFu));
}

static void gu_source_emit_bytes(
    struct GuSourceCodeBuilder *builder,
    const BYTE *bytes, DWORD length)
{
    DWORD index;
    if (!bytes) {
        builder->ok = 0;
        return;
    }
    for (index = 0u; index < length; ++index) {
        gu_source_emit_byte(builder, bytes[index]);
    }
}

static void gu_source_emit_absolute_call(
    struct GuSourceCodeBuilder *builder, const void *target)
{
    /* mov eax,imm32; call eax */
    gu_source_emit_byte(builder, 0xB8u);
    gu_source_emit_u32(builder, (DWORD)(ULONG_PTR)target);
    gu_source_emit_byte(builder, 0xFFu);
    gu_source_emit_byte(builder, 0xD0u);
}

static void gu_source_emit_jump(
    struct GuSourceCodeBuilder *builder, const BYTE *target)
{
    DWORD next;
    DWORD relative;
    gu_source_emit_byte(builder, 0xE9u);
    next = (DWORD)(ULONG_PTR)(
        builder->code + builder->cursor + 4u);
    relative = (DWORD)(ULONG_PTR)target - next;
    gu_source_emit_u32(builder, relative);
}

/* Source-bank detours run in the middle of engine functions.  Protect the
 * complete x86 callback-visible state, not only GPRs: x87/MMX, MXCSR and all
 * XMM0-7 registers are part of the interrupted computation.  The 528-byte
 * reservation always contains one aligned 512-byte FXSAVE area. */
static void gu_source_emit_callback_state_save(
    struct GuSourceCodeBuilder *builder)
{
    gu_source_emit_byte(builder, 0x9Cu); /* pushfd */
    gu_source_emit_byte(builder, 0x60u); /* pushad */
    gu_source_emit_byte(builder, 0x81u);
    gu_source_emit_byte(builder, 0xECu); /* sub esp,528 */
    gu_source_emit_u32(builder, GU_SOURCE_FP_SAVE_BYTES);
    gu_source_emit_byte(builder, 0x8Du);
    gu_source_emit_byte(builder, 0x44u);
    gu_source_emit_byte(builder, 0x24u);
    gu_source_emit_byte(builder, 0x0Fu); /* lea eax,[esp+15] */
    gu_source_emit_byte(builder, 0x83u);
    gu_source_emit_byte(builder, 0xE0u);
    gu_source_emit_byte(builder, 0xF0u); /* and eax,-16 */
    gu_source_emit_byte(builder, 0x0Fu);
    gu_source_emit_byte(builder, 0xAEu);
    gu_source_emit_byte(builder, 0x00u); /* fxsave [eax] */
    gu_source_emit_byte(builder, 0xFCu); /* cld: Win32 C ABI requires DF clear */
    /* Execute C under deterministic masked/default floating-point state.
     * FXRSTOR below reinstates the exact interrupted environment. */
    gu_source_emit_byte(builder, 0xDBu);
    gu_source_emit_byte(builder, 0xE3u); /* fninit */
    gu_source_emit_byte(builder, 0x68u); /* push 0x00001F80 */
    gu_source_emit_u32(builder, 0x00001F80u);
    gu_source_emit_byte(builder, 0x0Fu);
    gu_source_emit_byte(builder, 0xAEu);
    gu_source_emit_byte(builder, 0x14u);
    gu_source_emit_byte(builder, 0x24u); /* ldmxcsr [esp] */
    gu_source_emit_byte(builder, 0x83u);
    gu_source_emit_byte(builder, 0xC4u);
    gu_source_emit_byte(builder, 0x04u); /* add esp,4 */
}

static void gu_source_emit_callback_state_restore(
    struct GuSourceCodeBuilder *builder)
{
    gu_source_emit_byte(builder, 0x8Du);
    gu_source_emit_byte(builder, 0x44u);
    gu_source_emit_byte(builder, 0x24u);
    gu_source_emit_byte(builder, 0x0Fu); /* lea eax,[esp+15] */
    gu_source_emit_byte(builder, 0x83u);
    gu_source_emit_byte(builder, 0xE0u);
    gu_source_emit_byte(builder, 0xF0u); /* and eax,-16 */
    gu_source_emit_byte(builder, 0x0Fu);
    gu_source_emit_byte(builder, 0xAEu);
    gu_source_emit_byte(builder, 0x08u); /* fxrstor [eax] */
    gu_source_emit_byte(builder, 0x81u);
    gu_source_emit_byte(builder, 0xC4u); /* add esp,528 */
    gu_source_emit_u32(builder, GU_SOURCE_FP_SAVE_BYTES);
    gu_source_emit_byte(builder, 0x61u); /* popad */
    gu_source_emit_byte(builder, 0x9Du); /* popfd */
}

static void gu_source_emit_store_eax_esp_offset(
    struct GuSourceCodeBuilder *builder, DWORD offset)
{
    gu_source_emit_byte(builder, 0x89u);
    gu_source_emit_byte(builder, 0x84u);
    gu_source_emit_byte(builder, 0x24u);
    gu_source_emit_u32(builder, offset);
}

static int gu_source_patch_short_branch(
    struct GuSourceCodeBuilder *builder,
    DWORD displacement_offset, DWORD target_offset)
{
    LONG displacement;
    if (!builder || !builder->ok ||
        displacement_offset >= builder->cursor) {
        return 0;
    }
    displacement = (LONG)target_offset -
                   (LONG)(displacement_offset + 1u);
    if (displacement < -128 || displacement > 127) {
        builder->ok = 0;
        return 0;
    }
    builder->code[displacement_offset] = (BYTE)displacement;
    return 1;
}

static int gu_source_finalize_stub(
    struct GuSourceCodeBuilder *builder)
{
    DWORD old_protection = 0u;
    if (!builder || !builder->code || !builder->ok ||
        builder->cursor == 0u ||
        !VirtualProtect(
            builder->code, GU_SOURCE_STUB_BYTES,
            PAGE_EXECUTE_READ, &old_protection) ||
        !FlushInstructionCache(
            GetCurrentProcess(), builder->code,
            builder->cursor)) {
        return 0;
    }
    return 1;
}

static BYTE *gu_source_build_enter_stub(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    struct GuSourceCodeBuilder builder;
    DWORD false_displacement;
    DWORD after_displacement;
    DWORD false_offset;
    DWORD after_offset;
    BYTE *stub = gu_source_allocate_stub();
    if (!stub) {
        return NULL;
    }
    builder.code = stub;
    builder.capacity = GU_SOURCE_STUB_BYTES;
    builder.cursor = 0u;
    builder.ok = 1;

    /* Preserve the exact post-Enter machine state around mod work. */
    gu_source_emit_callback_state_save(&builder);
    /* movzx eax,byte ptr [esp + original_guard_offset + full frame] */
    gu_source_emit_byte(&builder, 0x0Fu);
    gu_source_emit_byte(&builder, 0xB6u);
    gu_source_emit_byte(&builder, 0x84u);
    gu_source_emit_byte(&builder, 0x24u);
    gu_source_emit_u32(
        &builder,
        family->enter_guard_stack_offset + GU_SOURCE_FULL_FRAME_BYTES);
    gu_source_emit_byte(&builder, 0x83u); /* cmp eax,1 */
    gu_source_emit_byte(&builder, 0xF8u);
    gu_source_emit_byte(&builder, 0x01u);
    gu_source_emit_byte(&builder, 0x75u); /* jne false */
    false_displacement = builder.cursor;
    gu_source_emit_byte(&builder, 0u);
    /* mov edx,[critical object]; test edx,edx; setne al; movzx eax,al */
    gu_source_emit_byte(&builder, 0x8Bu);
    gu_source_emit_byte(&builder, 0x15u);
    gu_source_emit_u32(
        &builder,
        (DWORD)(ULONG_PTR)(image_base + family->critical_object_rva));
    gu_source_emit_byte(&builder, 0x85u);
    gu_source_emit_byte(&builder, 0xD2u);
    gu_source_emit_byte(&builder, 0x0Fu);
    gu_source_emit_byte(&builder, 0x95u);
    gu_source_emit_byte(&builder, 0xC0u);
    gu_source_emit_byte(&builder, 0x0Fu);
    gu_source_emit_byte(&builder, 0xB6u);
    gu_source_emit_byte(&builder, 0xC0u);
    gu_source_emit_byte(&builder, 0xEBu); /* jmp after_false */
    after_displacement = builder.cursor;
    gu_source_emit_byte(&builder, 0u);
    false_offset = builder.cursor;
    gu_source_emit_byte(&builder, 0x31u); /* xor eax,eax */
    gu_source_emit_byte(&builder, 0xC0u);
    after_offset = builder.cursor;
    gu_source_patch_short_branch(
        &builder, false_displacement, false_offset);
    gu_source_patch_short_branch(
        &builder, after_displacement, after_offset);
    gu_source_emit_byte(&builder, 0x50u); /* push eax */
    gu_source_emit_absolute_call(
        &builder, (const void *)gu_source_bank_enter_frame);
    gu_source_emit_callback_state_restore(&builder);
    gu_source_emit_bytes(
        &builder, family->enter_post_call.expected,
        family->enter_post_call.length);
    gu_source_emit_jump(
        &builder,
        image_base + family->enter_post_call.rva +
            family->enter_post_call.length);
    if (!gu_source_finalize_stub(&builder)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static BYTE *gu_source_build_leave_stub(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    struct GuSourceCodeBuilder builder;
    BYTE *stub = gu_source_allocate_stub();
    if (!stub) {
        return NULL;
    }
    builder.code = stub;
    builder.capacity = GU_SOURCE_STUB_BYTES;
    builder.cursor = 0u;
    builder.ok = 1;
    /* Release/restore the owned frame before the original engine Leave. */
    gu_source_emit_callback_state_save(&builder);
    gu_source_emit_absolute_call(
        &builder, (const void *)gu_source_bank_leave_frame);
    gu_source_emit_callback_state_restore(&builder);
    /* Tail jump: the helper RET consumes the original patched CALL return. */
    gu_source_emit_jump(
        &builder,
        image_base + family->engine_leave_helper_rva);
    if (!gu_source_finalize_stub(&builder)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static BYTE *gu_source_build_cap_stub(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    struct GuSourceCodeBuilder builder;
    BYTE *stub = gu_source_allocate_stub();
    if (!stub) {
        return NULL;
    }
    builder.code = stub;
    builder.capacity = GU_SOURCE_STUB_BYTES;
    builder.cursor = 0u;
    builder.ok = 1;
    /* Preserve incoming DF while C executes with DF clear.  The stock CMP
     * below intentionally replaces the outgoing arithmetic flags. */
    gu_source_emit_callback_state_save(&builder);
    gu_source_emit_absolute_call(
        &builder, (const void *)gu_source_bank_active_capacity);
    /* Saved ESP is ignored by POPAD and maps back to [esp-24]. */
    gu_source_emit_store_eax_esp_offset(
        &builder, GU_SOURCE_SAVED_ESP);
    gu_source_emit_callback_state_restore(&builder);
    /* cmp count,[esp-24] is the final flag-writing instruction. */
    gu_source_emit_byte(&builder, 0x3Bu);
    if (family->cap_count_register == GU_SOURCE_BANK_REGISTER_EBP) {
        gu_source_emit_byte(&builder, 0x6Cu);
    } else if (family->cap_count_register ==
               GU_SOURCE_BANK_REGISTER_EDI) {
        gu_source_emit_byte(&builder, 0x7Cu);
    } else {
        builder.ok = 0;
    }
    gu_source_emit_byte(&builder, 0x24u);
    gu_source_emit_byte(&builder, 0xE8u);
    gu_source_emit_jump(
        &builder,
        image_base + family->cap.rva + family->cap.length);
    if (!gu_source_finalize_stub(&builder)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static BYTE *gu_source_build_writer_stub(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    struct GuSourceCodeBuilder builder;
    DWORD failure_displacement;
    DWORD failure_offset;
    BYTE *stub = gu_source_allocate_stub();
    if (!stub) {
        return NULL;
    }
    builder.code = stub;
    builder.capacity = GU_SOURCE_STUB_BYTES;
    builder.cursor = 0u;
    builder.ok = 1;
    /* Preserve pre-INC/ADD CF and the complete interrupted FP/SIMD state. */
    gu_source_emit_callback_state_save(&builder);
    if (family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        /* mov ecx,[original esp + accepted_count_offset] */
        gu_source_emit_byte(&builder, 0x8Bu);
        gu_source_emit_byte(&builder, 0x8Cu);
        gu_source_emit_byte(&builder, 0x24u);
        gu_source_emit_u32(
            &builder, GU_SOURCE_FULL_FRAME_BYTES +
                family->writer_count_stack_offset);
    }
    /* lea eax,[original esp + stock_writer_base_offset] */
    gu_source_emit_byte(&builder, 0x8Du);
    gu_source_emit_byte(&builder, 0x84u);
    gu_source_emit_byte(&builder, 0x24u);
    gu_source_emit_u32(
        &builder,
        GU_SOURCE_FULL_FRAME_BYTES + family->stock_writer_base_offset);
    gu_source_emit_byte(&builder, 0x50u); /* arg3 stock buffer */
    if (family->writer_count_register == GU_SOURCE_BANK_REGISTER_EBP) {
        gu_source_emit_byte(&builder, 0x55u);
    } else if (family->writer_count_register ==
               GU_SOURCE_BANK_REGISTER_EAX) {
        /* EAX was overwritten by LEA; push saved original EAX. */
        gu_source_emit_byte(&builder, 0xFFu);
        gu_source_emit_byte(&builder, 0xB4u);
        gu_source_emit_byte(&builder, 0x24u);
        /* Arg3 is already pushed, so the saved frame is four bytes above
         * the current ESP. */
        gu_source_emit_u32(
            &builder, GU_SOURCE_SAVED_EAX + sizeof(DWORD));
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        gu_source_emit_byte(&builder, 0x51u); /* saved count in ECX */
    } else {
        builder.ok = 0;
    }
    gu_source_emit_byte(&builder, 0x57u); /* arg1 EDI pointer */
    gu_source_emit_absolute_call(
        &builder, (const void *)gu_source_bank_append);
    gu_source_emit_byte(&builder, 0x85u); /* test eax,eax */
    gu_source_emit_byte(&builder, 0xC0u);
    gu_source_emit_byte(&builder, 0x74u); /* jz failure */
    failure_displacement = builder.cursor;
    gu_source_emit_byte(&builder, 0u);
    gu_source_emit_callback_state_restore(&builder);
    if (family->abi == GU_SOURCE_BANK_ABI_CE_LEG) {
        gu_source_emit_byte(&builder, 0x45u); /* inc ebp */
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_7_8) {
        gu_source_emit_byte(&builder, 0x83u); /* add eax,1 */
        gu_source_emit_byte(&builder, 0xC0u);
        gu_source_emit_byte(&builder, 0x01u);
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        static const BYTE count_replay[] = {
            0x8Bu, 0x44u, 0x24u, 0x10u,
            0x83u, 0xC0u, 0x01u,
            0x89u, 0x44u, 0x24u, 0x10u
        };
        gu_source_emit_bytes(
            &builder, count_replay, sizeof(count_replay));
    } else {
        builder.ok = 0;
    }
    gu_source_emit_jump(
        &builder,
        image_base + family->writer.rva + family->writer.length);
    failure_offset = builder.cursor;
    gu_source_patch_short_branch(
        &builder, failure_displacement, failure_offset);
    gu_source_emit_callback_state_restore(&builder);
    if (family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        static const BYTE count_load[] = {
            0x8Bu, 0x44u, 0x24u, 0x10u
        };
        gu_source_emit_bytes(&builder, count_load, sizeof(count_load));
    }
    gu_source_emit_jump(
        &builder,
        image_base + family->writer.rva + family->writer.length);
    if (!gu_source_finalize_stub(&builder)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static BYTE *gu_source_build_reader_stub(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    struct GuSourceCodeBuilder builder;
    BYTE *stub = gu_source_allocate_stub();
    if (!stub) {
        return NULL;
    }
    builder.code = stub;
    builder.capacity = GU_SOURCE_STUB_BYTES;
    builder.cursor = 0u;
    builder.ok = 1;
    gu_source_emit_callback_state_save(&builder);
    /* lea eax,[original esp + stock_reader_base_offset] */
    gu_source_emit_byte(&builder, 0x8Du);
    gu_source_emit_byte(&builder, 0x84u);
    gu_source_emit_byte(&builder, 0x24u);
    gu_source_emit_u32(
        &builder,
        GU_SOURCE_FULL_FRAME_BYTES + family->stock_reader_base_offset);
    gu_source_emit_byte(&builder, 0x50u); /* arg2 stock buffer */
    gu_source_emit_byte(&builder, 0x56u); /* arg1 ESI index */
    gu_source_emit_absolute_call(
        &builder, (const void *)gu_source_bank_read);
    /* Saved ESP is ignored by POPAD and maps back to stock [esp-24]. */
    gu_source_emit_store_eax_esp_offset(
        &builder, GU_SOURCE_SAVED_ESP);
    gu_source_emit_callback_state_restore(&builder);
    if (family->abi == GU_SOURCE_BANK_ABI_CE_LEG) {
        /* push dword ptr [esp-24]; mov ecx,ebx */
        gu_source_emit_byte(&builder, 0xFFu);
        gu_source_emit_byte(&builder, 0x74u);
        gu_source_emit_byte(&builder, 0x24u);
        gu_source_emit_byte(&builder, 0xE8u);
        gu_source_emit_byte(&builder, 0x8Bu);
        gu_source_emit_byte(&builder, 0xCBu);
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_7_8 ||
               family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        /* mov ecx,[esp-24]; push ecx */
        gu_source_emit_byte(&builder, 0x8Bu);
        gu_source_emit_byte(&builder, 0x4Cu);
        gu_source_emit_byte(&builder, 0x24u);
        gu_source_emit_byte(&builder, 0xE8u);
        gu_source_emit_byte(&builder, 0x51u);
    } else {
        builder.ok = 0;
    }
    gu_source_emit_jump(
        &builder,
        image_base + family->reader.rva + family->reader.length);
    if (!gu_source_finalize_stub(&builder)) {
        VirtualFree(stub, 0u, MEM_RELEASE);
        return NULL;
    }
    return stub;
}

static int gu_source_target_is_builtin(
    const struct GuSourceBankTargetContract *target)
{
    DWORD index;
    for (index = 0u; index < gu_source_bank_target_contract_count(); ++index) {
        if (target == &kGuSourceTargets[index]) {
            return 1;
        }
    }
    return 0;
}

static int gu_source_contract_shape_valid(
    const struct GuSourceBankFamilyContract *family)
{
    DWORD expected_writer_register;
    if (!family || family->critical_object_rva == 0u ||
        family->engine_leave_helper_rva == 0u ||
        family->enter_post_call.length < 5u ||
        family->leave_call.length != 5u ||
        family->cap.length < 5u ||
        family->writer.length < 5u ||
        family->reader.length < 5u ||
        family->radius_site_count == 0u ||
        family->radius_site_count > GU_SOURCE_BANK_MAX_RADIUS_SITES ||
        family->proof_site_count == 0u ||
        family->proof_site_count > GU_SOURCE_BANK_MAX_PROOF_SITES ||
        family->stock_writer_base_offset == 0u ||
        family->stock_reader_base_offset == 0u ||
        family->cap_count_register != GU_SOURCE_BANK_REGISTER_EBP &&
        family->cap_count_register != GU_SOURCE_BANK_REGISTER_EDI) {
        return 0;
    }
    if (family->abi == GU_SOURCE_BANK_ABI_CE_LEG) {
        expected_writer_register = GU_SOURCE_BANK_REGISTER_EBP;
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_7_8) {
        expected_writer_register = GU_SOURCE_BANK_REGISTER_EAX;
    } else if (family->abi == GU_SOURCE_BANK_ABI_PATCH_4) {
        expected_writer_register = GU_SOURCE_BANK_REGISTER_NONE;
        if (family->writer_count_stack_offset == 0u) {
            return 0;
        }
    } else {
        return 0;
    }
    if (family->writer_count_register != expected_writer_register) {
        return 0;
    }
    if (family->radius_patch_kind == GU_RADIUS_PATCH_DIRECT_FLOAT) {
        return family->radius_float_offset + sizeof(DWORD) <=
               family->radius_sites[0].length;
    }
    if (family->radius_patch_kind == GU_RADIUS_PATCH_POINTER_OPERAND) {
        return family->radius_site_count == 1u &&
               family->radius_float_offset + sizeof(DWORD) <=
               family->radius_sites[0].length;
    }
    return 0;
}

static int gu_source_validate_every_site(
    BYTE *image_base,
    const struct GuSourceBankFamilyContract *family)
{
    DWORD index;
    if (!gu_source_validate_site_with_diagnostic(
            image_base, &family->enter_post_call,
            GU_SOURCE_PREPARE_SITE_ENTER_POST_CALL, 0u) ||
        !gu_source_validate_site_with_diagnostic(
            image_base, &family->leave_call,
            GU_SOURCE_PREPARE_SITE_LEAVE_CALL, 0u) ||
        !gu_source_validate_site_with_diagnostic(
            image_base, &family->cap,
            GU_SOURCE_PREPARE_SITE_CAP, 0u) ||
        !gu_source_validate_site_with_diagnostic(
            image_base, &family->writer,
            GU_SOURCE_PREPARE_SITE_WRITER, 0u) ||
        !gu_source_validate_site_with_diagnostic(
            image_base, &family->reader,
            GU_SOURCE_PREPARE_SITE_READER, 0u)) {
        return 0;
    }
    for (index = 0u; index < family->radius_site_count; ++index) {
        if (!gu_source_validate_site_with_diagnostic(
                image_base, &family->radius_sites[index],
                GU_SOURCE_PREPARE_SITE_RADIUS, index)) {
            return 0;
        }
    }
    for (index = 0u; index < family->proof_site_count; ++index) {
        if (!gu_source_validate_site_with_diagnostic(
                image_base, &family->proof_sites[index],
                GU_SOURCE_PREPARE_SITE_PROOF, index)) {
            return 0;
        }
    }
    return 1;
}

static void gu_source_discard_transaction_tail(
    struct GuPatchTransaction *transaction, DWORD first)
{
    if (!transaction || first > transaction->count) {
        return;
    }
    ZeroMemory(
        &transaction->patches[first],
        (transaction->count - first) *
            sizeof(transaction->patches[0]));
    transaction->count = first;
}

static void gu_source_free_stub(BYTE *stub)
{
    if (stub) {
        VirtualFree(stub, 0u, MEM_RELEASE);
    }
}

void gu_source_bank_release_prepared(
    struct GuSourceBankPrepared *prepared)
{
    if (!prepared) {
        return;
    }
    gu_source_free_stub(prepared->enter_stub);
    gu_source_free_stub(prepared->leave_stub);
    gu_source_free_stub(prepared->cap_stub);
    gu_source_free_stub(prepared->writer_stub);
    gu_source_free_stub(prepared->reader_stub);
    ZeroMemory(prepared, sizeof(*prepared));
}

int gu_source_bank_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuSourceBankTargetContract *target,
    DWORD capacity,
    float distance_multiplier,
    struct GuSourceBankPrepared *prepared)
{
    const struct GuSourceBankFamilyContract *family;
    struct GuSourceBankPrepared local;
    BYTE replacement[GU_PATCH_MAX_BYTES];
    DWORD radius_bits;
    DWORD radius_pointer;
    DWORD index;
    DWORD first_patch;
    DWORD required_patches;

    gu_source_prepare_begin(transaction, image_base);
    gu_source_contracts_initialize();
    if (prepared) {
        ZeroMemory(prepared, sizeof(*prepared));
    }
    if (!transaction || transaction->committed || !image_base ||
        !target || !prepared ||
        capacity <= GU_SOURCE_BANK_STOCK_CAPACITY ||
        capacity > GU_SOURCE_BANK_CAPACITY ||
        !gu_source_float_multiplier_valid(distance_multiplier)) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_ARGUMENT, transaction);
    }
    if (!gu_source_target_is_builtin(target) ||
        !target->target_id || !target->sha256 || !target->family) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_TARGET_IDENTITY, transaction);
    }
    if (!gu_source_bank_initialize()) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_RUNTIME_INITIALIZE, transaction);
    }
    g_gu_source_prepare_diagnostic.tls_index = g_gu_source_bank_tls;
    family = target->family;
    if (family->enter_post_call.preferred_image_base != 0u) {
        g_gu_source_prepare_diagnostic.image_rebase_delta =
            (DWORD)(ULONG_PTR)image_base -
            family->enter_post_call.preferred_image_base;
    }
    required_patches = 5u + family->radius_site_count;
    if (!gu_source_contract_shape_valid(family)) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_CONTRACT_SHAPE, transaction);
    }
    if (transaction->count > GU_TRANSACTION_MAX_PATCHES ||
        required_patches >
            GU_TRANSACTION_MAX_PATCHES - transaction->count) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_TRANSACTION_CAPACITY, transaction);
    }
    if (!gu_source_validate_every_site(image_base, family)) {
        return gu_source_prepare_fail(
            GU_SOURCE_PREPARE_LIVE_PREIMAGE, transaction);
    }

    /* No transaction entry is added until every exact preimage has passed. */
    ZeroMemory(&local, sizeof(local));
    SetLastError(ERROR_SUCCESS);
    local.enter_stub = gu_source_build_enter_stub(image_base, family);
    if (!local.enter_stub) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_ENTER_STUB, transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    local.leave_stub = gu_source_build_leave_stub(image_base, family);
    if (!local.leave_stub) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_LEAVE_STUB, transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    local.cap_stub = gu_source_build_cap_stub(image_base, family);
    if (!local.cap_stub) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_CAP_STUB, transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    local.writer_stub = gu_source_build_writer_stub(image_base, family);
    if (!local.writer_stub) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_WRITER_STUB, transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    local.reader_stub = gu_source_build_reader_stub(image_base, family);
    if (!local.reader_stub) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_READER_STUB, transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }

    first_patch = transaction->count;
    SetLastError(ERROR_SUCCESS);
    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &family->enter_post_call, local.enter_stub)) {
        gu_source_prepare_fail(GU_SOURCE_PREPARE_ADD_ENTER, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    if (!gu_patch_transaction_add_call(
            transaction, image_base,
            &family->leave_call, local.leave_stub)) {
        gu_source_prepare_fail(GU_SOURCE_PREPARE_ADD_LEAVE, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &family->cap, local.cap_stub)) {
        gu_source_prepare_fail(GU_SOURCE_PREPARE_ADD_CAP, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &family->writer, local.writer_stub)) {
        gu_source_prepare_fail(GU_SOURCE_PREPARE_ADD_WRITER, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    if (!gu_patch_transaction_add_jump(
            transaction, image_base,
            &family->reader, local.reader_stub)) {
        gu_source_prepare_fail(GU_SOURCE_PREPARE_ADD_READER, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }

    radius_bits = *(DWORD *)(void *)&distance_multiplier;
    {
        float radius = 30.0f * distance_multiplier;
        radius_bits = *(DWORD *)(void *)&radius;
    }
    radius_pointer =
        (DWORD)(ULONG_PTR)&g_gu_source_bank_radius_value;
    for (index = 0u; index < family->radius_site_count; ++index) {
        const struct GuPatchContract *site =
            &family->radius_sites[index];
        gu_source_copy(replacement, site->expected, site->length);
        if (family->radius_patch_kind ==
                GU_RADIUS_PATCH_DIRECT_FLOAT) {
            replacement[family->radius_float_offset + 0u] =
                (BYTE)(radius_bits & 0xFFu);
            replacement[family->radius_float_offset + 1u] =
                (BYTE)((radius_bits >> 8) & 0xFFu);
            replacement[family->radius_float_offset + 2u] =
                (BYTE)((radius_bits >> 16) & 0xFFu);
            replacement[family->radius_float_offset + 3u] =
                (BYTE)((radius_bits >> 24) & 0xFFu);
        } else {
            replacement[family->radius_float_offset + 0u] =
                (BYTE)(radius_pointer & 0xFFu);
            replacement[family->radius_float_offset + 1u] =
                (BYTE)((radius_pointer >> 8) & 0xFFu);
            replacement[family->radius_float_offset + 2u] =
                (BYTE)((radius_pointer >> 16) & 0xFFu);
            replacement[family->radius_float_offset + 3u] =
                (BYTE)((radius_pointer >> 24) & 0xFFu);
        }
        SetLastError(ERROR_SUCCESS);
        if (!gu_patch_transaction_add(
                transaction, image_base, site, replacement)) {
            g_gu_source_prepare_diagnostic.site_kind =
                GU_SOURCE_PREPARE_SITE_RADIUS;
            g_gu_source_prepare_diagnostic.site_index = index;
            g_gu_source_prepare_diagnostic.site_rva = site->rva;
            g_gu_source_prepare_diagnostic.site_length = site->length;
            gu_source_prepare_fail(
                GU_SOURCE_PREPARE_ADD_RADIUS, transaction);
            gu_source_discard_transaction_tail(transaction, first_patch);
            gu_source_prepare_after_discard(transaction);
            gu_source_bank_release_prepared(&local);
            return 0;
        }
    }

    SetLastError(ERROR_SUCCESS);
    if (!gu_source_bank_configure(capacity, distance_multiplier)) {
        gu_source_prepare_fail(
            GU_SOURCE_PREPARE_CONFIGURE, transaction);
        gu_source_discard_transaction_tail(transaction, first_patch);
        gu_source_prepare_after_discard(transaction);
        gu_source_bank_release_prepared(&local);
        return 0;
    }
    *prepared = local;
    g_gu_source_prepare_diagnostic.stage = GU_SOURCE_PREPARE_COMPLETE;
    g_gu_source_prepare_diagnostic.win32_error = ERROR_SUCCESS;
    g_gu_source_prepare_diagnostic.transaction_count_partial =
        transaction->count;
    g_gu_source_prepare_diagnostic.transaction_count_after =
        transaction->count;
    g_gu_source_prepare_diagnostic.transaction_committed =
        transaction->committed;
    return 1;
}
