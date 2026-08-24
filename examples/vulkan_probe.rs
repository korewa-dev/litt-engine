//! Minimal Vulkan loader diagnostics: entry -> instance layers/exts ->
//! instance creation with surface extensions. Run:
//!   cargo run -p litt --example vulkan_probe
use ash::vk;

fn main() {
    unsafe {
        let entry = match ash::Entry::load() {
            Ok(e) => e,
            Err(e) => {
                println!("loader load failed: {e:?}");
                return;
            }
        };
        println!("[probe] loader loaded");

        match entry.enumerate_instance_layer_properties() {
            Ok(layers) => {
                println!("[probe] {} layers", layers.len());
                for l in layers.iter().take(8) {
                    let name = std::ffi::CStr::from_ptr(l.layer_name.as_ptr());
                    println!("  layer: {}", name.to_string_lossy());
                }
            }
            Err(e) => println!("enumerate layers failed: {e:?}"),
        }

        match entry.enumerate_instance_extension_properties(None) {
            Ok(exts) => {
                let names: Vec<String> = exts
                    .iter()
                    .map(|e| {
                        std::ffi::CStr::from_ptr(e.extension_name.as_ptr())
                            .to_string_lossy()
                            .into_owned()
                    })
                    .collect();
                println!("[probe] {} instance extensions", names.len());
                for n in &names {
                    println!("  ext: {n}");
                }
                let has_surface = names.iter().any(|n| n == "VK_KHR_surface");
                let has_win32 = names.iter().any(|n| n == "VK_KHR_win32_surface");
                if !has_surface || !has_win32 {
                    println!("[probe] MISSING surface extensions!");
                    return;
                }
            }
            Err(e) => {
                println!("enumerate instance ext failed: {e:?}");
                return;
            }
        }

        let try_create = |label: &str, names: &[&[u8]]| {
            let exts: Vec<*const i8> = names.iter().map(|n| n.as_ptr().cast()).collect();
            let info = vk::InstanceCreateInfo {
                flags: vk::InstanceCreateFlags::empty(),
                enabled_extension_count: exts.len() as u32,
                pp_enabled_extension_names: exts.as_ptr(),
                ..Default::default()
            };
            match entry.create_instance(&info, None) {
                Ok(instance) => {
                    println!("[probe] {label}: OK");
                    let _ = instance.destroy_instance(None);
                    true
                }
                Err(e) => {
                    println!("[probe] {label}: FAILED {e:?}");
                    false
                }
            }
        };

        try_create("no-extensions", &[]);
        try_create("surface-only", &[b"VK_KHR_surface\0"]);
        try_create(
            "win32-surface-only",
            &[b"VK_KHR_win32_surface\0"],
        );
        try_create(
            "both",
            &[b"VK_KHR_surface\0", b"VK_KHR_win32_surface\0"],
        );
    }
}
