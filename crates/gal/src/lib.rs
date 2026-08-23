//! Litt GAL -- Graphics Abstraction (Translation) Layer.
//!
//! Write your renderer ONCE against the logical API below and it runs
//! identically on every registered backend: Vulkan 1.3, DirectX 12, or
//! AMD AGS (which rides Vulkan with AGS driver extensions).
//!
//! # How the translation works
//!
//! 1. Resources are created through [`GraphicsDevice`] as *logical*
//!    descriptors (`BufferDesc`, `ImageDesc`, ...). Each backend materializes
//!    them into its own native objects but reports only an opaque
//!    [`BufferId`]/[`ImageId`]/[`PipelineId`] back to you.
//! 2. Per-frame work is recorded as a backend-neutral [`CommandList`] --
//!    bind, push constants, dispatch, draw, barrier. A [`CommandList`] is
//!    plain data; it has no Vulkan or D3D types in it.
//! 3. A [`BackendRouter`] owns every live device. `translate()` replays one
//!    recorded list on ALL devices (primary first). Developing against
//!    Vulkan therefore costs nothing extra when the router also carries a
//!    DX12 or AGS device -- they see the same commands.
//! 4. `set_primary()` migrates the logical resource table to another backend
//!    by replaying creation descriptors there. Hot-swap without touching
//!    game code.
//!
//! ```ignore
//! use litt_gal::*;
//!
//! let mut router = BackendRouter::new();
//! router.register(NullDevice::new());                    // headless / tests
//! // router.register(VulkanDevice::attach(&vk_device));  // feature = "vulkan"
//! // router.register(AgsDevice::attach(&vk_dev, ags));  // feature = "ags"
//!
//! let buf = router.primary().create_buffer(BufferDesc::storage(1024))?;
//! let mut cl = CommandList::new("frame");
//! cl.bind_pipeline(compute_pipe);
//! cl.dispatch(64, 1, 1);
//! router.translate(&cl)?;
//! ```

pub mod backend;
pub mod caps;
pub mod desc;
pub mod device;
pub mod error;
pub mod id;
pub mod null;
pub mod router;

pub use backend::BackendKind;
pub use caps::Capabilities;
pub use desc::{BufferDesc, BufferUsage, ImageDesc, ImageUsage, MemoryLocation, PipelineDesc, ShaderStage, SwapchainDesc};
pub use device::{Command, CommandList, GraphicsDevice};
pub use error::GalError;
pub use id::{BufferId, ImageId, PipelineId, SwapchainId};
pub use null::NullDevice;
pub use router::BackendRouter;

/// GAL version of the engine.
pub const GAL_VERSION: &str = "0.1.0";

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ids_are_generation_safe() {
        let mut dev = NullDevice::new();
        let b1 = dev.create_buffer(BufferDesc::storage(16)).unwrap();
        dev.destroy_buffer(b1).unwrap();
        // Same slot reused, generation bumped -> old id must be rejected.
        let b2 = dev.create_buffer(BufferDesc::storage(16)).unwrap();
        assert_ne!(b1.generation(), b2.generation());
        assert!(dev.destroy_buffer(b1).is_err());
        assert!(dev.destroy_buffer(b2).is_ok());
    }

    #[test]
    fn commands_translate_to_all_backends() {
        let mut router = BackendRouter::new();
        let primary_id = router.register(Box::new(NullDevice::with_name("null-a")));
        router.register(Box::new(NullDevice::with_name("null-b")));

        let buf = router.primary_mut().create_buffer(BufferDesc::uniform(256)).unwrap();

        let mut cl = CommandList::new("frame");
        cl.push_constants(&[1u8, 2, 3, 4]);
        cl.bind_buffer(buf, 0);
        cl.dispatch(8, 1, 1);

        router.translate(&cl).unwrap();

        for d in router.devices() {
            assert_eq!(d.stats().dispatches, 1, "backend {} missed dispatch", d.name());
            assert_eq!(d.stats().push_constant_writes, 1);
        }
        assert_eq!(primary_id, 0);
    }

    #[test]
    fn primary_migration_recreates_resources() {
        let mut router = BackendRouter::new();
        router.register(Box::new(NullDevice::with_name("a")));
        router.register(Box::new(NullDevice::with_name("b")));

        // Create THROUGH the router so the descriptor is tracked for migration.
        let img = router.create_image(ImageDesc {
            width: 4,
            height: 4,
            depth: 1,
            format: desc::Format::Rgba8Unorm,
            usage: ImageUsage::SAMPLED,
            location: MemoryLocation::DeviceLocal,
        }).unwrap();

        // Migrate: device slot 1 must now host a materialized copy.
        router.set_primary(1).unwrap();
        assert_eq!(router.primary_index(), 1);
        assert!(router.is_migrated_on(1, img));
        let b = router.device(1).unwrap();
        assert_eq!(b.stats().images_created, 1);
    }
}
