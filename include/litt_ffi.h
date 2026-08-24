// C contract for the Litt engine FFI bridge (see crates/ffi/src/lib.rs).
// Any C++ engine can link litt_ffi.dll and deploy generated worlds natively.
#ifndef LITT_FFI_H
#define LITT_FFI_H

#include <cstddef>

#ifdef _WIN32
#define LITT_API extern "C" __declspec(dllimport)
#else
#define LITT_API extern "C"
#endif

struct LittWorld;

LITT_API const char* litt_version();

LITT_API LittWorld* litt_deploy_world(const char* scene_path,
                                      const char* assets_base,
                                      char* out_error /* >=256 bytes, nullable */);

LITT_API size_t litt_world_triangles(const LittWorld*);
LITT_API size_t litt_world_spheres(const LittWorld*);
LITT_API size_t litt_world_meshes(const LittWorld*);

LITT_API int  litt_world_missing_count(const LittWorld*);
LITT_API int  litt_world_missing_at(const LittWorld*, int index,
                                    char* buf, size_t cap);

LITT_API void litt_world_free(LittWorld*);

#endif // LITT_FFI_H
