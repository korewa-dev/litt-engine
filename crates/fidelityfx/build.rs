//! Build script for litt-fidelityfx.
//!
//! Compiles GLSL shader sources to SPIR-V using glslangValidator / glslc.
//! If the compiler is not available, the GLSL source is embedded as strings
//! and the runtime falls back to a pass-through no-op pipeline.
//!
//! Usage:
//!   cargo build                        # normal build, no SPIR-V
//!   GLSLANG_PATH=/path/to/glslang cargo build  # embeds real SPIR-V

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

const SHADER_SOURCES: &[(&str, &str)] = &[
    ("fsr3_upscaler.comp", "FSR3_UPSCALER_SPIR_V"),
    ("fsr3_compensate.comp", "FSR3_COMPENSATE_SPIR_V"),
    ("fsr3_create.comp", "FSR3_CREATE_SPIR_V"),
    ("fsr3_framegen.comp", "FSR3_FRAMEGEN_SPIR_V"),
    ("cas.comp", "CAS_SPIR_V"),
    ("ray_recon.comp", "RAY_RECON_SPIR_V"),
    ("path_trace.comp",  "PATH_TRACE_SPIR_V"),
    ("display.comp",     "DISPLAY_SPIR_V"),
];

fn main() {
    println!("cargo:rerun-if-changed=src/shaders/");

    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = PathBuf::from(out_dir);

    // Try to find glslangValidator
    let glslang = find_glslang();

    if let Some(ref compiler) = glslang {
        println!("cargo:warning=Compiling GLSL shaders with {}", compiler.display());
        for (src_file, const_name) in SHADER_SOURCES {
            let src = Path::new("src/shaders").join(src_file);
            let dst = out_path.join(format!("{const_name}.spv"));
            if src.exists() {
                compile_shader(compiler, &src, &dst);
            }
        }
    } else {
        println!(
            "cargo:warning=glslangValidator not found -- shaders will use runtime GLSL source (no SPIR-V)"
        );
    }
}

fn find_glslang() -> Option<PathBuf> {
    // Check common locations (glslangValidator <=15, unified `glslang` >=16)
    let candidates = [
        "glslangValidator",
        "glslc",
        "C:/Program Files/glslang/Build/bin/glslangValidator.exe",
        "C:/Program Files (x86)/glslang/Build/bin/glslangValidator.exe",
        "D:/Allgemein/tools/glslang/bin/glslang.exe",
        "C:/Program Files/glslang/Build/bin/glslang.exe",
    ];
    for candidate in &candidates {
        if Command::new(candidate)
            .arg("--version")
            .output()
            .is_ok()
        {
            return Some(PathBuf::from(candidate));
        }
    }
    // Check GLSLANG_PATH / GLSLC_PATH env vars
    for var in ["GLSLANG_PATH", "GLSLC_PATH"] {
        if let Ok(path) = env::var(var) {
            let p = PathBuf::from(path);
            if p.exists() {
                return Some(p);
            }
        }
    }
    None
}

fn compile_shader(compiler: &Path, src: &Path, dst: &Path) {
    let name = compiler
        .file_name()
        .map(|n| n.to_string_lossy().to_lowercase())
        .unwrap_or_default();
    let mut cmd = Command::new(compiler);
    // glslc defaults to Vulkan SPIR-V; standalone glslang needs explicit -V.
    if !name.starts_with("glslc") {
        cmd.arg("-V");
    }
    let output = cmd
        .arg(src)
        .arg("-o")
        .arg(dst)
        .output();

    match output {
        Ok(out) if out.status.success() => {
            println!("cargo:warning=Compiled {}", src.file_name().unwrap().to_string_lossy());
        }
        Ok(out) => {
            eprintln!(
                "cargo:warning=Failed to compile {}: {}",
                src.display(),
                String::from_utf8_lossy(&out.stderr)
            );
        }
        Err(e) => {
            eprintln!("cargo:warning=Could not run shader compiler: {e}");
        }
    }
}
