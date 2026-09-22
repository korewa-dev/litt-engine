// Retired accelerated Dither3D compatibility translation unit.
//
// Dither3D is implemented by the dependency-free generated-pattern and CPU
// post-process contract in litt_dither.cpp / litt_dither_renderer.cpp.
// Accelerated API backends are capability-gated in litt_gpu.h and are not
// duplicated here.
#include "litt_dither.h"

namespace litt {
const char* dither_accelerated_compatibility_note() {
    return "use DitherProcessor with the supported software renderer";
}
} // namespace litt
