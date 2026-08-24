//! C ABI surface of the Litt engine.
//!
//! Lets any C++ engine (Unreal, custom renderers, Godot native modules)
//! load a generated `.lscn.json` world and deploy it through the same
//! pipeline the native player uses: parse -> areas -> OBJ meshes ->
//! pathtracer scene. No Rust knowledge required on the other side.
//!
//! See `include/litt_ffi.h` for the contract and
//! `examples/cpp/load_world.cpp` for a working consumer.

#![deny(unsafe_op_in_unsafe_fn)]

use std::ffi::{c_char, c_int, CStr, CString};
use std::ptr;

/// Opaque deployed-world handle.
pub struct LittWorld {
    tri_count: usize,
    sphere_count: usize,
    meshes_loaded: usize,
    missing: Vec<CString>,
}

/// Engine version string, static, never freed.
#[no_mangle]
pub extern "C" fn litt_version() -> *const c_char {
    concat!(env!("CARGO_PKG_NAME"), " ", env!("CARGO_PKG_VERSION"), "\0")
        .as_ptr()
        .cast()
}

/// Deploy a world. Returns an opaque handle or null on failure.
///
/// - `scene_path`: UTF-8 path to `world.lscn.json`
/// - `assets_base`: UTF-8 folder that contains `models/`
/// - `out_error`: optional buffer (>=256 bytes) receiving a message on null
#[no_mangle]
pub unsafe extern "C" fn litt_deploy_world(
    scene_path: *const c_char,
    assets_base: *const c_char,
    out_error: *mut c_char,
) -> *mut LittWorld {
    unsafe fn inner(
        scene_path: *const c_char,
        assets_base: *const c_char,
        _out_error: *mut c_char,
    ) -> Result<*mut LittWorld, String> {
        let scene_c = unsafe { CStr::from_ptr(scene_path) };
        let base_c = unsafe { CStr::from_ptr(assets_base) };
        let scene_path = scene_c.to_str().map_err(|e| e.to_string())?;
        let assets_base = base_c.to_str().map_err(|e| e.to_string())?;

        let (graph, _areas) =
            litt_scene::load_graph_and_areas_file(scene_path)?;
        let (scene, stats) = litt::world_bridge::build_render_scene(&graph, assets_base);

        Ok(Box::into_raw(Box::new(LittWorld {
            tri_count: scene.triangles.len(),
            sphere_count: scene.spheres.len(),
            meshes_loaded: stats.meshes_loaded,
            missing: stats
                .missing_models
                .iter()
                .filter_map(|m| CString::new(m.as_str()).ok())
                .collect(),
        })))
    }

    match unsafe { inner(scene_path, assets_base, out_error) } {
        Ok(h) => h,
        Err(e) => {
            if !out_error.is_null() {
                let msg = CString::new(e).unwrap_or_else(|_| CString::new("error").unwrap());
                let bytes = msg.as_bytes_with_nul();
                let dst = unsafe {
                    std::slice::from_raw_parts_mut(out_error.cast::<u8>(), 256.min(bytes.len()))
                };
                dst.copy_from_slice(&bytes[..dst.len()]);
            }
            ptr::null_mut()
        }
    }
}

/// Triangle count of a deployed world.
///
/// # Safety
/// `w` must be null or a valid handle from [`litt_deploy_world`].
#[no_mangle]
pub unsafe extern "C" fn litt_world_triangles(w: *const LittWorld) -> usize {
    unsafe { w.as_ref().map_or(0, |w| w.tri_count) }
}

/// Sphere (marker light/emitter proxy) count.
///
/// # Safety
/// `w` must be null or a valid handle from [`litt_deploy_world`].
#[no_mangle]
pub unsafe extern "C" fn litt_world_spheres(w: *const LittWorld) -> usize {
    unsafe { w.as_ref().map_or(0, |w| w.sphere_count) }
}

/// Number of OBJ meshes successfully loaded.
///
/// # Safety
/// `w` must be null or a valid handle from [`litt_deploy_world`].
#[no_mangle]
pub unsafe extern "C" fn litt_world_meshes(w: *const LittWorld) -> usize {
    unsafe { w.as_ref().map_or(0, |w| w.meshes_loaded) }
}

/// Number of models referenced by the scene but missing on disk.
///
/// # Safety
/// `w` must be null or a valid handle from [`litt_deploy_world`].
#[no_mangle]
pub unsafe extern "C" fn litt_world_missing_count(w: *const LittWorld) -> c_int {
    unsafe { w.as_ref().map_or(0, |w| w.missing.len()) as c_int }
}

/// Copy missing-model name `i` into `buf` (capacity `cap`). Returns bytes written.
#[no_mangle]
pub unsafe extern "C" fn litt_world_missing_at(
    w: *const LittWorld,
    i: c_int,
    buf: *mut c_char,
    cap: usize,
) -> c_int {
    let Some(world) = (unsafe { w.as_ref() }) else { return -1 };
    let Some(name) = world.missing.get(i as usize) else { return -1 };
    let n = cap.min(name.as_bytes_with_nul().len());
    unsafe {
        ptr::copy_nonoverlapping(name.as_ptr(), buf, n);
    }
    n as c_int
}

/// Free a deployed world handle.
///
/// # Safety
/// `w` must come from [`litt_deploy_world`] and must not be used afterwards.
#[no_mangle]
pub unsafe extern "C" fn litt_world_free(w: *mut LittWorld) {
    if !w.is_null() {
        drop(unsafe { Box::from_raw(w) });
    }
}
