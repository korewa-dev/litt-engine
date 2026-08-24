//! Build script for litt-physics RDNA shaders.
//! Compiles GLSL compute shaders to SPIR-V when glslangValidator is available.

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

const SHADER_SOURCES: &[&str] = &[
    "rdna_wave32_broadphase.comp",
    "rdna_subgroup_ballot.comp",
    "rdna_bvh_reuse.comp",
    "rdna_rt_rayquery.comp",
];

fn main() {
    println!("cargo:rerun-if-changed=src/shaders/");

    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = PathBuf::from(out_dir);

    let glslang = find_glslang();

    if let Some(ref compiler) = glslang {
        println!("cargo:warning=[RDNA] Compiling shaders with {}", compiler.display());
        for src_file in SHADER_SOURCES {
            let src = Path::new("src/shaders").join(src_file);
            let dst = out_path.join(format!("{}_spv", src_file.trim_end_matches(".comp")));
            if src.exists() {
                compile_shader(compiler, &src, &dst);
            }
        }
    } else {
        println!(
            "cargo:warning=[RDNA] glslangValidator not found -- shaders use runtime GLSL source"
        );
    }
}

fn find_glslang() -> Option<PathBuf> {
    for candidate in &[
        "glslangValidator",
        "glslc",
        "C:/Program Files/glslang/Build/bin/glslangValidator.exe",
        "C:/Program Files (x86)/glslang/Build/bin/glslangValidator.exe",
    ] {
        if Command::new(candidate).arg("--version").output().is_ok() {
            return Some(PathBuf::from(candidate));
        }
    }
    env::var("GLSLANG_PATH").ok().map(PathBuf::from)
}

fn compile_shader(compiler: &Path, src: &Path, dst: &Path) {
    let output = Command::new(compiler)
        .arg(src).arg("-o").arg(dst)
        .output();
    match output {
        Ok(out) if out.status.success() => {
            println!("cargo:warning=[RDNA] Compiled {}", src.file_name().unwrap().to_string_lossy());
        }
        Ok(out) => eprintln!(
            "cargo:warning=[RDNA] Failed {}: {}",
            src.display(),
            String::from_utf8_lossy(&out.stderr)
        ),
        Err(e) => eprintln!("cargo:warning=[RDNA] Cannot run compiler: {e}"),
    }
}
