#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

/* Returns true when appID is listed in the OWNED_DLCS environment
 * variable (comma-separated list of decimal AppIDs). Used to short-
 * circuit BIsDlcInstalled when the host knows DLC ownership but the
 * native libsteamclient.so has failed to reflect it into the daemon's
 * PICS cache. */
bool is_owned_dlc(uint32_t appID);

#ifdef __cplusplus
}
#endif
