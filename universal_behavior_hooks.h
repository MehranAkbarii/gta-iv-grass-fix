#ifndef GTAIV_GRASS_UNIVERSAL_BEHAVIOR_HOOKS_H
#define GTAIV_GRASS_UNIVERSAL_BEHAVIOR_HOOKS_H

#include <windows.h>
#include "universal_hooks.h"

enum GuCommitHookAbi {
    GU_COMMIT_ABI_NONE = 0,
    GU_COMMIT_ABI_CE_PLANT = 1,
    GU_COMMIT_ABI_PATCH_PLANT = 2,
    GU_COMMIT_ABI_CE_PROCOBJ = 3,
    GU_COMMIT_ABI_PATCH_PROCOBJ = 4
};

enum GuReleaseObserverAbi {
    GU_RELEASE_OBSERVER_ABI_NONE = 0,
    GU_RELEASE_OBSERVER_ABI_CE = 1,
    GU_RELEASE_OBSERVER_ABI_PATCH = 2
};

struct GuManagerPrebuildContract {
    struct GuPatchContract call_site;
    struct GuPatchContract rebuild_entry;
};

struct GuCommitHookContract {
    DWORD abi;
    struct GuPatchContract site;
};

struct GuReleaseObserverContract {
    DWORD abi;
    struct GuPatchContract site;
};

typedef void (__stdcall *GuDefinitionLoadedCallback)(void);

int gu_manager_prebuild_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuManagerPrebuildContract *contract,
    const void *fastcall_prebuild_hook,
    BYTE **stub_out);
int gu_commit_hook_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuCommitHookContract *contract,
    const void *fastcall_commit_hook,
    BYTE **stub_out);
int gu_definition_loaded_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuPatchContract *call_contract,
    GuDefinitionLoadedCallback callback,
    BYTE **stub_out);
int gu_release_observer_prepare_transaction(
    struct GuPatchTransaction *transaction,
    BYTE *image_base,
    const struct GuReleaseObserverContract *contract,
    const void *fastcall_release_observer,
    BYTE **stub_out);
void gu_behavior_hook_release_stub(BYTE *stub);

#endif
