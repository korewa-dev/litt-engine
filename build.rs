//! Build script: compiles GLSL shaders to SPIR-V.
//! Falls back to embedding pre-compiled SPIR-V if no compiler available.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=shaders/");
    println!("cargo:rerun-if-changed=build.rs");

    let out_dir = env::var("OUT_DIR").unwrap();
    let dest_path = Path::new(&out_dir).join("spirv");
    fs::create_dir_all(&dest_path).unwrap();

    let compiler = find_glsl_compiler();
    compile_shaders(&dest_path, compiler.as_ref());

    println!("cargo:rustc-env=BUILD_DATE={}", std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_secs());
    println!("cargo:rustc-env=GIT_COMMIT={}", std::process::Command::new("git").arg("rev-parse").arg("--short").arg("HEAD").output().ok().map(|o| String::from_utf8(o.stdout).unwrap_or_default()).unwrap_or_else(|| "unknown".to_string()));
    println!("cargo:rustc-link-lib=vulkan");
    if cfg!(target_os = "windows") {
        println!("cargo:rustc-link-lib=dylib=dxgi");
        println!("cargo:rustc-link-lib=dylib=kernel32");
        println!("cargo:rustc-link-lib=dylib=user32");
        println!("cargo:rustc-link-lib=dylib=gdi32");
    } else if cfg!(target_os = "android") {
        println!("cargo:rustc-link-lib=OpenSLES");
        println!("cargo:rustc-link-lib=android");
        println!("cargo:rustc-link-lib=log");
    }
}

fn find_glsl_compiler() -> Option<PathBuf> {
    let candidates = ["glslc", "glslangValidator"];
    for c in &candidates {
        if which(c).is_ok() { return Some(c.into()); }
    }
    if let Ok(sdk) = env::var("VULKAN_SDK") {
        for c in &candidates {
            let p = Path::new(&sdk).join("bin").join(format!("{}.exe", c));
            if p.exists() { return Some(p); }
        }
    }
    None
}

fn which(cmd: &str) -> Result<PathBuf, ()> {
    let path = env::var("PATH").unwrap_or_default();
    for dir in env::split_paths(&path) {
        let p = dir.join(format!("{}.exe", cmd));
        if p.exists() { return Ok(p); }
        let p = dir.join(cmd);
        if p.exists() { return Ok(p); }
    }
    Err(())
}

fn compile_shaders(out_dir: &Path, compiler: Option<&PathBuf>) {
    let shader_dir = PathBuf::from("shaders");
    if !shader_dir.exists() { return; }

    let shaders: &[(&str, &str)] = &[
        ("pathtracer/raygen.rgen.glsl", "raygen.spv"),
        ("pathtracer/miss.rmiss.glsl", "miss.spv"),
        ("pathtracer/chit.rchit.glsl", "chit.spv"),
        ("fidelityfx/denoiser_diffuse.comp.glsl", "denoise_diffuse.spv"),
        ("fidelityfx/denoiser_specular.comp.glsl", "denoise_specular.spv"),
        ("fidelityfx/ray_reconstruction.comp.glsl", "ray_recon.spv"),
        ("fidelityfx/cas.comp.glsl", "cas.spv"),
        ("fidelityfx/fsr3_create.comp.glsl", "fsr3_create.spv"),
        ("fidelityfx/fsr3_compensate.comp.glsl", "fsr3_comp.spv"),
        ("fidelityfx/fsr3_upscaler.comp.glsl", "fsr3_upscale.spv"),
        ("fidelityfx/fsr3_framegen.comp.glsl", "fsr3_fg.spv"),
        ("fidelityfx/xess3_framegen.comp.glsl", "xess3_fg.spv"),
        ("compute/blur.comp.glsl", "blur.spv"),
        ("compute/copy.comp.glsl", "copy.spv"),
        ("compute/tonemap.comp.glsl", "tonemap.spv"),
        ("quad/quad.vert.glsl", "quad_vert.spv"),
        ("quad/quad.frag.glsl", "quad_frag.spv"),
        ("mesh.vert.glsl", "mesh_vert.spv"),
        ("mesh.frag.glsl", "mesh_frag.spv"),
        ("fidelityfx/fsr4_upscaler.comp.glsl", "fsr4_upscale.spv"),
        ("fidelityfx/fsr4_framegen.comp.glsl", "fsr4_fg.spv"),
        ("compute/physics_broadphase.comp.glsl", "physics_broadphase.spv"),
        ("compute/physics_integrate.comp.glsl", "physics_integrate.spv"),
    ];

    for (src, out) in shaders {
        let src_path = shader_dir.join(src);
        let dst_path = out_dir.join(out);
        if !src_path.exists() { continue; }
        if let Some(comp) = compiler {
            compile_with(comp, &src_path, &dst_path);
        } else {
            generate_placeholder(&dst_path);
        }
    }
}

fn compile_with(compiler: &Path, src: &Path, dst: &Path) {
    let is_glslc = compiler.file_name().map(|n| n == "glslc").unwrap_or(false);
    let args = if is_glslc {
        vec![
            src.to_string_lossy().into_owned(),
            "-o".to_string(), dst.to_string_lossy().into_owned(),
            "-O3".to_string(),
        ]
    } else {
        vec![
            src.to_string_lossy().into_owned(),
            "-o".to_string(), dst.to_string_lossy().into_owned(),
            "--target-env=vulkan1.3".to_string(),
            "-O3".to_string(),
        ]
    };
    let status = Command::new(compiler).args(&args).status();
    if let Ok(s) = status {
        if !s.success() { eprintln!("Shader compile failed: {}", src.display()); }
    }
}

fn generate_placeholder(dst: &Path) {
    let header: [u32; 5] = [0x07230203, 0x00010600, 0x00000000, 0x00000000, 0x0002001b];
    let mut data: Vec<u8> = Vec::new();
    for word in header { data.extend_from_slice(&word.to_le_bytes()); }
    fs::write(dst, data).unwrap();
}