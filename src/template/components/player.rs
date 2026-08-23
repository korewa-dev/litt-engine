//! Player controller component
//!
//! WASD + mouse look FPS controller.

use litt_math::{Vec2, Vec3};

#[derive(Clone, Debug)]
pub struct Player {
    pub position: Vec3,
    pub rotation: Vec2, // yaw, pitch
    pub velocity: Vec3,
    pub speed: f32,
    pub look_speed: f32,
    pub is_ground: bool,
}

impl Player {
    pub fn new() -> Self {
        Self {
            position: Vec3::new(0.0, 1.0, 0.0),
            rotation: Vec2::new(0.0, 0.0),
            velocity: Vec3::ZERO,
            speed: 5.0,
            look_speed: 0.002,
            is_ground: false,
        }
    }

    pub fn update(&mut self, dt: f32, forward: bool, backward: bool, left: bool, right: bool) {
        let mut dir = Vec3::ZERO;
        if forward { dir.2 -= 1.0; }
        if backward { dir.2 += 1.0; }
        if left { dir.0 -= 1.0; }
        if right { dir.0 += 1.0; }
        dir = dir.normalized() * self.speed;
        self.position = self.position + dir * dt;
    }

    pub fn update_look(&mut self, dx: f32, dy: f32) {
        self.rotation.0 += dx * self.look_speed;
        self.rotation.1 += dy * self.look_speed;
        self.rotation.1 = self.rotation.1.clamp(-1.5, 1.5);
    }
}

impl Default for Player {
    fn default() -> Self { Self::new() }
}

