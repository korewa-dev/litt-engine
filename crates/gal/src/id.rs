//! Generational resource handles.
//!
//! A handle is a packed u64: low 32 bits = slot index, high 32 bits =
//! generation counter. Devices bump a slot's generation on free, so a stale
//! handle copied elsewhere is rejected instead of aliasing new memory.

use std::fmt;

macro_rules! gal_id {
    ($(#[$meta:meta])* $name:ident, $tag:expr) => {
        $(#[$meta])*
        #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
        pub struct $name(u64);

        impl $name {
            /// Pack index + generation into one handle.
            #[inline]
            pub const fn pack(index: u32, generation: u32) -> Self {
                Self(((generation as u64) << 32) | index as u64)
            }

            /// Slot index this handle points at.
            #[inline]
            pub const fn index(self) -> u32 {
                self.0 as u32
            }

            /// Generation guard against use-after-free.
            #[inline]
            pub const fn generation(self) -> u32 {
                (self.0 >> 32) as u32
            }

            /// Raw bits (for push constants / GPU keys).
            #[inline]
            pub const fn bits(self) -> u64 {
                self.0
            }
        }

        impl fmt::Display for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(f, "{}({}:{})", $tag, self.index(), self.generation())
            }
        }
    };
}

gal_id!(
    /// Handle to a device buffer.
    BufferId,
    "buffer"
);
gal_id!(
    /// Handle to an image / texture.
    ImageId,
    "image"
);
gal_id!(
    /// Handle to a compiled pipeline (graphics or compute).
    PipelineId,
    "pipeline"
);
gal_id!(
    /// Handle to a swapchain / presentation target.
    SwapchainId,
    "swapchain"
);
