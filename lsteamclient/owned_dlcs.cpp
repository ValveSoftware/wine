/* owned_dlcs.cpp
 *
 * Bridges a host-side ownership list into lsteamclient running inside
 * the Wine prefix. The host knows which DLCs the user owns (from the
 * Steam license/PICS payload) but native libsteamclient.so on this
 * build can't always reflect that into the daemon's PICS cache (the
 * "HasDepotsInDLC" merge step skips apps where Steam's CDN didn't ship
 * that flag, e.g. Vampire Survivors). So we override BIsDlcInstalled
 * to use the host's list as ground truth.
 *
 * Format: OWNED_DLCS=<appid>[,<appid>...]
 */

#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "owned_dlcs.h"

extern "C" {

static pthread_once_t  owned_dlcs_once  = PTHREAD_ONCE_INIT;
static uint32_t       *owned_dlcs       = NULL;
static size_t          owned_dlcs_count = 0;

static void init_owned_dlcs(void)
{
    const char *env = getenv("OWNED_DLCS");
    if (!env || !*env) return;

    size_t max = 1;
    for (const char *p = env; *p; p++) if (*p == ',') max++;

    owned_dlcs = (uint32_t *)calloc(max, sizeof(uint32_t));
    if (!owned_dlcs) return;

    char *dup = strdup(env);
    if (!dup) return;

    char  *saveptr = NULL;
    char  *tok     = strtok_r(dup, ",", &saveptr);
    size_t n       = 0;
    while (tok && n < max)
    {
        uint32_t v = (uint32_t)strtoul(tok, NULL, 10);
        if (v) owned_dlcs[n++] = v;
        tok = strtok_r(NULL, ",", &saveptr);
    }
    owned_dlcs_count = n;

    free(dup);
}

bool is_owned_dlc(uint32_t appID)
{
    pthread_once(&owned_dlcs_once, init_owned_dlcs);
    for (size_t i = 0; i < owned_dlcs_count; i++)
    {
        if (owned_dlcs[i] == appID) return true;
    }
    return false;
}

} /* extern "C" */
