//! Build script for litt-platform.
//! Compiles GLSL MUSA compute shaders to SPIR-V when glslangValidator is available.
//! Falls back to embedded GLSL source strings when unavailable.

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=src/shaders/");

    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = PathBuf::from(out_dir);

    let glslang = find_glslang();

    if let Some(ref compiler) = glslang {
        println!("cargo:warning=[MUSA] Compiling shaders with {}", compiler.display());
        for src_file in &["musa_dotprod.comp", "musa_vectoradd.comp"] {
            let src = Path::new("src/shaders").join(src_file);
            let dst = out_path.join(format!("{}_spv", src_file.trim_end_matches(".comp")));
            if src.exists() {
                compile_shader(compiler, &src, &dst);
            }
        }
    } else {
        println!(
            "cargo:warning=[MUSA] glslangValidator not found -- shaders use runtime GLSL source"
        );
    }
}

fn find_glslang() -> Option<PathBuf> {
    for candidate in &["glslangValidator", "glslc",
        "C:/Program Files/glslang/Build/bin/glslangValidator.exe",
        "C:/Program Files (x86)/glslang/Build/bin/glslangValidator.exe"]
    {
        if Command::new(candidate).arg("--version").output().is_ok() {
            return Some(PathBuf::from(candidate));
        }
    }
    env::var("GLSLANG_PATH").ok().map(|p| PathBuf::from(p))
}

fn compile_shader(compiler: &Path, src: &Path, dst: &Path) {
    let output = Command::new(compiler)
        .arg(src).arg("-o").arg(dst)
        .output();
    match output {
        Ok(out) if out.status.success() => {
            println!("cargo:warning=[MUSA] Compiled {}", src.file_name().unwrap().to_string_lossy());
        }
        Ok(out) => eprintln!("cargo:warning=[MUSA] Failed {}: {}", src.display(), String::from_utf8_lossy(&out.stderr)),
        Err(e) => eprintln!("cargo:warning=[MUSA] Cannot run compiler: {}", e),
    }
}
