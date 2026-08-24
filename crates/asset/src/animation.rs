//! Animation playback and blending system.
//!
//! Turns the keyframe data in `asset::model::Animation` into a runtime
//! playback system: sampling, looping, speed control, and cross-fading.
//!
//! ```ignore
//! use litt_asset::animation::*;
//! let mut player = AnimationPlayer::new();
//! player.play("Walk", PlayMode::Loop, 1.0);
//! player.update(dt);
//! let pose = player.sample_pose("Hips");
//! ```

use litt_math::Vec3;
use crate::model::{Animation, Keyframe};

// =============================================================================
// Quaternion helpers ([x, y, z, w])
// =============================================================================

/// Normalized linear interpolation with shortest-path correction.
pub fn quat_nlerp(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    let mut b = b;
    let dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if dot < 0.0 {
        b = [-b[0], -b[1], -b[2], -b[3]];
    }
    let mut q = [
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
        a[3] + (b[3] - a[3]) * t,
    ];
    let len = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]).sqrt();
    if len > 1e-8 {
        q = [q[0] / len, q[1] / len, q[2] / len, q[3] / len];
    }
    q
}

/// Spherical linear interpolation between quaternions.
pub fn quat_slerp(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    let mut b = b;
    let mut cos_theta = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if cos_theta < 0.0 {
        b = [-b[0], -b[1], -b[2], -b[3]];
        cos_theta = -cos_theta;
    }
    if cos_theta > 0.9995 {
        return quat_nlerp(a, b, t);
    }
    let theta = cos_theta.clamp(-1.0, 1.0).acos();
    let sin_theta = theta.sin();
    let wa = ((1.0 - t) * theta).sin() / sin_theta;
    let wb = (t * theta).sin() / sin_theta;
    [
        wa * a[0] + wb * b[0],
        wa * a[1] + wb * b[1],
        wa * a[2] + wb * b[2],
        wa * a[3] + wb * b[3],
    ]
}

// =============================================================================
// Sampled pose
// =============================================================================

/// A sampled transform for one animation target.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Pose {
    pub position: Vec3,
    pub rotation: [f32; 4],
    pub scale: Vec3,
}

impl Default for Pose {
    fn default() -> Self {
        Self {
            position: Vec3::ZERO,
            rotation: [0.0, 0.0, 0.0, 1.0],
            scale: Vec3::new(1.0, 1.0, 1.0),
        }
    }
}

impl Pose {
    /// Linear blend between two poses (rotation via slerp).
    pub fn blend(a: &Pose, b: &Pose, factor: f32) -> Pose {
        let f = factor.clamp(0.0, 1.0);
        Pose {
            position: Vec3::new(
                a.position.0 + (b.position.0 - a.position.0) * f,
                a.position.1 + (b.position.1 - a.position.1) * f,
                a.position.2 + (b.position.2 - a.position.2) * f,
            ),
            rotation: quat_slerp(a.rotation, b.rotation, f),
            scale: Vec3::new(
                a.scale.0 + (b.scale.0 - a.scale.0) * f,
                a.scale.1 + (b.scale.1 - a.scale.1) * f,
                a.scale.2 + (b.scale.2 - a.scale.2) * f,
            ),
        }
    }
}

fn key_to_pose(k: &Keyframe) -> Pose {
    Pose { position: k.position, rotation: k.rotation, scale: k.scale }
}

/// Sample one channel at local time `t` (clamped, linear/slerp between keys).
pub fn sample_channel(channel: &crate::model::AnimationChannel, t: f32) -> Pose {
    let keys = &channel.keyframes;
    if keys.is_empty() {
        return Pose::default();
    }
    if t <= keys[0].time {
        return key_to_pose(&keys[0]);
    }
    let last = keys.len() - 1;
    if t >= keys[last].time {
        return key_to_pose(&keys[last]);
    }
    let mut i = 0;
    while i + 1 < keys.len() && keys[i + 1].time <= t {
        i += 1;
    }
    let k0 = &keys[i];
    let k1 = &keys[(i + 1).min(last)];
    let span = (k1.time - k0.time).max(1e-6);
    let f = ((t - k0.time) / span).clamp(0.0, 1.0);

    Pose {
        position: Vec3::new(
            k0.position.0 + (k1.position.0 - k0.position.0) * f,
            k0.position.1 + (k1.position.1 - k0.position.1) * f,
            k0.position.2 + (k1.position.2 - k0.position.2) * f,
        ),
        rotation: quat_slerp(k0.rotation, k1.rotation, f),
        scale: Vec3::new(
            k0.scale.0 + (k1.scale.0 - k0.scale.0) * f,
            k0.scale.1 + (k1.scale.1 - k0.scale.1) * f,
            k0.scale.2 + (k1.scale.2 - k0.scale.2) * f,
        ),
    }
}

// =============================================================================
// Playback
// =============================================================================

/// Playback mode for an animation clip.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PlayMode {
    Once,
    Loop,
    PingPong,
}

/// Playback state of a clip.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PlayState {
    Stopped,
    Playing,
    Paused,
    Finished,
}

/// A playing animation instance.
#[derive(Clone, Debug)]
pub struct PlayingClip {
    pub name: String,
    pub time: f32,
    pub speed: f32,
    pub weight: f32,
    pub mode: PlayMode,
    pub state: PlayState,
    direction: f32,
}

/// Runtime animation player over a clip library.
pub struct AnimationPlayer {
    clips: Vec<Animation>,
    active: Vec<PlayingClip>,
    fade_remaining: f32,
    fade_total: f32,
    fade_target: Option<(String, PlayMode, f32)>,
}

impl Default for AnimationPlayer {
    fn default() -> Self { Self::new() }
}

impl AnimationPlayer {
    pub fn new() -> Self {
        Self {
            clips: Vec::new(),
            active: Vec::new(),
            fade_remaining: 0.0,
            fade_total: 0.0,
            fade_target: None,
        }
    }

    /// Register a clip library (e.g. from a loaded Model).
    pub fn set_clips(&mut self, clips: Vec<Animation>) {
        self.clips = clips;
        self.active.clear();
        self.fade_target = None;
    }

    pub fn clip_names(&self) -> Vec<String> {
        self.clips.iter().map(|c| c.name.clone()).collect()
    }

    /// Start playing a clip by name, replacing current playback.
    pub fn play(&mut self, name: &str, mode: PlayMode, speed: f32) -> bool {
        if !self.clips.iter().any(|c| c.name == name) {
            return false;
        }
        self.fade_target = None;
        self.active.clear();
        self.active.push(PlayingClip {
            name: name.to_string(),
            time: 0.0,
            speed,
            weight: 1.0,
            mode,
            state: PlayState::Playing,
            direction: 1.0,
        });
        true
    }

    /// Cross-fade to a clip over `fade_sec` seconds. Existing clips ramp out.
    pub fn crossfade(&mut self, name: &str, mode: PlayMode, speed: f32, fade_sec: f32) -> bool {
        if !self.clips.iter().any(|c| c.name == name) {
            return false;
        }
        if fade_sec <= 0.0 || self.active.is_empty() {
            return self.play(name, mode, speed);
        }
        // Add target at weight 0; ramps up while others ramp down in update()
        self.active.push(PlayingClip {
            name: name.to_string(),
            time: 0.0,
            speed,
            weight: 0.0,
            mode,
            state: PlayState::Playing,
            direction: 1.0,
        });
        self.fade_remaining = fade_sec;
        self.fade_total = fade_sec;
        self.fade_target = Some((name.to_string(), mode, speed));
        true
    }

    /// Advance playback by `dt` seconds.
    pub fn update(&mut self, dt: f32) {
        // Cross-fade weights
        if self.fade_target.is_some() && self.fade_total > 0.0 {
            self.fade_remaining -= dt;
            let progress = (1.0 - (self.fade_remaining / self.fade_total).max(0.0)).clamp(0.0, 1.0);
            let target_name = self.fade_target.as_ref().map(|(n, _, _)| n.clone());
            for clip in &mut self.active {
                let is_target = target_name.as_deref() == Some(clip.name.as_str());
                clip.weight = if is_target { progress } else { 1.0 - progress };
            }
            if self.fade_remaining <= 0.0 {
                self.active.retain(|c| target_name.as_deref() == Some(c.name.as_str()));
                if let Some(w) = self.active.first_mut() {
                    w.weight = 1.0;
                }
                self.fade_target = None;
            }
        }

        for clip in &mut self.active {
            if clip.state != PlayState::Playing {
                continue;
            }
            let duration = self
                .clips
                .iter()
                .find(|c| c.name == clip.name)
                .map(|c| c.duration.max(1e-6))
                .unwrap_or(1e-6);

            clip.time += clip.speed * clip.direction * dt;

            match clip.mode {
                PlayMode::Once => {
                    if clip.time >= duration {
                        clip.time = duration;
                        clip.state = PlayState::Finished;
                    }
                }
                PlayMode::Loop => {
                    if clip.time >= duration {
                        clip.time %= duration;
                    }
                }
                PlayMode::PingPong => {
                    if clip.time >= duration {
                        clip.time = (2.0 * duration - clip.time).min(duration);
                        clip.direction = -1.0;
                    } else if clip.time <= 0.0 && clip.direction < 0.0 {
                        clip.time = (-clip.time).min(duration);
                        clip.direction = 1.0;
                    }
                }
            }
        }

        // Finished Once-mode clips stay active so they keep holding their
        // final pose; they are replaced by play()/crossfade() or stop().
    }

    /// Sample the blended pose for a named target (node/bone). None if not animated.
    pub fn sample_pose(&self, target: &str) -> Option<Pose> {
        let mut result: Option<Pose> = None;
        let mut weight_sum = 0.0;

        for playing in &self.active {
            if playing.weight <= 1e-5 {
                continue;
            }
            // A clip that does not animate this target contributes nothing
            // instead of nuking the whole blended pose mid-crossfade.
            let Some(clip) = self.clips.iter().find(|c| c.name == playing.name) else {
                continue;
            };
            let Some(channel) = clip.channels.iter().find(|ch| ch.target_name == target) else {
                continue;
            };
            let pose = sample_channel(channel, playing.time);

            result = Some(match result {
                None => pose,
                Some(prev) => {
                    let total = weight_sum + playing.weight;
                    Pose::blend(&prev, &pose, playing.weight / total.max(1e-6))
                }
            });
            weight_sum += playing.weight;
        }
        result
    }

    pub fn active_clips(&self) -> &[PlayingClip] {
        &self.active
    }

    pub fn state_of(&self, name: &str) -> Option<PlayState> {
        self.active.iter().find(|c| c.name == name).map(|c| c.state)
    }

    pub fn pause(&mut self, name: &str) {
        if let Some(c) = self.active.iter_mut().find(|c| c.name == name) {
            c.state = PlayState::Paused;
        }
    }

    pub fn resume(&mut self, name: &str) {
        if let Some(c) = self.active.iter_mut().find(|c| c.name == name) {
            if c.state == PlayState::Paused {
                c.state = PlayState::Playing;
            }
        }
    }

    pub fn stop(&mut self) {
        self.active.clear();
        self.fade_target = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::{Animation, AnimationChannel, Keyframe};

    fn walk_clip() -> Animation {
        Animation {
            name: "Walk".into(),
            duration: 1.0,
            channels: vec![AnimationChannel {
                target_name: "Hips".into(),
                keyframes: vec![
                    Keyframe { time: 0.0, position: Vec3::new(0.0, 0.0, 0.0), rotation: [0.0, 0.0, 0.0, 1.0], scale: Vec3::ONE },
                    Keyframe { time: 1.0, position: Vec3::new(0.0, 1.0, 0.0), rotation: [0.0, std::f32::consts::FRAC_1_SQRT_2, 0.0, std::f32::consts::FRAC_1_SQRT_2], scale: Vec3::ONE },
                ],
            }],
        }
    }

    #[test]
    fn samples_mid_keyframe() {
        let mut p = AnimationPlayer::new();
        p.set_clips(vec![walk_clip()]);
        assert!(p.play("Walk", PlayMode::Loop, 1.0));
        p.update(0.5);
        let pose = p.sample_pose("Hips").unwrap();
        assert!((pose.position.1 - 0.5).abs() < 1e-4);
    }

    #[test]
    fn loops() {
        let mut p = AnimationPlayer::new();
        p.set_clips(vec![walk_clip()]);
        p.play("Walk", PlayMode::Loop, 1.0);
        p.update(1.25);
        let pose = p.sample_pose("Hips").unwrap();
        assert!((pose.position.1 - 0.25).abs() < 1e-4);
    }

    #[test]
    fn once_finishes_and_holds() {
        let mut p = AnimationPlayer::new();
        p.set_clips(vec![walk_clip()]);
        p.play("Walk", PlayMode::Once, 1.0);
        p.update(2.0);
        assert_eq!(p.state_of("Walk"), Some(PlayState::Finished));
        let pose = p.sample_pose("Hips").unwrap();
        assert!((pose.position.1 - 1.0).abs() < 1e-4);
    }

    #[test]
    fn crossfade_blends_weights() {
        let mut idle = walk_clip();
        idle.name = "Idle".into();
        idle.channels[0].keyframes[1].position = Vec3::new(0.0, 10.0, 0.0);

        let mut p = AnimationPlayer::new();
        p.set_clips(vec![walk_clip(), idle]);
        p.play("Walk", PlayMode::Loop, 1.0);
        assert!(p.crossfade("Idle", PlayMode::Loop, 1.0, 1.0));

        // Halfway through fade both contribute
        p.update(0.5);
        let names: Vec<_> = p.active_clips().iter().map(|c| c.name.clone()).collect();
        assert_eq!(names.len(), 2);

        // After fade completes only target remains
        p.update(0.6);
        let names: Vec<_> = p.active_clips().iter().map(|c| c.name.clone()).collect();
        assert_eq!(names, vec!["Idle".to_string()]);
    }

    #[test]
    fn slerp_identity_halfway_is_normalized() {
        let a = [0.0, 0.0, 0.0, 1.0];
        let b = [0.0, 1.0, 0.0, 0.0];
        let q = quat_slerp(a, b, 0.5);
        let len = (q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]).sqrt();
        assert!((len - 1.0).abs() < 1e-5);
    }
}
