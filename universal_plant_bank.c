#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "universal_plant_bank.h"
#include "universal_pipeline.h"
#include "grass_compat.generated.h"

#define GU_PLANT_THREAD_CONTEXTS 16u
#define GU_PLANT_FRAME_DEPTH 16u
#define GU_PLANT_SELECTOR_DISABLED 0xFFFFFFFFu
#define GU_PLANT_STUB_BYTES 512u
#define GU_PLANT_FP_SAVE_BYTES 528u
#define GU_PLANT_STUB_SAVED_BYTES 564u

#define GU_SAVED_EDI 528u
#define GU_SAVED_ESI 532u
#define GU_SAVED_EBP 536u
#define GU_SAVED_ESP 540u
#define GU_SAVED_EBX 544u
#define GU_SAVED_EDX 548u
#define GU_SAVED_ECX 552u
#define GU_SAVED_EAX 556u
#define GU_SAVED_FLAGS 560u

#define GU_RESOLVE_CANDIDATE_RECORD 1u
#define GU_RESOLVE_CANDIDATE_POINTER_BASE 2u
#define GU_RESOLVE_OUTPUT_POINTER_BASE 3u
#define GU_RESOLVE_OUTPUT_POINTER_VALUE 4u

#define GU_SOURCE_REGISTER 1u
#define GU_SOURCE_STACK 2u

#define GU_SITE(r, l, ...) { (r), (l), { __VA_ARGS__ } }
#define GU_SITE_RELOC1(r, l, p, o, ...) \
    { (r), (l), { __VA_ARGS__ }, (p), 1u, { (o) } }

static const struct GuPlantBankFamilyContract g_gu_plant_ce = {
    GU_PLANT_BANK_ABI_CE_LEG, 0x5Cu, 0x84u, 0xA8u, 0x00809FA0u,
    GU_SITE(0x0080A0A8u, 8u,
        0xF3,0x0F,0x59,0xC8,0xF3,0x0F,0x2C,0xF9),
    GU_SITE(0x0080A0C3u, 13u,
        0x8D,0x81,0x81,0x01,0x00,0x00,0xC1,0xE0,0x05,0x03,0x44,0x24,0x34),
    GU_SITE(0x0080A327u, 6u,
        0x81,0xC6,0x20,0x38,0x00,0x00),
    GU_SITE(0x0080A340u, 6u,
        0x8B,0x86,0x00,0x19,0x00,0x00),
    GU_SITE(0x0080A70Du, 6u,
        0x8D,0x81,0x20,0x51,0x00,0x00),
    GU_SITE(0x0080A75Au, 6u,
        0x8D,0x81,0x20,0x38,0x00,0x00),
    GU_SITE(0x0080A793u, 7u,
        0xFF,0xB4,0x81,0x20,0x51,0x00,0x00),
    GU_SITE(0x0080ACA6u, 5u,
        0xE8,0xF5,0xF2,0xFF,0xFF),
    {
        GU_SITE(0x00809FA0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x00809FBAu, 13u,
            0x56,0x57,0x8B,0x7D,0x08,0x89,0x4C,0x24,0x34,0x89,0x7C,0x24,0x58),
        GU_SITE(0x00809FD0u, 24u,
            0x33,0xC0,0x5F,0x5E,0x8B,0x8C,0x24,0xF4,0x02,0x00,0x00,0x33,
            0xCC,0xE8,0xCC,0xEA,0x1E,0x00,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0080A7DFu, 19u,
            0x8B,0x44,0x24,0x78,0x5F,0x5E,0x33,0xCC,0xE8,0xC2,
            0xE2,0x1E,0x00,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0080ACA0u, 16u,
            0xFF,0x74,0xB4,0x1C,0x8B,0xCB,0xE8,0xF5,
            0xF2,0xFF,0xFF,0x46,0x3B,0xF5,0x7C,0xF0),
        GU_SITE(0x00338970u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x003389B4u, 8u, 0x8B,0x4D,0x1C,0x8B,0x7D,0x0C,0x2B,0xCF),
        GU_SITE(0x00338A71u, 3u, 0xC2,0x2C,0x00),
        GU_SITE(0x003394D8u, 11u,
            0x43,0xB8,0x7F,0x00,0x00,0x00,0x3B,0xD8,0x0F,0x4C,0xC3),
        GU_SITE(0x003394ECu, 9u,
            0x43,0x81,0xFB,0x80,0x00,0x00,0x00,0x7D,0x0F)
    },
    10u
};

static const struct GuPlantBankFamilyContract g_gu_plant_leg = {
    GU_PLANT_BANK_ABI_CE_LEG, 0x5Cu, 0x84u, 0xA8u, 0x008098B0u,
    GU_SITE(0x008099B8u, 8u,
        0xF3,0x0F,0x59,0xC8,0xF3,0x0F,0x2C,0xF9),
    GU_SITE(0x008099D3u, 13u,
        0x8D,0x81,0x81,0x01,0x00,0x00,0xC1,0xE0,0x05,0x03,0x44,0x24,0x34),
    GU_SITE(0x00809C37u, 6u, 0x81,0xC6,0x20,0x38,0x00,0x00),
    GU_SITE(0x00809C50u, 6u, 0x8B,0x86,0x00,0x19,0x00,0x00),
    GU_SITE(0x0080A01Du, 6u, 0x8D,0x81,0x20,0x51,0x00,0x00),
    GU_SITE(0x0080A06Au, 6u, 0x8D,0x81,0x20,0x38,0x00,0x00),
    GU_SITE(0x0080A0A3u, 7u, 0xFF,0xB4,0x81,0x20,0x51,0x00,0x00),
    GU_SITE(0x0080A5B6u, 5u, 0xE8,0xF5,0xF2,0xFF,0xFF),
    {
        GU_SITE(0x008098B0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x008098CAu, 13u,
            0x56,0x57,0x8B,0x7D,0x08,0x89,0x4C,0x24,0x34,0x89,0x7C,0x24,0x58),
        GU_SITE(0x008098E0u, 24u,
            0x33,0xC0,0x5F,0x5E,0x8B,0x8C,0x24,0xF4,0x02,0x00,0x00,0x33,
            0xCC,0xE8,0xBC,0xEE,0x1E,0x00,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0080A0EFu, 19u,
            0x8B,0x44,0x24,0x78,0x5F,0x5E,0x33,0xCC,0xE8,0xB2,
            0xE6,0x1E,0x00,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0080A5B0u, 16u,
            0xFF,0x74,0xB4,0x1C,0x8B,0xCB,0xE8,0xF5,
            0xF2,0xFF,0xFF,0x46,0x3B,0xF5,0x7C,0xF0),
        GU_SITE(0x003385D0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x00338614u, 8u, 0x8B,0x4D,0x1C,0x8B,0x7D,0x0C,0x2B,0xCF),
        GU_SITE(0x003386D1u, 3u, 0xC2,0x2C,0x00),
        GU_SITE(0x00339138u, 11u,
            0x43,0xB8,0x7F,0x00,0x00,0x00,0x3B,0xD8,0x0F,0x4C,0xC3),
        GU_SITE(0x0033914Cu, 9u,
            0x43,0x81,0xFB,0x80,0x00,0x00,0x00,0x7D,0x0F)
    },
    10u
};

static const struct GuPlantBankFamilyContract g_gu_plant_p8 = {
    GU_PLANT_BANK_ABI_PATCH, 0x0Cu, 0x58u, 0x68u, 0x005F94E0u,
    GU_SITE(0x005F95B4u, 8u,
        0xF3,0x0F,0x59,0xC1,0xF3,0x0F,0x2C,0xC0),
    GU_SITE(0x005F95D3u, 13u,
        0x8D,0xB8,0x81,0x01,0x00,0x00,0xC1,0xE7,0x05,0x03,0x7C,0x24,0x2C),
    GU_SITE(0x005F9828u, 6u, 0x81,0xC6,0x20,0x38,0x00,0x00),
    GU_SITE(0x005F982Eu, 6u, 0x8B,0x86,0x00,0x19,0x00,0x00),
    GU_SITE(0x005F9BE1u, 6u, 0x8D,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x005F9C1Fu, 5u, 0x05,0x20,0x38,0x00,0x00),
    GU_SITE(0x005F9C50u, 7u, 0x8B,0x94,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x005FA577u, 5u, 0xE8,0x64,0xEF,0xFF,0xFF),
    {
        GU_SITE(0x005F94E0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x005F94ECu, 7u, 0x53,0x56,0x57,0x89,0x4C,0x24,0x2C),
        GU_SITE(0x005F94FCu, 11u, 0x33,0xC0,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x005F9C86u, 13u,
            0x8B,0x44,0x24,0x50,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x005FA570u, 19u,
            0x8B,0x4C,0xB4,0x14,0x51,0x8B,0xCB,0xE8,0x64,0xEF,
            0xFF,0xFF,0x83,0xC6,0x01,0x3B,0xF7,0x7C,0xED),
        GU_SITE(0x000E73C0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x000E7403u, 8u, 0x8B,0x7D,0x0C,0x8B,0x5D,0x1C,0x2B,0xDF),
        GU_SITE(0x000E749Bu, 3u, 0xC2,0x2C,0x00),
        GU_SITE(0x000E7F34u, 18u,
            0x83,0xC7,0x01,0x83,0xFF,0x7F,0x89,0x7D,0x1C,
            0x8B,0xC7,0x7C,0x05,0xB8,0x7F,0x00,0x00,0x00),
        GU_SITE(0x000E7F53u, 14u,
            0x83,0xC7,0x01,0x81,0xFF,0x80,0x00,0x00,0x00,
            0x89,0x7D,0x1C,0x7D,0x0D)
    },
    10u
};

static const struct GuPlantBankFamilyContract g_gu_plant_p7 = {
    GU_PLANT_BANK_ABI_PATCH, 0x0Cu, 0x58u, 0x68u, 0x0051DF70u,
    GU_SITE(0x0051E044u, 8u,
        0xF3,0x0F,0x59,0xC1,0xF3,0x0F,0x2C,0xC0),
    GU_SITE(0x0051E063u, 13u,
        0x8D,0xB8,0x81,0x01,0x00,0x00,0xC1,0xE7,0x05,0x03,0x7C,0x24,0x2C),
    GU_SITE(0x0051E2B8u, 6u, 0x81,0xC6,0x20,0x38,0x00,0x00),
    GU_SITE(0x0051E2BEu, 6u, 0x8B,0x86,0x00,0x19,0x00,0x00),
    GU_SITE(0x0051E671u, 6u, 0x8D,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x0051E6AFu, 5u, 0x05,0x20,0x38,0x00,0x00),
    GU_SITE(0x0051E6E0u, 7u, 0x8B,0x94,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x0051F007u, 5u, 0xE8,0x64,0xEF,0xFF,0xFF),
    {
        GU_SITE(0x0051DF70u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x0051DF7Cu, 7u, 0x53,0x56,0x57,0x89,0x4C,0x24,0x2C),
        GU_SITE(0x0051DF8Cu, 11u, 0x33,0xC0,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0051E716u, 13u,
            0x8B,0x44,0x24,0x50,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0051F000u, 19u,
            0x8B,0x4C,0xB4,0x14,0x51,0x8B,0xCB,0xE8,0x64,0xEF,
            0xFF,0xFF,0x83,0xC6,0x01,0x3B,0xF7,0x7C,0xED),
        GU_SITE(0x002A70E0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x002A7123u, 8u, 0x8B,0x7D,0x0C,0x8B,0x5D,0x1C,0x2B,0xDF),
        GU_SITE(0x002A71BBu, 3u, 0xC2,0x2C,0x00),
        GU_SITE(0x002A7854u, 18u,
            0x83,0xC7,0x01,0x83,0xFF,0x7F,0x89,0x7D,0x1C,
            0x8B,0xC7,0x7C,0x05,0xB8,0x7F,0x00,0x00,0x00),
        GU_SITE(0x002A7873u, 14u,
            0x83,0xC7,0x01,0x81,0xFF,0x80,0x00,0x00,0x00,
            0x89,0x7D,0x1C,0x7D,0x0D)
    },
    10u
};

static const struct GuPlantBankFamilyContract g_gu_plant_104 = {
    GU_PLANT_BANK_ABI_PATCH, 0x0Cu, 0x58u, 0x68u, 0x0053EDF0u,
    GU_SITE(0x0053EEC4u, 8u,
        0xF3,0x0F,0x59,0xC1,0xF3,0x0F,0x2C,0xC0),
    GU_SITE(0x0053EEE3u, 13u,
        0x8D,0xB8,0x81,0x01,0x00,0x00,0xC1,0xE7,0x05,0x03,0x7C,0x24,0x2C),
    GU_SITE(0x0053F138u, 6u, 0x81,0xC6,0x20,0x38,0x00,0x00),
    GU_SITE(0x0053F13Eu, 6u, 0x8B,0x86,0x00,0x19,0x00,0x00),
    GU_SITE(0x0053F4F1u, 6u, 0x8D,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x0053F52Fu, 5u, 0x05,0x20,0x38,0x00,0x00),
    GU_SITE(0x0053F560u, 7u, 0x8B,0x94,0x88,0x20,0x51,0x00,0x00),
    GU_SITE(0x0053FE87u, 5u, 0xE8,0x64,0xEF,0xFF,0xFF),
    {
        GU_SITE(0x0053EDF0u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x0053EDFCu, 7u, 0x53,0x56,0x57,0x89,0x4C,0x24,0x2C),
        GU_SITE(0x0053EE0Cu, 11u, 0x33,0xC0,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0053F596u, 13u,
            0x8B,0x44,0x24,0x50,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xC2,0x04,0x00),
        GU_SITE(0x0053FE80u, 19u,
            0x8B,0x4C,0xB4,0x14,0x51,0x8B,0xCB,0xE8,0x64,0xEF,
            0xFF,0xFF,0x83,0xC6,0x01,0x3B,0xF7,0x7C,0xED),
        GU_SITE(0x000D9990u, 6u, 0x55,0x8B,0xEC,0x83,0xE4,0xF0),
        GU_SITE(0x000D99D3u, 8u, 0x8B,0x7D,0x0C,0x8B,0x5D,0x1C,0x2B,0xDF),
        GU_SITE(0x000D9A6Bu, 3u, 0xC2,0x2C,0x00),
        GU_SITE(0x000DA504u, 18u,
            0x83,0xC7,0x01,0x83,0xFF,0x7F,0x89,0x7D,0x1C,
            0x8B,0xC7,0x7C,0x05,0xB8,0x7F,0x00,0x00,0x00),
        GU_SITE(0x000DA523u, 14u,
            0x83,0xC7,0x01,0x81,0xFF,0x80,0x00,0x00,0x00,
            0x89,0x7D,0x1C,0x7D,0x0D)
    },
    10u
};

static const struct GuPlantBankTargetContract g_gu_plant_targets[] = {
#include "grass_compat_plant.generated.inc"
};

struct GuPlantFrame {
    DWORD previous_selector;
    DWORD leased_selector;
};

struct GuPlantThreadContext {
    volatile LONG owner_thread_id;
    DWORD token;
    DWORD depth;
    DWORD active_selector;
    DWORD overflow_depth;
    DWORD overflow_previous;
    struct GuPlantFrame frames[GU_PLANT_FRAME_DEPTH];
};

struct GuPlantRuntime {
    volatile LONG initialize_state;
    BYTE *storage;
    volatile LONG bank_owner[GU_PLANT_BANK_COUNT];
    struct GuPlantThreadContext contexts[GU_PLANT_THREAD_CONTEXTS];
    volatile LONG selector_callback;
    volatile LONG selector_user;
    volatile LONG model_info_table;
};

static struct GuPlantRuntime g_gu_plant_runtime = {
    0, NULL, {0,0,0,0}, {{0}}, 0, 0, 0
};
static volatile LONG g_gu_plant_count_calls;
static volatile LONG g_gu_plant_invalid_calls;
static volatile LONG g_gu_plant_selected_calls;
static volatile LONG g_gu_plant_authored_count_total;
static volatile LONG g_gu_plant_emitted_count_total;
static volatile LONG g_gu_plant_clamp_events;
static volatile LONG g_gu_plant_capacity_saturations;
static volatile LONG g_gu_plant_maximum_total_after;

static void gu_plant_update_maximum(
    volatile LONG *destination, DWORD candidate)
{
    LONG observed = InterlockedCompareExchange(destination, 0, 0);
    while (candidate > (DWORD)observed) {
        LONG previous = InterlockedCompareExchange(
            destination, (LONG)candidate, observed);
        if (previous == observed) {
            return;
        }
        observed = previous;
    }
}

static void gu_plant_reset_counters(void)
{
    InterlockedExchange(&g_gu_plant_count_calls, 0);
    InterlockedExchange(&g_gu_plant_invalid_calls, 0);
    InterlockedExchange(&g_gu_plant_selected_calls, 0);
    InterlockedExchange(&g_gu_plant_authored_count_total, 0);
    InterlockedExchange(&g_gu_plant_emitted_count_total, 0);
    InterlockedExchange(&g_gu_plant_clamp_events, 0);
    InterlockedExchange(&g_gu_plant_capacity_saturations, 0);
    InterlockedExchange(&g_gu_plant_maximum_total_after, 0);
}

static DWORD gu_float_bits(float value)
{
    union { float f; DWORD u; } bits;
    bits.f = value;
    return bits.u;
}

static float gu_bits_float(DWORD value)
{
    union { float f; DWORD u; } bits;
    bits.u = value;
    return bits.f;
}

static int gu_float_is_finite_nonnegative(DWORD bits)
{
    return (bits & 0x80000000u) == 0u &&
           (bits & 0x7F800000u) != 0x7F800000u;
}

DWORD gu_plant_bank_target_contract_count(void)
{
    return (DWORD)(sizeof(g_gu_plant_targets) /
                   sizeof(g_gu_plant_targets[0]));
}

const struct GuPlantBankTargetContract *gu_plant_bank_target_contract_at(
    DWORD index)
{
    if (index >= gu_plant_bank_target_contract_count()) {
        return NULL;
    }
    return &g_gu_plant_targets[index];
}

const struct GuPlantBankTargetContract *gu_plant_bank_contract_for_target(
    const char *target_id)
{
    DWORD index;
    if (!target_id) {
        return NULL;
    }
    for (index = 0u; index < gu_plant_bank_target_contract_count(); ++index) {
        if (lstrcmpA(target_id, g_gu_plant_targets[index].target_id) == 0) {
            return &g_gu_plant_targets[index];
        }
    }
    return NULL;
}

static DWORD gu_plant_read_u32(const BYTE *address)
{
    return (DWORD)address[0] |
           ((DWORD)address[1] << 8) |
           ((DWORD)address[2] << 16) |
           ((DWORD)address[3] << 24);
}

static int gu_plant_target_is_builtin(
    const struct GuPlantBankTargetContract *target)
{
    DWORD index;
    for (index = 0u; index < gu_plant_bank_target_contract_count(); ++index) {
        if (target == &g_gu_plant_targets[index]) {
            return 1;
        }
    }
    return 0;
}

static int gu_plant_model_table_contract_valid(
    const struct GuPlantBankTargetContract *target)
{
    const struct GuPatchContract *site;
    DWORD expected_address;
    if (!target || target->model_info_table_rva == 0u) {
        return 0;
    }
    site = &target->model_info_table_load_site;
    if (site->length != 7u || site->expected[0] != 0x8Bu ||
        (site->expected[1] != 0x34u && site->expected[1] != 0x3Cu) ||
        site->expected[2] != 0x85u ||
        site->preferred_image_base == 0u ||
        site->relocation_count != 1u ||
        site->relocation_offsets[0] != 3u ||
        target->model_info_table_rva >
            0xFFFFFFFFu - site->preferred_image_base) {
        return 0;
    }
    expected_address = site->preferred_image_base +
                       target->model_info_table_rva;
    return gu_plant_read_u32(site->expected + 3u) == expected_address;
}

static int gu_plant_range_readable(const BYTE *address, DWORD length)
{
    const BYTE *cursor = address;
    DWORD remaining = length;
    if (!address || length == 0u) {
        return 0;
    }
    while (remaining != 0u) {
        MEMORY_BASIC_INFORMATION information;
        DWORD cursor_value;
        DWORD region_end;
        DWORD available;
        DWORD protection;
        ZeroMemory(&information, sizeof(information));
        if (VirtualQuery(
                cursor, &information,
                sizeof(information)) != sizeof(information) ||
            information.State != MEM_COMMIT) {
            return 0;
        }
        protection = information.Protect & 0xFFu;
        if ((information.Protect & PAGE_GUARD) != 0u ||
            protection == PAGE_NOACCESS) {
            return 0;
        }
        cursor_value = (DWORD)(ULONG_PTR)cursor;
        if ((DWORD)(ULONG_PTR)information.BaseAddress >
                0xFFFFFFFFu - information.RegionSize) {
            return 0;
        }
        region_end = (DWORD)(ULONG_PTR)information.BaseAddress +
                     information.RegionSize;
        if (cursor_value >= region_end) {
            return 0;
        }
        available = region_end - cursor_value;
        if (available >= remaining) {
            return 1;
        }
        cursor += available;
        remaining -= available;
    }
    return 1;
}

static BYTE *gu_bank_base_unchecked(DWORD bank_index)
{
    return g_gu_plant_runtime.storage + bank_index * GU_PLANT_BANK_BYTES;
}

void *gu_plant_bank_storage_base(void)
{
    if (InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 2, 2) != 2) {
        return NULL;
    }
    return g_gu_plant_runtime.storage;
}

void *gu_plant_bank_base(DWORD bank_index)
{
    if (!gu_plant_bank_storage_base() || bank_index >= GU_PLANT_BANK_COUNT) {
        return NULL;
    }
    return gu_bank_base_unchecked(bank_index);
}

int gu_plant_bank_initialize(void)
{
    BYTE *storage;
    DWORD bank;
    DWORD index;
    LONG state = InterlockedCompareExchange(
        &g_gu_plant_runtime.initialize_state, 1, 0);
    if (state == 2) {
        return 1;
    }
    if (state != 0) {
        while (InterlockedCompareExchange(
                   &g_gu_plant_runtime.initialize_state, 0, 0) == 1) {
            SwitchToThread();
        }
        return InterlockedCompareExchange(
                   &g_gu_plant_runtime.initialize_state, 0, 0) == 2;
    }
    if (sizeof(void *) != 4u) {
        InterlockedExchange(&g_gu_plant_runtime.initialize_state, 0);
        return 0;
    }
    storage = (BYTE *)VirtualAlloc(
        NULL, GU_PLANT_BANK_COUNT * GU_PLANT_BANK_BYTES,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!storage) {
        InterlockedExchange(&g_gu_plant_runtime.initialize_state, 0);
        return 0;
    }
    ZeroMemory(storage, GU_PLANT_BANK_COUNT * GU_PLANT_BANK_BYTES);
    ZeroMemory(g_gu_plant_runtime.contexts,
               sizeof(g_gu_plant_runtime.contexts));
    for (bank = 0u; bank < GU_PLANT_BANK_COUNT; ++bank) {
        BYTE *base = storage + bank * GU_PLANT_BANK_BYTES;
        void **candidate_pointers = (void **)(
            base + GU_PLANT_BANK_CANDIDATE_POINTERS_OFFSET);
        void **output_pointers = (void **)(
            base + GU_PLANT_BANK_OUTPUT_POINTERS_OFFSET);
        for (index = 0u; index < GU_PLANT_BANK_CAPACITY; ++index) {
            candidate_pointers[index] =
                base + GU_PLANT_BANK_CANDIDATE_RECORDS_OFFSET +
                index * GU_PLANT_BANK_CANDIDATE_STRIDE;
            output_pointers[index] =
                base + GU_PLANT_BANK_OUTPUT_RECORDS_OFFSET +
                index * GU_PLANT_BANK_OUTPUT_STRIDE;
        }
        InterlockedExchange(&g_gu_plant_runtime.bank_owner[bank], 0);
    }
    g_gu_plant_runtime.storage = storage;
    gu_plant_reset_counters();
    InterlockedExchange(&g_gu_plant_runtime.selector_user, 0);
    InterlockedExchange(&g_gu_plant_runtime.selector_callback, 0);
    InterlockedExchange(&g_gu_plant_runtime.model_info_table, 0);
    InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
    return 1;
}

void gu_plant_bank_get_counters(
    struct GuPlantBankCounters *counters)
{
    if (!counters) {
        return;
    }
    counters->count_calls = (DWORD)InterlockedCompareExchange(
        &g_gu_plant_count_calls, 0, 0);
    counters->invalid_calls = (DWORD)InterlockedCompareExchange(
        &g_gu_plant_invalid_calls, 0, 0);
    counters->selected_calls = (DWORD)InterlockedCompareExchange(
        &g_gu_plant_selected_calls, 0, 0);
    counters->authored_count_total =
        (DWORD)InterlockedCompareExchange(
            &g_gu_plant_authored_count_total, 0, 0);
    counters->emitted_count_total =
        (DWORD)InterlockedCompareExchange(
            &g_gu_plant_emitted_count_total, 0, 0);
    counters->clamp_events = (DWORD)InterlockedCompareExchange(
        &g_gu_plant_clamp_events, 0, 0);
    counters->capacity_saturations =
        (DWORD)InterlockedCompareExchange(
            &g_gu_plant_capacity_saturations, 0, 0);
    counters->maximum_total_after =
        (DWORD)InterlockedCompareExchange(
            &g_gu_plant_maximum_total_after, 0, 0);
}

static int gu_runtime_is_idle(void)
{
    DWORD index;
    for (index = 0u; index < GU_PLANT_BANK_COUNT; ++index) {
        if (InterlockedCompareExchange(
                &g_gu_plant_runtime.bank_owner[index], 0, 0) != 0) {
            return 0;
        }
    }
    for (index = 0u; index < GU_PLANT_THREAD_CONTEXTS; ++index) {
        if (InterlockedCompareExchange(
                &g_gu_plant_runtime.contexts[index].owner_thread_id,
                0, 0) != 0) {
            return 0;
        }
    }
    return 1;
}

int gu_plant_bank_shutdown(void)
{
    BYTE *storage;
    if (InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 2, 2) != 2) {
        return 1;
    }
    /* Block new wrapper entries before proving that no old entry is live. */
    if (InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 1, 2) != 2) {
        return 0;
    }
    if (!gu_runtime_is_idle()) {
        InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
        return 0;
    }
    storage = g_gu_plant_runtime.storage;
    g_gu_plant_runtime.storage = NULL;
    InterlockedExchange(&g_gu_plant_runtime.selector_callback, 0);
    InterlockedExchange(&g_gu_plant_runtime.selector_user, 0);
    InterlockedExchange(&g_gu_plant_runtime.model_info_table, 0);
    if (storage) {
        VirtualFree(storage, 0u, MEM_RELEASE);
    }
    InterlockedExchange(&g_gu_plant_runtime.initialize_state, 0);
    return 1;
}

int gu_plant_bank_set_selector(
    GuPlantSelectionCallback callback, void *user_context)
{
    if (!callback || InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 1, 2) != 2) {
        return 0;
    }
    if (!gu_runtime_is_idle()) {
        InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
        return 0;
    }
    InterlockedExchange(
        &g_gu_plant_runtime.selector_user, (LONG)(DWORD)user_context);
    InterlockedExchange(
        &g_gu_plant_runtime.selector_callback, (LONG)(DWORD)callback);
    InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
    return 1;
}

void gu_plant_bank_clear_selector(void)
{
    if (InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 1, 2) != 2) {
        return;
    }
    if (!gu_runtime_is_idle()) {
        InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
        return;
    }
    InterlockedExchange(&g_gu_plant_runtime.selector_callback, 0);
    InterlockedExchange(&g_gu_plant_runtime.selector_user, 0);
    InterlockedExchange(&g_gu_plant_runtime.initialize_state, 2);
}

static struct GuPlantThreadContext *gu_thread_context(int create)
{
    struct GuPlantThreadContext *context;
    DWORD thread_id;
    DWORD index;
    if (InterlockedCompareExchange(
            &g_gu_plant_runtime.initialize_state, 2, 2) != 2) {
        return NULL;
    }
    thread_id = GetCurrentThreadId();
    if (thread_id == 0u) {
        return NULL;
    }
    /*
     * The contexts are already a bounded, allocation-free registry.  Looking
     * up the current owner directly removes the redundant Win32 TLS slot and
     * makes unrelated modules' TLS consumption irrelevant to PLANT startup.
     */
    for (index = 0u; index < GU_PLANT_THREAD_CONTEXTS; ++index) {
        context = &g_gu_plant_runtime.contexts[index];
        if ((DWORD)InterlockedCompareExchange(
                &context->owner_thread_id, 0, 0) == thread_id) {
            return context;
        }
    }
    if (!create) {
        return NULL;
    }
    for (index = 0u; index < GU_PLANT_THREAD_CONTEXTS; ++index) {
        context = &g_gu_plant_runtime.contexts[index];
        if (InterlockedCompareExchange(
                &context->owner_thread_id, (LONG)thread_id, 0) == 0) {
            context->token = index + 1u;
            context->depth = 0u;
            context->active_selector = GU_PLANT_SELECTOR_DISABLED;
            context->overflow_depth = 0u;
            context->overflow_previous = GU_PLANT_SELECTOR_DISABLED;
            return context;
        }
    }
    return NULL;
}

static void gu_release_idle_thread_context(
    struct GuPlantThreadContext *context)
{
    if (!context || context->depth != 0u || context->overflow_depth != 0u) {
        return;
    }
    context->active_selector = GU_PLANT_SELECTOR_DISABLED;
    InterlockedExchange(&context->owner_thread_id, 0);
}

int __stdcall gu_plant_bank_enter_frame(void)
{
    struct GuPlantThreadContext *context = gu_thread_context(1);
    DWORD selector = GU_PLANT_SELECTOR_DISABLED;
    DWORD index;
    if (!context) {
        return -1;
    }
    if (context->depth >= GU_PLANT_FRAME_DEPTH) {
        if (context->overflow_depth == 0u) {
            context->overflow_previous = context->active_selector;
            context->active_selector = GU_PLANT_SELECTOR_DISABLED;
        }
        ++context->overflow_depth;
        return -1;
    }
    for (index = 0u; index < GU_PLANT_BANK_COUNT; ++index) {
        if (InterlockedCompareExchange(
                &g_gu_plant_runtime.bank_owner[index],
                (LONG)context->token, 0) == 0) {
            selector = index;
            break;
        }
    }
    context->frames[context->depth].previous_selector =
        context->active_selector;
    context->frames[context->depth].leased_selector = selector;
    ++context->depth;
    context->active_selector = selector;
    return selector == GU_PLANT_SELECTOR_DISABLED ? -1 : (int)selector;
}

void __stdcall gu_plant_bank_leave_frame(void)
{
    struct GuPlantThreadContext *context = gu_thread_context(0);
    struct GuPlantFrame *frame;
    DWORD selector;
    if (!context) {
        return;
    }
    if (context->overflow_depth != 0u) {
        --context->overflow_depth;
        if (context->overflow_depth == 0u) {
            context->active_selector = context->overflow_previous;
        }
        return;
    }
    if (context->depth == 0u) {
        context->active_selector = GU_PLANT_SELECTOR_DISABLED;
        gu_release_idle_thread_context(context);
        return;
    }
    --context->depth;
    frame = &context->frames[context->depth];
    selector = frame->leased_selector;
    context->active_selector = frame->previous_selector;
    if (selector < GU_PLANT_BANK_COUNT &&
        InterlockedCompareExchange(
            &g_gu_plant_runtime.bank_owner[selector], 0,
            (LONG)context->token) != (LONG)context->token) {
        context->active_selector = GU_PLANT_SELECTOR_DISABLED;
    }
    if (context->depth == 0u) {
        gu_release_idle_thread_context(context);
    }
}

static DWORD gu_active_selector(void)
{
    struct GuPlantThreadContext *context = gu_thread_context(0);
    DWORD selector;
    if (!context) {
        return GU_PLANT_SELECTOR_DISABLED;
    }
    selector = context->active_selector;
    if (selector >= GU_PLANT_BANK_COUNT ||
        InterlockedCompareExchange(
            &g_gu_plant_runtime.bank_owner[selector], 0, 0) !=
            (LONG)context->token) {
        return GU_PLANT_SELECTOR_DISABLED;
    }
    return selector;
}

DWORD gu_plant_bank_active_capacity(void)
{
    return gu_active_selector() < GU_PLANT_BANK_COUNT ?
        GU_PLANT_BANK_CAPACITY : 0u;
}

void *gu_plant_bank_active_candidate_records(void)
{
    DWORD selector = gu_active_selector();
    return selector < GU_PLANT_BANK_COUNT ?
        gu_bank_base_unchecked(selector) +
            GU_PLANT_BANK_CANDIDATE_RECORDS_OFFSET : NULL;
}

void *gu_plant_bank_active_candidate_pointers(void)
{
    DWORD selector = gu_active_selector();
    return selector < GU_PLANT_BANK_COUNT ?
        gu_bank_base_unchecked(selector) +
            GU_PLANT_BANK_CANDIDATE_POINTERS_OFFSET : NULL;
}

void *gu_plant_bank_active_output_pointers(void)
{
    DWORD selector = gu_active_selector();
    return selector < GU_PLANT_BANK_COUNT ?
        gu_bank_base_unchecked(selector) +
            GU_PLANT_BANK_OUTPUT_POINTERS_OFFSET : NULL;
}

DWORD __stdcall gu_plant_bank_compute_selected_count(
    DWORD abi, const void *source_entity,
    const void *source_model_info, const void *descriptor,
    const void *saved_descriptor, const void *geometry_entry,
    DWORD submesh_index, DWORD authored_float_bits,
    DWORD current_total)
{
    struct GuPlantSelectionContext context;
    GuPlantSelectionCallback callback;
    void *user_context;
    DWORD multiplier_bits = gu_float_bits(1.0f);
    DWORD remaining;
    DWORD authored_count;
    DWORD scaled_count;
    DWORD emitted_count;
    DWORD total_after;
    float authored;
    float multiplier;
    float scaled;
    LONG source_model_index;
    const void *resolved_model_info = NULL;
    const DWORD *model_info_table = (const DWORD *)(DWORD)
        InterlockedCompareExchange(
            &g_gu_plant_runtime.model_info_table, 0, 0);
    InterlockedIncrement(&g_gu_plant_count_calls);
    if (gu_plant_bank_active_capacity() != GU_PLANT_BANK_CAPACITY ||
        current_total >= GU_PLANT_BANK_CAPACITY ||
        !gu_float_is_finite_nonnegative(authored_float_bits)) {
        InterlockedIncrement(&g_gu_plant_invalid_calls);
        return 0u;
    }
    ZeroMemory(&context, sizeof(context));
    context.abi = abi;
    context.source_entity = source_entity;
    context.source_model_info = source_model_info;
    context.descriptor = descriptor;
    context.geometry_entry = geometry_entry;
    context.submesh_index = submesh_index;
    context.authored_float_bits = authored_float_bits;
    context.current_total = current_total;
    if (model_info_table && source_entity && source_model_info && descriptor &&
        descriptor == saved_descriptor) {
        source_model_index = (LONG)*(const SHORT *)(
            (const BYTE *)source_entity + 0x2Eu);
        if (source_model_index >= 0 && source_model_index < 31000) {
            resolved_model_info = (const void *)(DWORD)
                model_info_table[(DWORD)source_model_index];
            if (resolved_model_info &&
                resolved_model_info == source_model_info) {
                context.geometry_type = 0x0Cu;
                context.source_model_index = (DWORD)source_model_index;
                context.source_model_hash = *(const DWORD *)(
                    (const BYTE *)resolved_model_info + 0x3Cu);
                context.output_model_registry_key = *(const DWORD *)(
                    (const BYTE *)descriptor + 0x18u);
                context.descriptor_field_14_bits = *(const DWORD *)(
                    (const BYTE *)descriptor + 0x14u);
                context.descriptor_field_1c_bits = *(const DWORD *)(
                    (const BYTE *)descriptor + 0x1Cu);
            }
        }
    }
    callback = (GuPlantSelectionCallback)(DWORD)
        InterlockedCompareExchange(
            &g_gu_plant_runtime.selector_callback, 0, 0);
    user_context = (void *)(DWORD)InterlockedCompareExchange(
        &g_gu_plant_runtime.selector_user, 0, 0);
    if (callback && context.geometry_type == 0x0Cu) {
        DWORD selected = callback(&context, user_context);
        float selected_float = gu_bits_float(selected);
        if (gu_float_is_finite_nonnegative(selected) &&
            selected_float >= GU_DENSITY_MULTIPLIER_MIN &&
            selected_float <= GU_DENSITY_MULTIPLIER_MAX) {
            multiplier_bits = selected;
        }
    }
    authored = gu_bits_float(authored_float_bits);
    multiplier = gu_bits_float(multiplier_bits);
    remaining = GU_PLANT_BANK_CAPACITY - current_total;
    if (authored <= 0.0f) {
        return 0u;
    }
    if (authored >= 2147483648.0f) {
        InterlockedIncrement(&g_gu_plant_invalid_calls);
        return 0u;
    }
    authored_count = (DWORD)authored;
    scaled = authored * multiplier;
    if (!gu_float_is_finite_nonnegative(gu_float_bits(scaled)) ||
        scaled >= 2147483648.0f) {
        InterlockedIncrement(&g_gu_plant_invalid_calls);
        return 0u;
    }
    scaled_count = (DWORD)scaled;
    emitted_count = scaled_count;
    if (emitted_count > remaining) {
        emitted_count = remaining;
        InterlockedIncrement(&g_gu_plant_clamp_events);
    }
    total_after = current_total + emitted_count;
    if (multiplier_bits != gu_float_bits(1.0f)) {
        InterlockedIncrement(&g_gu_plant_selected_calls);
    }
    InterlockedExchangeAdd(
        &g_gu_plant_authored_count_total, (LONG)authored_count);
    InterlockedExchangeAdd(
        &g_gu_plant_emitted_count_total, (LONG)emitted_count);
    if (total_after == GU_PLANT_BANK_CAPACITY) {
        InterlockedIncrement(&g_gu_plant_capacity_saturations);
    }
    gu_plant_update_maximum(
        &g_gu_plant_maximum_total_after, total_after);
    return emitted_count;
}

static void *__stdcall gu_resolve_active_address(
    DWORD kind, BYTE *stock_generator, DWORD index)
{
    BYTE *base;
    DWORD capacity;
    DWORD selector = gu_active_selector();
    if (selector < GU_PLANT_BANK_COUNT) {
        base = gu_bank_base_unchecked(selector);
        capacity = GU_PLANT_BANK_CAPACITY;
        if (kind == GU_RESOLVE_CANDIDATE_RECORD) {
            base += GU_PLANT_BANK_CANDIDATE_RECORDS_OFFSET;
        } else if (kind == GU_RESOLVE_CANDIDATE_POINTER_BASE) {
            base += GU_PLANT_BANK_CANDIDATE_POINTERS_OFFSET;
        } else {
            base += GU_PLANT_BANK_OUTPUT_POINTERS_OFFSET;
        }
    } else {
        capacity = 64u;
        if (!stock_generator) {
            return NULL;
        }
        if (kind == GU_RESOLVE_CANDIDATE_RECORD) {
            base = stock_generator + 0x3020u;
        } else if (kind == GU_RESOLVE_CANDIDATE_POINTER_BASE) {
            base = stock_generator + 0x3820u;
        } else {
            base = stock_generator + 0x5120u;
        }
    }
    if (index >= capacity) {
        index = 0u;
    }
    if (kind == GU_RESOLVE_CANDIDATE_RECORD) {
        return base + index * GU_PLANT_BANK_CANDIDATE_STRIDE;
    }
    if (kind == GU_RESOLVE_OUTPUT_POINTER_VALUE) {
        return ((void **)base)[index];
    }
    return base;
}

struct GuEmitter {
    BYTE *code;
    DWORD capacity;
    DWORD cursor;
    int ok;
};

static void gu_emit_byte(struct GuEmitter *emitter, BYTE value)
{
    if (!emitter || !emitter->ok || emitter->cursor >= emitter->capacity) {
        if (emitter) {
            emitter->ok = 0;
        }
        return;
    }
    emitter->code[emitter->cursor++] = value;
}

static void gu_emit_u32(struct GuEmitter *emitter, DWORD value)
{
    gu_emit_byte(emitter, (BYTE)(value & 0xFFu));
    gu_emit_byte(emitter, (BYTE)((value >> 8) & 0xFFu));
    gu_emit_byte(emitter, (BYTE)((value >> 16) & 0xFFu));
    gu_emit_byte(emitter, (BYTE)((value >> 24) & 0xFFu));
}

static void gu_emit_rel32(
    struct GuEmitter *emitter, BYTE opcode, const void *target)
{
    BYTE *instruction;
    DWORD relative;
    gu_emit_byte(emitter, opcode);
    if (!emitter->ok) {
        return;
    }
    instruction = emitter->code + emitter->cursor - 1u;
    if (!target || !gu_encode_rel32(
            (DWORD)instruction, 5u,
            (DWORD)target, &relative)) {
        emitter->ok = 0;
        return;
    }
    gu_emit_u32(emitter, relative);
}

static void gu_emit_push_ebx_offset(
    struct GuEmitter *emitter, DWORD offset)
{
    gu_emit_byte(emitter, 0xFFu);
    gu_emit_byte(emitter, 0xB3u);
    gu_emit_u32(emitter, offset);
}

static void gu_emit_store_eax_ebx_offset(
    struct GuEmitter *emitter, DWORD offset)
{
    gu_emit_byte(emitter, 0x89u);
    gu_emit_byte(emitter, 0x83u);
    gu_emit_u32(emitter, offset);
}

static void gu_emit_save_frame(struct GuEmitter *emitter)
{
    /* A plant detour is entered in the middle of an engine function.  Save
     * all callback-visible integer, flag, x87, MMX, MXCSR, and XMM0-7 state
     * before calling C.  A 528-byte reservation always contains one aligned
     * 512-byte FXSAVE area without overlapping the PUSHAD/PUSHFD frame. */
    gu_emit_byte(emitter, 0x9Cu); /* pushfd */
    gu_emit_byte(emitter, 0x60u); /* pushad */
    gu_emit_byte(emitter, 0x81u);
    gu_emit_byte(emitter, 0xECu); /* sub esp,528 */
    gu_emit_u32(emitter, GU_PLANT_FP_SAVE_BYTES);
    gu_emit_byte(emitter, 0x8Du);
    gu_emit_byte(emitter, 0x44u);
    gu_emit_byte(emitter, 0x24u);
    gu_emit_byte(emitter, 0x0Fu); /* lea eax,[esp+15] */
    gu_emit_byte(emitter, 0x83u);
    gu_emit_byte(emitter, 0xE0u);
    gu_emit_byte(emitter, 0xF0u); /* and eax,-16 */
    gu_emit_byte(emitter, 0x0Fu);
    gu_emit_byte(emitter, 0xAEu);
    gu_emit_byte(emitter, 0x00u); /* fxsave [eax] */
    gu_emit_byte(emitter, 0x8Bu);
    gu_emit_byte(emitter, 0xDCu); /* mov ebx,esp */
    gu_emit_byte(emitter, 0xFCu); /* cld: Win32 C ABI requires DF clear */
    /* Isolate callback arithmetic from an interrupted engine rounding mode
     * or pending unmasked exception.  FXRSTOR reinstates the exact state. */
    gu_emit_byte(emitter, 0xDBu);
    gu_emit_byte(emitter, 0xE3u); /* fninit */
    gu_emit_byte(emitter, 0x68u); /* push 0x00001F80 */
    gu_emit_u32(emitter, 0x00001F80u);
    gu_emit_byte(emitter, 0x0Fu);
    gu_emit_byte(emitter, 0xAEu);
    gu_emit_byte(emitter, 0x14u);
    gu_emit_byte(emitter, 0x24u); /* ldmxcsr [esp] */
    gu_emit_byte(emitter, 0x83u);
    gu_emit_byte(emitter, 0xC4u);
    gu_emit_byte(emitter, 0x04u); /* add esp,4 */
}

static void gu_emit_restore_frame(struct GuEmitter *emitter)
{
    gu_emit_byte(emitter, 0x8Bu);
    gu_emit_byte(emitter, 0xE3u); /* mov esp,ebx */
    gu_emit_byte(emitter, 0x8Du);
    gu_emit_byte(emitter, 0x44u);
    gu_emit_byte(emitter, 0x24u);
    gu_emit_byte(emitter, 0x0Fu); /* lea eax,[esp+15] */
    gu_emit_byte(emitter, 0x83u);
    gu_emit_byte(emitter, 0xE0u);
    gu_emit_byte(emitter, 0xF0u); /* and eax,-16 */
    gu_emit_byte(emitter, 0x0Fu);
    gu_emit_byte(emitter, 0xAEu);
    gu_emit_byte(emitter, 0x08u); /* fxrstor [eax] */
    gu_emit_byte(emitter, 0x81u);
    gu_emit_byte(emitter, 0xC4u); /* add esp,528 */
    gu_emit_u32(emitter, GU_PLANT_FP_SAVE_BYTES);
    gu_emit_byte(emitter, 0x61u); /* popad */
    gu_emit_byte(emitter, 0x9Du); /* popfd */
}

static int gu_finalize_stub(struct GuEmitter *emitter)
{
    DWORD old_protection = 0u;
    if (!emitter || !emitter->ok || emitter->cursor == 0u) {
        return 0;
    }
    return VirtualProtect(
               emitter->code, emitter->cursor,
               PAGE_EXECUTE_READ, &old_protection) != 0 &&
           FlushInstructionCache(
               GetCurrentProcess(), emitter->code,
               emitter->cursor) != 0;
}

static struct GuEmitter gu_new_emitter(void)
{
    struct GuEmitter emitter;
    emitter.code = (BYTE *)VirtualAlloc(
        NULL, GU_PLANT_STUB_BYTES,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    emitter.capacity = GU_PLANT_STUB_BYTES;
    emitter.cursor = 0u;
    emitter.ok = emitter.code != NULL;
    return emitter;
}

static BYTE *gu_build_count_stub(
    const struct GuPlantBankFamilyContract *contract,
    BYTE *continuation)
{
    struct GuEmitter e = gu_new_emitter();
    DWORD submesh_offset;
    DWORD geometry_offset;
    DWORD result_offset;
    if (!e.ok || !contract) {
        return e.code;
    }
    /* Preserve the exact authored multiply; C performs only selection/clamp. */
    gu_emit_byte(&e, contract->count_site.expected[0]);
    gu_emit_byte(&e, contract->count_site.expected[1]);
    gu_emit_byte(&e, contract->count_site.expected[2]);
    gu_emit_byte(&e, contract->count_site.expected[3]);
    gu_emit_save_frame(&e);
    if (contract->abi == GU_PLANT_BANK_ABI_CE_LEG) {
        submesh_offset = GU_SAVED_ESI;
        geometry_offset = GU_SAVED_EDI;
        result_offset = GU_SAVED_EDI;
    } else {
        submesh_offset = GU_SAVED_EDI;
        geometry_offset = GU_SAVED_ESI;
        result_offset = GU_SAVED_EAX;
    }
    gu_emit_push_ebx_offset(
        &e, GU_PLANT_STUB_SAVED_BYTES + contract->total_stack_offset);
    /* movd edx,xmm1 (CE/LEG) or movd edx,xmm0 (patch families). */
    gu_emit_byte(&e, 0x66u);
    gu_emit_byte(&e, 0x0Fu);
    gu_emit_byte(&e, 0x7Eu);
    gu_emit_byte(&e, contract->abi == GU_PLANT_BANK_ABI_CE_LEG ?
        0xCAu : 0xC2u);
    gu_emit_byte(&e, 0x52u); /* push edx authored bits */
    gu_emit_push_ebx_offset(&e, submesh_offset);
    gu_emit_push_ebx_offset(&e, geometry_offset);
    gu_emit_push_ebx_offset(
        &e, GU_PLANT_STUB_SAVED_BYTES +
                contract->saved_descriptor_stack_offset);
    gu_emit_push_ebx_offset(&e, GU_SAVED_EAX);
    gu_emit_push_ebx_offset(
        &e, GU_PLANT_STUB_SAVED_BYTES +
                contract->source_model_info_stack_offset);
    gu_emit_byte(&e, 0x8Bu);
    gu_emit_byte(&e, 0x83u); /* mov eax,[ebx+saved ebp] */
    gu_emit_u32(&e, GU_SAVED_EBP);
    gu_emit_byte(&e, 0xFFu);
    gu_emit_byte(&e, 0x70u);
    gu_emit_byte(&e, 0x08u); /* push [eax+8] source */
    gu_emit_byte(&e, 0x6Au);
    gu_emit_byte(&e, (BYTE)contract->abi);
    gu_emit_rel32(&e, 0xE8u, gu_plant_bank_compute_selected_count);
    gu_emit_store_eax_ebx_offset(&e, result_offset);
    gu_emit_restore_frame(&e);
    gu_emit_rel32(&e, 0xE9u, continuation);
    if (!gu_finalize_stub(&e)) {
        if (e.code) {
            VirtualFree(e.code, 0u, MEM_RELEASE);
        }
        return NULL;
    }
    return e.code;
}

static BYTE *gu_build_resolve_stub(
    DWORD kind, DWORD source_type, DWORD source_offset,
    int index_is_register, DWORD index_offset,
    DWORD result_offset, int ce_push_result,
    BYTE *continuation)
{
    struct GuEmitter e = gu_new_emitter();
    if (!e.ok) {
        return NULL;
    }
    gu_emit_save_frame(&e);
    if (index_is_register) {
        gu_emit_push_ebx_offset(&e, index_offset);
    } else {
        gu_emit_byte(&e, 0x6Au);
        gu_emit_byte(&e, 0x00u);
    }
    if (source_type == GU_SOURCE_REGISTER) {
        gu_emit_push_ebx_offset(&e, source_offset);
    } else {
        gu_emit_push_ebx_offset(
            &e, GU_PLANT_STUB_SAVED_BYTES + source_offset);
    }
    gu_emit_byte(&e, 0x6Au);
    gu_emit_byte(&e, (BYTE)kind);
    gu_emit_rel32(&e, 0xE8u, gu_resolve_active_address);
    if (!ce_push_result) {
        gu_emit_store_eax_ebx_offset(&e, result_offset);
        gu_emit_restore_frame(&e);
    } else {
        /*
         * Keep the callback result in the volatile saved EDX while EAX is
         * used to align the FXSAVE area.  Restore flags before replacing
         * their saved stack slot with the value the stock PUSH would have
         * placed there.  POPAD then leaves ESP at that value while restoring
         * every original GPR, including the source index in EAX.
         */
        gu_emit_byte(&e, 0x8Bu);
        gu_emit_byte(&e, 0xD0u); /* mov edx,eax */
        gu_emit_byte(&e, 0x8Bu);
        gu_emit_byte(&e, 0xE3u); /* mov esp,ebx */
        gu_emit_byte(&e, 0x8Du);
        gu_emit_byte(&e, 0x44u);
        gu_emit_byte(&e, 0x24u);
        gu_emit_byte(&e, 0x0Fu); /* lea eax,[esp+15] */
        gu_emit_byte(&e, 0x83u);
        gu_emit_byte(&e, 0xE0u);
        gu_emit_byte(&e, 0xF0u); /* and eax,-16 */
        gu_emit_byte(&e, 0x0Fu);
        gu_emit_byte(&e, 0xAEu);
        gu_emit_byte(&e, 0x08u); /* fxrstor [eax] */
        gu_emit_byte(&e, 0x81u);
        gu_emit_byte(&e, 0xC4u);
        gu_emit_u32(&e, GU_PLANT_FP_SAVE_BYTES);
        gu_emit_byte(&e, 0xFFu);
        gu_emit_byte(&e, 0x74u);
        gu_emit_byte(&e, 0x24u);
        gu_emit_byte(&e, 0x20u); /* push saved flags */
        gu_emit_byte(&e, 0x9Du); /* popfd */
        gu_emit_byte(&e, 0x89u);
        gu_emit_byte(&e, 0x54u);
        gu_emit_byte(&e, 0x24u);
        gu_emit_byte(&e, 0x20u); /* saved flags slot = result */
        gu_emit_byte(&e, 0x61u); /* popad; result remains pushed */
    }
    gu_emit_rel32(&e, 0xE9u, continuation);
    if (!gu_finalize_stub(&e)) {
        VirtualFree(e.code, 0u, MEM_RELEASE);
        return NULL;
    }
    return e.code;
}

static BYTE *gu_build_generation_wrapper(
    BYTE *generation_entry)
{
    struct GuEmitter e = gu_new_emitter();
    if (!e.ok) {
        return NULL;
    }
    gu_emit_byte(&e, 0x55u);       /* push ebp */
    gu_emit_byte(&e, 0x8Bu);
    gu_emit_byte(&e, 0xECu);       /* mov ebp,esp */
    /* Enter/leave are bookkeeping callbacks around an ordinary engine call.
     * Preserve the caller's complete incoming state across enter, then the
     * engine callee's complete return state across leave. */
    gu_emit_save_frame(&e);
    gu_emit_rel32(&e, 0xE8u, gu_plant_bank_enter_frame);
    gu_emit_restore_frame(&e);
    gu_emit_byte(&e, 0xFFu);
    gu_emit_byte(&e, 0x75u);
    gu_emit_byte(&e, 0x08u);       /* push copied arg */
    gu_emit_rel32(&e, 0xE8u, generation_entry);
    gu_emit_save_frame(&e);
    gu_emit_rel32(&e, 0xE8u, gu_plant_bank_leave_frame);
    gu_emit_restore_frame(&e);
    gu_emit_byte(&e, 0x8Bu);
    gu_emit_byte(&e, 0xE5u);       /* mov esp,ebp */
    gu_emit_byte(&e, 0x5Du);       /* pop ebp */
    gu_emit_byte(&e, 0xC2u);
    gu_emit_byte(&e, 0x04u);
    gu_emit_byte(&e, 0x00u);       /* ret 4 */
    if (!gu_finalize_stub(&e)) {
        VirtualFree(e.code, 0u, MEM_RELEASE);
        return NULL;
    }
    return e.code;
}

static int gu_validate_family_contract(
    BYTE *image_base, const struct GuPlantBankFamilyContract *family)
{
    DWORD index;
    const struct GuPatchContract *patched_sites[8];
    if (!image_base || !family ||
        (family->abi != GU_PLANT_BANK_ABI_CE_LEG &&
         family->abi != GU_PLANT_BANK_ABI_PATCH) ||
        family->count_site.length != 8u ||
        family->candidate_record_site.length < 5u ||
        family->candidate_pointer_site.length < 5u ||
        family->paired_output_read_site.length == 0u ||
        family->projector_output_site.length < 5u ||
        family->projector_candidate_site.length < 5u ||
        family->drain_output_site.length < 5u ||
        family->generation_call_site.length != 5u ||
        family->proof_site_count > GU_PLANT_BANK_MAX_PROOF_SITES) {
        return 0;
    }
    patched_sites[0] = &family->count_site;
    patched_sites[1] = &family->candidate_record_site;
    patched_sites[2] = &family->candidate_pointer_site;
    patched_sites[3] = &family->paired_output_read_site;
    patched_sites[4] = &family->projector_output_site;
    patched_sites[5] = &family->projector_candidate_site;
    patched_sites[6] = &family->drain_output_site;
    patched_sites[7] = &family->generation_call_site;
    for (index = 0u; index < 8u; ++index) {
        if (!gu_patch_contract_validate(image_base, patched_sites[index])) {
            return 0;
        }
    }
    for (index = 0u; index < family->proof_site_count; ++index) {
        if (!gu_patch_contract_validate(
                image_base, &family->proof_sites[index])) {
            return 0;
        }
    }
    return 1;
}

void gu_plant_bank_release_prepared(
    struct GuPlantBankPrepared *prepared)
{
    BYTE **stubs;
    DWORD index;
    if (!prepared) {
        return;
    }
    stubs = (BYTE **)(void *)prepared;
    for (index = 0u;
         index < sizeof(*prepared) / sizeof(BYTE *); ++index) {
        if (stubs[index]) {
            VirtualFree(stubs[index], 0u, MEM_RELEASE);
        }
    }
    ZeroMemory(prepared, sizeof(*prepared));
}

static void gu_transaction_truncate(
    struct GuPatchTransaction *transaction, DWORD original_count)
{
    if (!transaction || transaction->committed ||
        original_count > transaction->count) {
        return;
    }
    while (transaction->count > original_count) {
        --transaction->count;
        ZeroMemory(&transaction->patches[transaction->count],
                   sizeof(transaction->patches[0]));
    }
}

int gu_plant_bank_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPlantBankTargetContract *contract,
    struct GuPlantBankPrepared *prepared)
{
    const struct GuPlantBankFamilyContract *family;
    BYTE *model_info_table;
    DWORD original_count;
    if (!transaction || !image_base || !contract || !contract->family ||
        !contract->target_id || !contract->sha256 || !prepared ||
        !gu_plant_target_is_builtin(contract) ||
        !gu_plant_model_table_contract_valid(contract) ||
        (DWORD)(ULONG_PTR)image_base >
            0xFFFFFFFFu - contract->model_info_table_rva ||
        transaction->committed ||
        transaction->count + GU_PLANT_BANK_TRANSACTION_PATCHES >
            GU_TRANSACTION_MAX_PATCHES) {
        return 0;
    }
    ZeroMemory(prepared, sizeof(*prepared));
    family = contract->family;
    model_info_table = image_base + contract->model_info_table_rva;

    /* First pass: validate every write site and every proof anchor. */
    if (!gu_validate_family_contract(image_base, family) ||
        !gu_patch_contract_validate(
            image_base, &contract->model_info_table_load_site) ||
        !gu_plant_range_readable(
            model_info_table, 31000u * sizeof(DWORD)) ||
        !gu_plant_bank_initialize()) {
        return 0;
    }

    prepared->count_stub = gu_build_count_stub(
        family, image_base + family->count_site.rva +
                    family->count_site.length);
    if (family->abi == GU_PLANT_BANK_ABI_CE_LEG) {
        prepared->candidate_record_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_RECORD, GU_SOURCE_STACK, 0x34u,
            1, GU_SAVED_ECX, GU_SAVED_EAX, 0,
            image_base + family->candidate_record_site.rva +
                family->candidate_record_site.length);
        prepared->candidate_pointer_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_ESI, 0, 0u, GU_SAVED_ESI, 0,
            image_base + family->candidate_pointer_site.rva +
                family->candidate_pointer_site.length);
        prepared->projector_output_stub = gu_build_resolve_stub(
            GU_RESOLVE_OUTPUT_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_ECX, 0, 0u, GU_SAVED_EAX, 0,
            image_base + family->projector_output_site.rva +
                family->projector_output_site.length);
        prepared->projector_candidate_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_ECX, 0, 0u, GU_SAVED_EAX, 0,
            image_base + family->projector_candidate_site.rva +
                family->projector_candidate_site.length);
        prepared->drain_output_stub = gu_build_resolve_stub(
            GU_RESOLVE_OUTPUT_POINTER_VALUE, GU_SOURCE_REGISTER,
            GU_SAVED_ECX, 1, GU_SAVED_EAX, 0u, 1,
            image_base + family->drain_output_site.rva +
                family->drain_output_site.length);
    } else {
        prepared->candidate_record_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_RECORD, GU_SOURCE_STACK, 0x2Cu,
            1, GU_SAVED_EAX, GU_SAVED_EDI, 0,
            image_base + family->candidate_record_site.rva +
                family->candidate_record_site.length);
        prepared->candidate_pointer_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_ESI, 0, 0u, GU_SAVED_ESI, 0,
            image_base + family->candidate_pointer_site.rva +
                family->candidate_pointer_site.length);
        prepared->projector_output_stub = gu_build_resolve_stub(
            GU_RESOLVE_OUTPUT_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_EAX, 0, 0u, GU_SAVED_ECX, 0,
            image_base + family->projector_output_site.rva +
                family->projector_output_site.length);
        prepared->projector_candidate_stub = gu_build_resolve_stub(
            GU_RESOLVE_CANDIDATE_POINTER_BASE, GU_SOURCE_REGISTER,
            GU_SAVED_EAX, 0, 0u, GU_SAVED_EAX, 0,
            image_base + family->projector_candidate_site.rva +
                family->projector_candidate_site.length);
        prepared->drain_output_stub = gu_build_resolve_stub(
            GU_RESOLVE_OUTPUT_POINTER_VALUE, GU_SOURCE_REGISTER,
            GU_SAVED_EAX, 1, GU_SAVED_ECX, GU_SAVED_EDX, 0,
            image_base + family->drain_output_site.rva +
                family->drain_output_site.length);
    }
    prepared->generation_wrapper_stub = gu_build_generation_wrapper(
        image_base + family->generation_entry_rva);
    if (!prepared->count_stub || !prepared->candidate_record_stub ||
        !prepared->candidate_pointer_stub || !prepared->projector_output_stub ||
        !prepared->projector_candidate_stub || !prepared->drain_output_stub ||
        !prepared->generation_wrapper_stub) {
        gu_plant_bank_release_prepared(prepared);
        return 0;
    }

    /* Second pass adds descriptions only; GuPatchTransaction writes nothing. */
    original_count = transaction->count;
    if (!gu_patch_transaction_add_jump(
            transaction, image_base, &family->count_site,
            prepared->count_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, &family->candidate_record_site,
            prepared->candidate_record_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, &family->candidate_pointer_site,
            prepared->candidate_pointer_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, &family->projector_output_site,
            prepared->projector_output_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, &family->projector_candidate_site,
            prepared->projector_candidate_stub) ||
        !gu_patch_transaction_add_jump(
            transaction, image_base, &family->drain_output_site,
            prepared->drain_output_stub) ||
        !gu_patch_transaction_add_call(
            transaction, image_base, &family->generation_call_site,
            prepared->generation_wrapper_stub)) {
        gu_transaction_truncate(transaction, original_count);
        gu_plant_bank_release_prepared(prepared);
        return 0;
    }
    if (!gu_runtime_is_idle()) {
        gu_transaction_truncate(transaction, original_count);
        gu_plant_bank_release_prepared(prepared);
        return 0;
    }
    /*
     * Publish only the exact loaded-image table proven by the target-specific
     * load preimage.  Hot classification never trusts the saved stack pointer
     * until this table resolves the same modelInfo for the current index.
     */
    InterlockedExchange(
        &g_gu_plant_runtime.model_info_table,
        (LONG)(DWORD)(ULONG_PTR)model_info_table);
    return 1;
}
