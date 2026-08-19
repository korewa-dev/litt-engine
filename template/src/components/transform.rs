//! Transform component - position, rotation, scale
use litt_math::*;

#[derive(Clone, Debug, Default)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
}

impl Transform {
    pub fn new() -> Self { Self::default() }
    pub fn matrix(&self) -> Mat4 {
        Mat4::translate(self.position.0, self.position.1, self.position.2)
            * self.rotation.to_mat4()
            * Mat4::scale(self.scale.0, self.scale.1, self.scale.2)
    }
    pub fn inverse_matrix(&self) -> Mat4 { self.matrix().inverse() }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct Quat { pub x: f32, pub y: f32, pub z: f32, pub w: f32 }

impl Quat {
    pub fn from_axis_angle(axis: Vec3, angle: f32) -> Self {
        let (s, c) = (angle * 0.5).sin_cos();
        let v = axis.normalized() * s;
        Self { x: v.0, y: v.1, z: v.2, w: c }
    }
    pub fn to_mat4(&self) -> Mat4 {
        let x=self.x; let y=self.y; let z=self.z; let w=self.w;
        Mat4([
            1.0-2.0*(y*y+z*z), 2.0*(x*y+z*w), 2.0*(x*z-y*w), 0.0,
            2.0*(x*y-z*w), 1.0-2.0*(x*x+z*z), 2.0*(y*z+x*w), 0.0,
            2.0*(x*z+y*w), 2.0*(y*z-x*w), 1.0-2.0*(x*x+y*y), 0.0,
            0.0, 0.0, 0.0, 1.0,
        ])
    }
}
