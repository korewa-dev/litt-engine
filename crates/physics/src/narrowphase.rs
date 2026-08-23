//! Narrowphase collision detection
//!
//! Implements:
//! - AABB vs AABB: SAT (Separating Axis Theorem)
//! - Sphere vs Sphere: distance check
//! - AABB vs Sphere: closest-point test
//! - Capsule vs Capsule: segment-segment distance

use litt_math::Vec3;

// =============================================================================
// Contact
// =============================================================================

/// Contact point between two colliding bodies
#[derive(Clone, Debug)]
pub struct Contact {
    /// Index of first body
    pub a: usize,
    /// Index of second body
    pub b: usize,
    /// Collision normal (points from body_a toward body_b)
    pub normal: Vec3,
    /// Penetration depth in meters
    pub penetration: f32,
    /// Contact point in world space
    pub point: Vec3,
}

// =============================================================================
// AABB vs AABB -- SAT
// =============================================================================

pub fn resolve_aabb_aabb(
    a_min: Vec3, a_max: Vec3,
    b_min: Vec3, b_max: Vec3,
    body_a_idx: usize,
    body_b_idx: usize,
) -> Option<Contact> {
    let overlap_x = (a_max.0 - b_min.0).min(b_max.0 - a_min.0);
    let overlap_y = (a_max.1 - b_min.1).min(b_max.1 - a_min.1);
    let overlap_z = (a_max.2 - b_min.2).min(b_max.2 - a_min.2);

    if overlap_x < 0.0 || overlap_y < 0.0 || overlap_z < 0.0 {
        return None;
    }

    let (normal, penetration) = if overlap_x <= overlap_y && overlap_x <= overlap_z {
        let dir = if a_min.0 < b_min.0 { -1.0 } else { 1.0 };
        (Vec3::new(dir, 0.0, 0.0), overlap_x)
    } else if overlap_y <= overlap_x && overlap_y <= overlap_z {
        let dir = if a_min.1 < b_min.1 { -1.0 } else { 1.0 };
        (Vec3::new(0.0, dir, 0.0), overlap_y)
    } else {
        let dir = if a_min.2 < b_min.2 { -1.0 } else { 1.0 };
        (Vec3::new(0.0, 0.0, dir), overlap_z)
    };

    let contact_min = Vec3::new(
        a_min.0.max(b_min.0), a_min.1.max(b_min.1), a_min.2.max(b_min.2),
    );
    let contact_max = Vec3::new(
        a_max.0.min(b_max.0), a_max.1.min(b_max.1), a_max.2.min(b_max.2),
    );
    let point = Vec3::new(
        (contact_min.0 + contact_max.0) * 0.5,
        (contact_min.1 + contact_max.1) * 0.5,
        (contact_min.2 + contact_max.2) * 0.5,
    );

    Some(Contact { a: body_a_idx, b: body_b_idx, normal, penetration, point })
}

// =============================================================================
// Sphere vs Sphere
// =============================================================================

pub fn resolve_sphere_sphere(
    center_a: Vec3, radius_a: f32,
    center_b: Vec3, radius_b: f32,
    body_a_idx: usize, body_b_idx: usize,
) -> Option<Contact> {
    let diff = center_b - center_a;
    let dist = diff.length();
    let combined_radius = radius_a + radius_b;

    if dist >= combined_radius || dist < 1e-8 {
        return None;
    }

    let normal = diff * (1.0 / dist);
    let penetration = combined_radius - dist;
    let point = center_a + normal * radius_a;

    Some(Contact { a: body_a_idx, b: body_b_idx, normal, penetration, point })
}

// =============================================================================
// AABB vs Sphere
// =============================================================================

pub fn resolve_aabb_sphere(
    aabb_min: Vec3, aabb_max: Vec3,
    sphere_center: Vec3, sphere_radius: f32,
    body_a_idx: usize, body_b_idx: usize,
) -> Option<Contact> {
    let closest = Vec3::new(
        sphere_center.0.max(aabb_min.0).min(aabb_max.0),
        sphere_center.1.max(aabb_min.1).min(aabb_max.1),
        sphere_center.2.max(aabb_min.2).min(aabb_max.2),
    );

    let diff = sphere_center - closest;
    let dist = diff.length();

    if dist >= sphere_radius || dist < 1e-8 {
        return None;
    }

    let normal = if dist > 1e-8 { diff * (1.0 / dist) } else { Vec3::new(1.0, 0.0, 0.0) };
    let penetration = sphere_radius - dist;

    Some(Contact { a: body_a_idx, b: body_b_idx, normal, penetration, point: closest })
}

// =============================================================================
// Capsule vs Capsule (axis-aligned, vertical capsules)
// =============================================================================

pub fn resolve_capsule_capsule(
    center_a: Vec3, radius_a: f32, half_height_a: f32,
    center_b: Vec3, radius_b: f32, half_height_b: f32,
    body_a_idx: usize, body_b_idx: usize,
) -> Option<Contact> {
    let a_bottom = Vec3::new(center_a.0, center_a.1 - half_height_a, center_a.2);
    let a_top = Vec3::new(center_a.0, center_a.1 + half_height_a, center_a.2);
    let b_bottom = Vec3::new(center_b.0, center_b.1 - half_height_b, center_b.2);
    let b_top = Vec3::new(center_b.0, center_b.1 + half_height_b, center_b.2);

    let (closest_a, closest_b) = segment_segment_closest(a_bottom, a_top, b_bottom, b_top);
    let diff = closest_b - closest_a;
    let dist = diff.length();
    let combined_radius = radius_a + radius_b;

    if dist >= combined_radius || dist < 1e-8 {
        return None;
    }

    let normal = diff * (1.0 / dist);
    let penetration = combined_radius - dist;

    Some(Contact { a: body_a_idx, b: body_b_idx, normal, penetration, point: (closest_a + closest_b) * 0.5 })
}

/// Closest points between two line segments
fn segment_segment_closest(p1: Vec3, p2: Vec3, p3: Vec3, p4: Vec3) -> (Vec3, Vec3) {
    let d1 = p2 - p1;
    let d2 = p4 - p3;
    let r = p1 - p3;
    let a = d1.dot(d1);
    let e = d2.dot(d2);
    let f = d2.dot(r);

    let mut s = 0.0;
    let mut t = 0.0;

    if a <= 1e-8 && e <= 1e-8 {
        return (p1, p3);
    }

    if a <= 1e-8 {
        s = 0.0;
        t = f.clamp(0.0, 1.0) / e;
    } else {
        let c = d1.dot(r);
        let b = d1.dot(d2);
        let denom = a * e - b * b;
        if denom != 0.0 {
            s = (b * f - c * e).clamp(0.0, 1.0) / denom;
        } else {
            s = 0.0;
        }
        t = (s * b + f) / e;
        if t < 0.0 {
            t = 0.0;
            s = (b * t - c).clamp(0.0, 1.0) / a;
        } else if t > 1.0 {
            t = 1.0;
            s = (b * t - c).clamp(0.0, 1.0) / a;
        }
    }

    (p1 + d1 * s, p3 + d2 * t)
}

// =============================================================================
// CollisionPair -- routes to correct narrowphase algorithm
// =============================================================================

/// Prepared collision pair with all data needed for narrowphase resolution
#[derive(Clone, Debug)]
pub struct CollisionPair {
    pub body_a_idx: usize,
    pub body_b_idx: usize,
    pub center_a: Vec3,
    pub center_b: Vec3,
    pub shape_type_a: u32,
    pub shape_type_b: u32,
    pub shape_data_a: [f32; 4],
    pub shape_data_b: [f32; 4],
}

impl CollisionPair {
    /// Resolve the collision -- returns Some(Contact) if colliding
    pub fn resolve(&self) -> Option<Contact> {
        match (self.shape_type_a, self.shape_type_b) {
            (0, 0) => {
                let a_half = Vec3::new(self.shape_data_a[0], self.shape_data_a[1], self.shape_data_a[2]);
                let b_half = Vec3::new(self.shape_data_b[0], self.shape_data_b[1], self.shape_data_b[2]);
                resolve_aabb_aabb(
                    self.center_a - a_half, self.center_a + a_half,
                    self.center_b - b_half, self.center_b + b_half,
                    self.body_a_idx, self.body_b_idx,
                )
            }
            (1, 1) => resolve_sphere_sphere(
                self.center_a, self.shape_data_a[0],
                self.center_b, self.shape_data_b[0],
                self.body_a_idx, self.body_b_idx,
            ),
            (0, 1) => {
                let a_half = Vec3::new(self.shape_data_a[0], self.shape_data_a[1], self.shape_data_a[2]);
                resolve_aabb_sphere(
                    self.center_a - a_half, self.center_a + a_half,
                    self.center_b, self.shape_data_b[0],
                    self.body_a_idx, self.body_b_idx,
                )
            }
            (1, 0) => {
                let b_half = Vec3::new(self.shape_data_b[0], self.shape_data_b[1], self.shape_data_b[2]);
                resolve_aabb_sphere(
                    self.center_b - b_half, self.center_b + b_half,
                    self.center_a, self.shape_data_a[0],
                    self.body_b_idx, self.body_a_idx,
                ).map(|mut c| { c.normal = -c.normal; c })
            }
            (2, 2) => resolve_capsule_capsule(
                self.center_a, self.shape_data_a[0], self.shape_data_a[1],
                self.center_b, self.shape_data_b[0], self.shape_data_b[1],
                self.body_a_idx, self.body_b_idx,
            ),
            _ => {
                // Mixed shapes: use bounding sphere approximation
                let r_a = self.bounding_radius(0);
                let r_b = self.bounding_radius(1);
                resolve_sphere_sphere(
                    self.center_a, r_a, self.center_b, r_b,
                    self.body_a_idx, self.body_b_idx,
                )
            }
        }
    }

    fn bounding_radius(&self, idx: usize) -> f32 {
        let data = if idx == 0 { self.shape_data_a } else { self.shape_data_b };
        let st = if idx == 0 { self.shape_type_a } else { self.shape_type_b };
        match st {
            0 => Vec3::new(data[0], data[1], data[2]).length(),
            1 => data[0],
            2 => (data[0] * data[0] + data[1] * data[1]).sqrt(),
            _ => 0.5,
        }
    }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use litt_math::Vec3;

    #[test]
    fn test_aabb_aabb_collision() {
        let contact = resolve_aabb_aabb(
            Vec3::new(-1.0, -1.0, -1.0), Vec3::new(1.0, 1.0, 1.0),
            Vec3::new(0.5, -1.0, -1.0), Vec3::new(2.5, 1.0, 1.0),
            0, 1,
        ).unwrap();
        assert!(contact.penetration > 0.0);
        assert!((contact.normal.0 - (-1.0)).abs() < 1e-6); // normal points left
        // Contact point = center of the overlap volume: x in [0.5, 1.0] => 0.75
        assert!((contact.point.0 - 0.75).abs() < 1e-6);
    }

    #[test]
    fn test_aabb_aabb_no_collision() {
        let result = resolve_aabb_aabb(
            Vec3::new(-2.0, -2.0, -2.0), Vec3::new(-1.0, 1.0, 1.0),
            Vec3::new(1.0, -1.0, -1.0), Vec3::new(2.0, 1.0, 1.0),
            0, 1,
        );
        assert!(result.is_none());
    }

    #[test]
    fn test_sphere_sphere_collision() {
        let contact = resolve_sphere_sphere(
            Vec3::new(0.0, 0.0, 0.0), 1.0,
            Vec3::new(1.5, 0.0, 0.0), 1.0,
            0, 1,
        ).unwrap();
        assert!(contact.penetration > 0.0);
        assert!((contact.normal.0 - 1.0).abs() < 1e-6);
    }

    #[test]
    fn test_sphere_sphere_no_collision() {
        let result = resolve_sphere_sphere(
            Vec3::new(0.0, 0.0, 0.0), 1.0,
            Vec3::new(5.0, 0.0, 0.0), 1.0,
            0, 1,
        );
        assert!(result.is_none());
    }

    #[test]
    fn test_aabb_sphere_collision() {
        let contact = resolve_aabb_sphere(
            Vec3::new(-1.0, -1.0, -1.0), Vec3::new(1.0, 1.0, 1.0),
            Vec3::new(1.5, 0.0, 0.0), 1.0,
            0, 1,
        ).unwrap();
        assert!(contact.penetration > 0.0);
    }

    #[test]
    fn test_collision_pair_aabb_aabb() {
        let pair = CollisionPair {
            body_a_idx: 0,
            body_b_idx: 1,
            center_a: Vec3::new(0.0, 0.0, 0.0),
            center_b: Vec3::new(1.0, 0.0, 0.0),
            shape_type_a: 0,
            shape_type_b: 0,
            shape_data_a: [0.5, 0.5, 0.5, 0.0],
            shape_data_b: [0.5, 0.5, 0.5, 0.0],
        };
        // AABBs: [-0.5,0.5] and [0.5,1.5] -- touching at x=0.5, no overlap
        let result = pair.resolve();
        // They touch but don't overlap -- this is a borderline case
        // With our SAT, overlap_x = (0.5 - 0.5).min(1.5 - (-0.5)) = 0.0
        // 0.0 is NOT < 0.0, so it returns Some with penetration=0
        // Actually let me check: overlap_x = min(0.5-0.5, 1.5-(-0.5)) = min(0.0, 2.0) = 0.0
        // Since 0.0 is NOT < 0.0, it proceeds and returns Some with penetration=0
        assert!(result.is_some());
    }

    #[test]
    fn test_collision_pair_sphere_sphere() {
        let pair = CollisionPair {
            body_a_idx: 0,
            body_b_idx: 1,
            center_a: Vec3::new(0.0, 0.0, 0.0),
            center_b: Vec3::new(1.0, 0.0, 0.0),
            shape_type_a: 1,
            shape_type_b: 1,
            shape_data_a: [0.6, 0.0, 0.0, 0.0], // radius 0.6
            shape_data_b: [0.6, 0.0, 0.0, 0.0], // radius 0.6
        };
        // Distance = 1.0, combined radius = 1.2, so they overlap
        let contact = pair.resolve().unwrap();
        assert!(contact.penetration > 0.0);
        assert!((contact.penetration - 0.2).abs() < 1e-6);
    }
}
