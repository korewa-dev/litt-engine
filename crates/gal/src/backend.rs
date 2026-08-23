//! Backend identification.

use crate::error::GalError;

/// A registered graphics backend.
///
/// `Ags` is not a standalone API: AMD AGS rides on Vulkan and layers AGS
/// driver extensions (power profiles, telemetry, crossfire hints) on top.
/// The GAL treats it as its own backend so game code never branches.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum BackendKind {
    /// Headless reference backend. Records everything, renders nothing.
    Null,
    /// Vulkan 1.3 (ash + VMA).
    Vulkan,
    /// DirectX 12 (winapi, DXR).
    Dx12,
    /// AMD AGS extensions over Vulkan (RX 6000/7000/9000 tuned paths).
    Ags,
}

impl BackendKind {
    /// All backend kinds known to this build.
    pub const ALL: [BackendKind; 4] = [
        BackendKind::Null,
        BackendKind::Vulkan,
        BackendKind::Dx12,
        BackendKind::Ags,
    ];

    /// Stable machine name ("vulkan", "dx12", "ags", "null").
    pub const fn name(self) -> &'static str {
        match self {
            BackendKind::Null => "null",
            BackendKind::Vulkan => "vulkan",
            BackendKind::Dx12 => "dx12",
            BackendKind::Ags => "ags",
        }
    }

    /// Parse a name produced by [`BackendKind::name`].
    pub fn parse(s: &str) -> Result<Self, GalError> {
        match s.to_ascii_lowercase().as_str() {
            "null" | "headless" => Ok(BackendKind::Null),
            "vulkan" | "vk" => Ok(BackendKind::Vulkan),
            "dx12" | "d3d12" | "directx12" => Ok(BackendKind::Dx12),
            "ags" | "amd-ags" => Ok(BackendKind::Ags),
            other => Err(GalError::UnknownBackend(other.to_string())),
        }
    }

    /// True when the backend can present to a real window surface.
    pub const fn can_present(self) -> bool {
        !matches!(self, BackendKind::Null)
    }
}

impl std::fmt::Display for BackendKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.name())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_roundtrip() {
        for b in BackendKind::ALL {
            assert_eq!(BackendKind::parse(b.name()).unwrap(), b);
        }
        assert!(BackendKind::parse("wgpu").is_err());
    }
}
