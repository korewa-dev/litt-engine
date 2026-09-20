// Legacy FFI deployment example.
//
// World deployment through litt_ffi is not part of the release-supported Litt
// runtime contract and currently reports "not implemented". This source remains
// only as a compatibility/reference example. For a working generated-game path,
// follow alt/docs/SUPPORTED_RUNTIME.md and GAME_BUILD_PROTOCOL.md.
//
// Dither3D helper functions in litt_ffi remain usable independently.

#include "litt_ffi.h"
#include <cstdio>

int main() {
    std::fprintf(stderr,
        "This legacy FFI world-deployment example is unavailable. "
        "Use the native generated-game runtime; see alt/docs/SUPPORTED_RUNTIME.md.\n");
    return 2;
}
