//! HLSL -> DXIL compilation via DXC (stub).
//!
//! Real implementation shells out to `dxc.exe` (like the asset crate does for
//! GLSL) or links `dxcompiler.dll`. DXIL signing requires dxc's validator.

use std::path::PathBuf;
use std::process::Command;

/// Shader model used for compilation.
pub const SHADER_MODEL: &str = "6_5";

/// Locate dxc.exe on PATH.
pub fn find_dxc() -> Option<PathBuf> {
    let exts: &[&str] = if cfg!(windows) { &[".exe", ""] } else { &[] };
    let path_var = std::env::var_os("PATH")?;
    for dir in std::env::split_paths(&path_var) {
        for ext in exts {
            let candidate = dir.join(format!("dxc{}", ext));
            if candidate.is_file() {
                return Some(candidate);
            }
        }
    }
    None
}

/// Compile HLSL source to DXIL bytecode. Stub (returns NotImplemented even
/// when dxc exists, until argument handling is validated end-to-end).
pub fn compile_hlsl(source: &str, entry: &str, stage: &str) -> Result<Vec<u8>, String> {
    let _ = (source, entry);
    let target = format!("{}_{}", stage_prefix(stage), SHADER_MODEL);
    match find_dxc() {
        Some(dxc) => {
            let _ = Command::new(dxc).arg("--version").output();
            Err(format!(
                "DXC found but DX12 pipeline not wired yet (target {})",
                target
            ))
        }
        None => Err("dxc.exe not found on PATH".to_string()),
    }
}

fn stage_prefix(stage: &str) -> &'static str {
    match stage.to_ascii_lowercase().as_str() {
        "vertex" => "vs",
        "pixel" | "fragment" => "ps",
        "compute" => "cs",
        "geometry" => "gs",
        "hull" => "hs",
        "domain" => "ds",
        _ => "cs",
    }
}
