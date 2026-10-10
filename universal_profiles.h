#ifndef GTAIV_GRASS_UNIVERSAL_PROFILES_H
#define GTAIV_GRASS_UNIVERSAL_PROFILES_H

#include <windows.h>
#include "universal_install.h"

const struct GuUniversalExecutableProfile *
gu_universal_profile_by_identity(DWORD identity_id);
DWORD gu_universal_profile_count(void);
int gu_universal_profile_fixture(void);

#endif
