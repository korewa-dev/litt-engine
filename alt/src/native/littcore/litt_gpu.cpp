// LittGPU implementation for the release device factory.
//
// The supported runtime exposes the software backend and a test-only null
// backend. Accelerated API names are capability entries that fail explicitly;
// there are no macro-gated placeholder device implementations in the release
// source path.

#include "litt_gpu.h"
#include "litt_gpu_software.h"

#include <stdexcept>

namespace litt {

std::unique_ptr<IGPUDevice> create_gpu_device(const std::string& backend_name) {
    if (backend_name == "null") {
        return std::make_unique<NullGPUDevice>();
    }
    if (backend_name == "software" || backend_name == "auto") {
        return std::make_unique<SoftwareRenderer>();
    }

    const GPUBackendCapability capability = gpu_backend_capability(backend_name);
    if (capability.name) {
        throw std::runtime_error(
            std::string("GPU backend '") + capability.name +
            "' is not part of the release renderer: " + capability.reason);
    }
    throw std::invalid_argument("Unknown GPU backend: " + backend_name);
}

} // namespace litt
