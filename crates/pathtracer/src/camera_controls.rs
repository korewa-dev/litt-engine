//! Camera control system -- WASD movement + mouse-look for the path tracer.
//!
//! Uses locked-cursor mode for FPS-style camera rotation.
//! Keyboard movement uses world-space axes derived from camera yaw.

use litt_math::*;

/// Controls for the camera
#[derive(Debug)]
pub struct CameraControls {
    /// Is mouse-look active (cursor locked to window)
    pub active: bool,
    /// Current yaw (radians, around Y axis)
    pub yaw: f32,
    /// Current pitch (radians, up/down)
    pub pitch: f32,
    /// Camera position
    pub position: Vec3,
    /// Movement speed (units per second)
    pub move_speed: f32,
    /// Mouse sensitivity (radians per pixel)
    pub mouse_sensitivity: f32,
}

impl Default for CameraControls {
    fn default() -> Self {
        Self {
            active: true,
            yaw: 0.0,
            pitch: 0.0,
            position: Vec3::new(0.0, 2.0, 8.0),
            move_speed: 5.0,
            mouse_sensitivity: 0.002,
        }
    }
}

impl CameraControls {
    pub fn new() -> Self { Self::default() }

    /// Process mouse delta (from locked cursor)
    pub fn process_mouse(&mut self, dx: f32, dy: f32) {
        self.yaw   -= dx * self.mouse_sensitivity;
        self.pitch += dy * self.mouse_sensitivity;
        // Clamp pitch to avoid flipping
        self.pitch = self.pitch.max(-1.55).min(1.55);
    }

    /// Process keyboard input for movement
    pub fn process_keyboard(
        &mut self,
        keys: &litt_input::KeyboardState,
        dt: f32,
    ) {
        let speed = self.move_speed * dt;
        // Forward vector from yaw
        let fwd = Vec3::new(
            self.yaw.sin() * self.pitch.cos(),
            self.pitch.sin(),
            self.yaw.cos() * self.pitch.cos(),
        );

        // Right vector (perpendicular to forward, in XZ plane)
        let right = Vec3::new(fwd.z, 0.0, -fwd.x).normalized();

        if keys.is_down(litt_input::Key::W) {
            self.position = self.position + fwd * speed;
        }
        if keys.is_down(litt_input::Key::S) {
            self.position = self.position - fwd * speed;
        }
        if keys.is_down(litt_input::Key::A) {
            self.position = self.position - right * speed;
        }
        if keys.is_down(litt_input::Key::D) {
            self.position = self.position + right * speed;
        }
        if keys.is_down(litt_input::Key::Space) {
            self.position.y += speed;
        }
        if keys.is_down(litt_input::Key::Shift) {
            self.position.y -= speed;
        }
    }

    /// Convert to a path tracer Camera
    pub fn to_camera(&self, fov: f32, aspect: f32) -> litt_pathtracer::Camera {
        litt_pathtracer::Camera {
            position: Vec3::new(self.position.0, self.position.1, self.position.2),
            rotation: Vec2::new(self.yaw, self.pitch),
            fov,
            aspect,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn camera_controls_basic() {
        let mut cc = CameraControls::new();
        assert_eq!(cc.position, Vec3::new(0.0, 2.0, 8.0));

        cc.process_mouse(100.0, -50.0);
        assert!((cc.yaw + 100.0 * 0.002).abs() < 1e-6);
        assert!((cc.pitch - 50.0 * 0.002).abs() < 1e-6);
    }
}
