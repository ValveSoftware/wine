#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#define GOLIATH_PLATFORM_WINDOWS 1
#else
#define GOLIATH_PLATFORM_WINDOWS 0
#endif

#if defined(__APPLE__)
#define GOLIATH_PLATFORM_DARWIN 1
#else
#define GOLIATH_PLATFORM_DARWIN 0
#endif

#if defined(__linux__)
#define GOLIATH_PLATFORM_LINUX 1
#else
#define GOLIATH_PLATFORM_LINUX 0
#endif

// Detect WSL
#if GOLIATH_PLATFORM_LINUX
#include <unistd.h>
static int is_wsl() {
    FILE *f = fopen("/proc/version", "r");
    if (!f) return 0;
    char buf[256];
    int found = 0;
    if (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "Microsoft") || strstr(buf, "WSL")) found = 1;
    }
    fclose(f);
    return found;
}
#else
static int is_wsl() { return 0; }
#endif

// Detect browser/unknown
#if defined(__EMSCRIPTEN__)
#define GOLIATH_PLATFORM_BROWSER 1
#else
#define GOLIATH_PLATFORM_BROWSER 0
#endif

// Usage example:
// if (GOLIATH_PLATFORM_WINDOWS) { /* enable Wine */ }
// if (GOLIATH_PLATFORM_DARWIN) { /* enable Darling */ }
// if (GOLIATH_PLATFORM_LINUX && is_wsl()) { /* enable WSL */ }
// if (GOLIATH_PLATFORM_BROWSER) { /* enable all */ }
