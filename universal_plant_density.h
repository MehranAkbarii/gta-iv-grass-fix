#ifndef GTAIV_GRASS_UNIVERSAL_PLANT_DENSITY_H
#define GTAIV_GRASS_UNIVERSAL_PLANT_DENSITY_H

#include <windows.h>
#include "universal_hooks.h"

enum GuPlantCountAbi {
    GU_PLANT_COUNT_ABI_NONE = 0,
    GU_PLANT_COUNT_ABI_CE = 1,
    GU_PLANT_COUNT_ABI_PATCH = 2
};

struct GuPlantCountContract {
    DWORD abi;
    struct GuPatchContract count_site;
    DWORD total_stack_offset;
    DWORD candidate_capacity;
};

extern volatile LONG g_gu_plant_density_multiplier_bits;

int gu_plant_count_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPlantCountContract *contract,
    float multiplier, BYTE **stub_out);
void gu_plant_count_release_stub(BYTE *stub);
int gu_plant_count_set_active_multiplier(float multiplier);
void gu_plant_count_reset_active_multiplier(void);

#endif
