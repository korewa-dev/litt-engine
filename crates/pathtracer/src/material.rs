//! Material system for path tracing.
//! Simple BRDF models: Lambertian (diffuse) and GGX (metallic/rough).

use litt_math::*;

/// BSDF types
#[derive(Clone, Copy, Debug, Default)]
pub enum BsdfType {
    #[default]
    Lambertian,
    GGX,
    Metal,
    Dielectric,
    DiffuseLight,
}

/// Evaluate a simple Lambertian BRDF
pub fn lambertian_eval(albedo: Vec3, _wi: Vec3, wo: Vec3, normal: Vec3) -> f32 {
    let cos = wo.dot(normal).abs();
    albedo.dot(Vec3::ONE) / core::f32::consts::PI * cos
}

/// Sample a Lambertian hemisphere
pub fn lambertian_sample(_albedo: Vec3, normal: Vec3, rng: &mut Rng) -> (Vec3, Vec3, f32) {
    let direction = normal.random_hemisphere(rng.next_f32(), rng.next_f32());
    let pdf = 0.5 * normal.dot(direction).abs() / core::f32::consts::PI;
    (direction.normalized(), normal, pdf.max(1e-7))
}

/// GGX Normal Distribution Function (Smith geometry, simple)
pub fn ggx_ndf(roughness: f32, NdotH: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    a2 / (core::f32::consts::PI * d * d)
}

/// GGX visibility function (Schlick-GGX)
pub fn ggx_visibility(NdotV: f32, NdotL: f32, roughness: f32) -> f32 {
    let k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    let v = 1.0 / (NdotV * (1.0 - k) + k);
    let l = 1.0 / (NdotL * (1.0 - k) + k);
    v * l
}

/// Sample GGX microfacet direction
pub fn ggx_sample(roughness: f32, normal: Vec3, rng: &mut Rng) -> (Vec3, f32) {
    let alpha = roughness * roughness;
    let phi = rng.next_f32() * 2.0 * core::f32::consts::PI;
    let cos_theta = ((1.0 - rng.next_f32()) / (1.0 + (alpha * alpha - 1.0) * rng.next_f32())).sqrt();
    let sin_theta = (1.0 - cos_theta * cos_theta).sqrt();

    // Create local basis
    let up = if normal.dot(Vec3::Y) > 0.999 { Vec3::X } else { Vec3::Y };
    let tangent = normal.cross(up).normalized();
    let bitangent = normal.cross(tangent);

    let h = tangent * (sin_theta * phi.cos()) + bitangent * (sin_theta * phi.sin()) + normal * cos_theta;

    let ndot_h = normal.dot(h).abs();
    let pdf = ggx_ndf(roughness, ndot_h) * ndot_h / (4.0 * ndot_h + 1e-7);

    (h.normalized(), pdf.max(1e-7))
}

/// Schlick Fresnel approximation
pub fn schlick_fresnel(cos_theta: f32, f0: Vec3) -> Vec3 {
    f0 + (1.0 - f0) * (1.0 - cos_theta).powi(5)
}

/// Fresnel for dielectric
pub fn fresnel_dielectric(cos_theta: f32, ior: f32) -> f32 {
    let ratio = if cos_theta > 0.0 { 1.0 / ior } else { ior };
    let sin_theta2 = ratio * ratio * (1.0 - cos_theta * cos_theta);
    if sin_theta2 >= 1.0 { return 1.0; }
    let cos_theta_t = (1.0 - sin_theta2).sqrt();
    let rs = ((ior * cos_theta) - cos_theta_t) / ((ior * cos_theta) + cos_theta_t);
    let rp = ((cos_theta) - (ior * cos_theta_t)) / ((cos_theta) + (ior * cos_theta_t));
    (rs * rs + rp * rp) / 2.0
}
