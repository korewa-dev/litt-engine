//! Path tracing backend.
//! Computes rays, handles acceleration structures, and outputs to accumulation buffer.

pub mod scene;
pub mod tracer;
pub mod material;
pub mod rng;

pub use scene::*;
pub use tracer::*;
pub use material::*;
pub use rng::*;
