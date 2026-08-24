//! Scene asset -- collection of models, textures, and materials.
//! Represents a complete scene or level.

use super::handle::AssetHandle;

/// A scene asset
#[derive(Debug)]
pub struct Scene {
    pub name: String,
    pub handle: AssetHandle,
    pub models: Vec<(AssetHandle, Transform)>,
    pub lights: Vec<Light>,
    pub camera: Option<Camera>,
}

impl Scene {
    /// Create an empty scene
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            handle: AssetHandle::from_path(name, super::handle::AssetType::Scene),
            models: Vec::new(),
            lights: Vec::new(),
            camera: None,
        }
    }

    /// Add a model to the scene
    pub fn add_model(&mut self, handle: AssetHandle, transform: Transform) {
        self.models.push((handle, transform));
    }

    /// Add a light to the scene
    pub fn add_light(&mut self, light: Light) {
        self.lights.push(light);
    }

    /// Set the scene camera
    pub fn set_camera(&mut self, camera: Camera) {
        self.camera = Some(camera);
    }
}

/// Transform for scene objects
#[derive(Clone, Debug)]
pub struct Transform {
    pub position: (f32, f32, f32),
    pub rotation: (f32, f32, f32, f32), // quaternion
    pub scale: (f32, f32, f32),
}

impl Default for Transform {
    fn default() -> Self {
        Self {
            position: (0.0, 0.0, 0.0),
            rotation: (0.0, 0.0, 0.0, 1.0),
            scale: (1.0, 1.0, 1.0),
        }
    }
}

/// Light in a scene
#[derive(Clone, Debug)]
pub struct Light {
    pub position: (f32, f32, f32),
    pub color: (f32, f32, f32),
    pub intensity: f32,
    pub range: f32,
    pub type_: LightType,
}

/// Light type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LightType {
    Directional,
    Point,
    Spot,
}

/// Camera for a scene
#[derive(Clone, Debug)]
pub struct Camera {
    pub position: (f32, f32, f32),
    pub target: (f32, f32, f32),
    pub up: (f32, f32, f32),
    pub fov: f32,
    pub near: f32,
    pub far: f32,
    pub aspect: f32,
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            position: (0.0, 0.0, -10.0),
            target: (0.0, 0.0, 0.0),
            up: (0.0, 1.0, 0.0),
            fov: 60.0,
            near: 0.1,
            far: 1000.0,
            aspect: 16.0 / 9.0,
        }
    }
}
