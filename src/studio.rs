//! Studio mode -- Unity-style chat + live viewport inside the real window.
//!
//! Left panel: a chat log + input line, rasterized with a built-in 3x5
//! bitmap font into plain colored quads (no textures, one shared pipeline).
//! Right side: the loaded world rendered as solid vertex-colored triangles.
//!
//! Chat commands are executed by shipping tools (make_game.py and friends)
//! on background threads; results stream into the log and hot-reload the
//! viewport when a build succeeds.

use litt_math::Vec3;
use std::path::PathBuf;
use std::sync::mpsc::{Receiver, TryRecvError};

/// Width of the chat panel in pixels (viewport = window minus this).
pub const PANEL_W: u32 = 430;
const LINE_H: f32 = 14.0;
const CHAT_LEFT: f32 = 8.0;
const FONT_W: f32 = 4.0; // advance per char at scale 1 (3 px glyph + 1 gap)

// ---------------------------------------------------------------------------
// 3x5 bitmap font. Rows top->bottom, bit2 = left pixel. Printable ASCII only;
// lowercase folds to uppercase shapes.
// ---------------------------------------------------------------------------
#[rustfmt::skip]
const FONT: [[u8; 5]; 95] = [
    [0b000,0b000,0b000,0b000,0b000], // space
    [0b010,0b010,0b010,0b000,0b010], // !
    [0b101,0b101,0b000,0b000,0b000], // "
    [0b101,0b111,0b101,0b111,0b101], // #
    [0b010,0b111,0b010,0b111,0b010], // $  (stylized)
    [0b101,0b001,0b010,0b100,0b101], // %
    [0b010,0b101,0b010,0b101,0b011], // &
    [0b010,0b010,0b000,0b000,0b000], // '
    [0b010,0b100,0b100,0b100,0b010], // (
    [0b010,0b001,0b001,0b001,0b010], // )
    [0b000,0b101,0b010,0b101,0b000], // *
    [0b000,0b010,0b111,0b010,0b000], // +
    [0b000,0b000,0b000,0b010,0b100], // ,
    [0b000,0b000,0b111,0b000,0b000], // -
    [0b000,0b000,0b000,0b000,0b010], // .
    [0b001,0b001,0b010,0b100,0b100], // /
    [0b111,0b101,0b101,0b101,0b111], // 0
    [0b010,0b110,0b010,0b010,0b111], // 1
    [0b111,0b001,0b111,0b100,0b111], // 2
    [0b111,0b001,0b111,0b001,0b111], // 3
    [0b101,0b101,0b111,0b001,0b001], // 4
    [0b111,0b100,0b111,0b001,0b111], // 5
    [0b111,0b100,0b111,0b101,0b111], // 6
    [0b111,0b001,0b001,0b010,0b010], // 7
    [0b111,0b101,0b111,0b101,0b111], // 8
    [0b111,0b101,0b111,0b001,0b111], // 9
    [0b000,0b010,0b000,0b010,0b000], // :
    [0b000,0b010,0b000,0b010,0b100], // ;
    [0b001,0b010,0b100,0b010,0b001], // <
    [0b000,0b111,0b000,0b111,0b000], // =
    [0b100,0b010,0b001,0b010,0b100], // >
    [0b111,0b001,0b011,0b000,0b010], // ?
    [0b111,0b101,0b111,0b100,0b111], // @  (approx)
    [0b010,0b101,0b111,0b101,0b101], // A
    [0b110,0b101,0b110,0b101,0b110], // B
    [0b011,0b100,0b100,0b100,0b011], // C
    [0b110,0b101,0b101,0b101,0b110], // D
    [0b111,0b100,0b110,0b100,0b111], // E
    [0b111,0b100,0b110,0b100,0b100], // F
    [0b011,0b100,0b101,0b101,0b011], // G
    [0b101,0b101,0b111,0b101,0b101], // H
    [0b111,0b010,0b010,0b010,0b111], // I
    [0b001,0b001,0b001,0b101,0b010], // J
    [0b101,0b101,0b110,0b101,0b101], // K
    [0b100,0b100,0b100,0b100,0b111], // L
    [0b101,0b111,0b111,0b101,0b101], // M
    [0b111,0b101,0b101,0b101,0b101], // N
    [0b111,0b101,0b101,0b101,0b111], // O
    [0b111,0b101,0b111,0b100,0b100], // P
    [0b111,0b101,0b101,0b111,0b001], // Q
    [0b111,0b101,0b110,0b101,0b101], // R
    [0b011,0b100,0b010,0b001,0b110], // S
    [0b111,0b010,0b010,0b010,0b010], // T
    [0b101,0b101,0b101,0b101,0b111], // U
    [0b101,0b101,0b101,0b101,0b010], // V
    [0b101,0b101,0b111,0b111,0b101], // W
    [0b101,0b101,0b010,0b101,0b101], // X
    [0b101,0b101,0b010,0b010,0b010], // Y
    [0b111,0b001,0b010,0b100,0b111], // Z
    [0b110,0b100,0b100,0b100,0b110], // [
    [0b100,0b100,0b010,0b001,0b001], // backslash
    [0b011,0b001,0b001,0b001,0b011], // ]
    [0b010,0b101,0b000,0b000,0b000], // ^
    [0b000,0b000,0b000,0b000,0b111], // _
    [0b100,0b010,0b000,0b000,0b000], // `
    [0b111,0b001,0b011,0b101,0b011], // a
    [0b100,0b100,0b110,0b101,0b110], // b
    [0b011,0b100,0b100,0b100,0b011], // c
    [0b001,0b001,0b011,0b101,0b011], // d
    [0b111,0b100,0b110,0b100,0b111], // e
    [0b011,0b100,0b111,0b100,0b100], // f
    [0b011,0b100,0b101,0b011,0b001], // g
    [0b100,0b100,0b110,0b101,0b101], // h
    [0b010,0b000,0b010,0b010,0b010], // i
    [0b001,0b000,0b001,0b001,0b110], // j
    [0b100,0b101,0b110,0b101,0b101], // k
    [0b010,0b010,0b010,0b010,0b110], // l
    [0b101,0b111,0b111,0b101,0b101], // m
    [0b100,0b100,0b110,0b101,0b101], // n
    [0b010,0b101,0b101,0b101,0b010], // o
    [0b110,0b101,0b110,0b100,0b100], // p
    [0b011,0b101,0b011,0b001,0b001], // q
    [0b100,0b100,0b110,0b101,0b101], // r
    [0b011,0b100,0b010,0b001,0b110], // s
    [0b100,0b110,0b100,0b011,0b011], // t  (approx)
    [0b101,0b101,0b101,0b101,0b011], // u
    [0b101,0b101,0b101,0b101,0b010], // v
    [0b101,0b101,0b111,0b111,0b101], // w
    [0b101,0b101,0b010,0b101,0b101], // x
    [0b101,0b101,0b010,0b010,0b010], // y
    [0b111,0b001,0b010,0b100,0b111], // z
    [0b011,0b010,0b010,0b010,0b011], // {
    [0b010,0b010,0b000,0b010,0b010], // |
    [0b110,0b010,0b010,0b010,0b110], // }
    [0b000,0b101,0b010,0b101,0b000], // ~
];

#[inline]
fn push_quad(v: &mut Vec<f32>, x0: f32, y0: f32, x1: f32, y1: f32, c: [f32; 3]) {
    // two triangles, non-indexed
    for &(px, py) in &[
        (x0, y0), (x1, y0), (x1, y1),
        (x0, y0), (x1, y1), (x0, y1),
    ] {
        v.extend_from_slice(&[px, py, 0.0, c[0], c[1], c[2]]);
    }
}

fn draw_char(v: &mut Vec<f32>, ch: u8, x: f32, y: f32, s: f32, c: [f32; 3]) {
    let idx = ch.wrapping_sub(32) as usize;
    if idx >= 95 {
        return;
    }
    let rows = &FONT[idx];
    for (ry, bits) in rows.iter().enumerate() {
        for cx in 0..3 {
            if bits & (1 << (2 - cx)) != 0 {
                let px = x + cx as f32 * s;
                let py = y + ry as f32 * s;
                push_quad(v, px, py, px + s, py + s, c);
            }
        }
    }
}

pub fn text_width(s: &str, scale: f32) -> f32 {
    s.chars().count() as f32 * FONT_W * scale
}

/// Free-floating text into pixel-space panel verts (y down), same
/// convention as `Panel::raster`.
fn draw_str(v: &mut Vec<f32>, s: &str, x: f32, y: f32, scale: f32, c: [f32; 3]) {
    let mut cx = x;
    for ch in s.chars() {
        draw_char(v, ch as u8, cx, y, scale, c);
        cx += FONT_W * scale;
    }
}

/// Play-mode HUD raster: objective/score/lives lines top-left plus an
/// optional centered banner (GOAL REACHED / GAME OVER).
pub fn hud_verts(
    lines: &[(String, [f32; 3])],
    banner: Option<&str>,
    w: u32,
    h: u32,
) -> Vec<f32> {
    let mut v = Vec::with_capacity(8 * 1024);
    let scale = 2.0f32.max((h as f32 / 720.0) * 2.0);
    for (i, (s, c)) in lines.iter().enumerate() {
        // dark backing strip for legibility over any world
        let tw = text_width(s, scale);
        push_quad(
            &mut v,
            6.0,
            4.0 + i as f32 * LINE_H * 1.25,
            12.0 + tw,
            10.0 + i as f32 * LINE_H * 1.25 + LINE_H,
            [0.0, 0.0, 0.02],
        );
        draw_str(&mut v, s, 12.0, 8.0 + i as f32 * LINE_H * 1.25, scale, *c);
    }
    if let Some(b) = banner {
        let bs = 5.0f32.max((h as f32 / 720.0) * 5.0);
        let bw = text_width(b, bs);
        let x = ((w as f32) - bw) * 0.5;
        let y = (h as f32) * 0.16;
        let col: [f32; 3] = if b.starts_with("GAME") { [1.0, 0.35, 0.30] } else { [1.0, 0.85, 0.35] };
        push_quad(&mut v, x - 14.0, y - 10.0, x + bw + 14.0, y + LINE_H * 1.1 + 10.0, [0.03, 0.02, 0.05]);
        draw_str(&mut v, b, x, y, bs, col);
    }
    v
}

/// Wrap a line into chunks of at most `cols` chars (breaks long words).
pub fn wrap(line: &str, cols: usize) -> Vec<String> {
    let cols = cols.max(4);
    let mut out: Vec<String> = Vec::new();
    let mut cur = String::new();
    let mut n = 0usize;
    for word in line.split(' ') {
        let mut rest = word;
        while rest.chars().count() > cols {
            if !cur.is_empty() {
                out.push(std::mem::take(&mut cur));
                n = 0;
            }
            let cut: usize = rest.chars().take(cols).map(|c| c.len_utf8()).sum();
            out.push(rest[..cut].to_string());
            rest = &rest[cut..];
        }
        let wl = rest.chars().count();
        if n == 0 {
            cur.push_str(rest);
            n = wl;
        } else if n + 1 + wl <= cols {
            cur.push(' ');
            cur.push_str(rest);
            n += 1 + wl;
        } else {
            out.push(std::mem::take(&mut cur));
            cur.push_str(rest);
            n = wl;
        }
    }
    out.push(cur);
    out
}

// ---------------------------------------------------------------------------
// Colors (linear-ish sRGB floats)
// ---------------------------------------------------------------------------
pub const COL_BG: [f32; 3] = [0.043, 0.047, 0.078];
pub const COL_BAR: [f32; 3] = [0.09, 0.10, 0.16];
pub const COL_TEXT: [f32; 3] = [0.82, 0.84, 0.88];
pub const COL_SYS: [f32; 3] = [0.30, 0.78, 0.95];
pub const COL_AI: [f32; 3] = [0.45, 0.90, 0.55];
pub const COL_ERR: [f32; 3] = [0.95, 0.45, 0.40];
pub const COL_USER: [f32; 3] = [0.98, 0.85, 0.45];

/// Log line kinds -> colors.
#[derive(Clone, Copy, PartialEq)]
pub enum Kind {
    Sys,
    User,
    Ai,
    Err,
}

impl Kind {
    fn color(self) -> [f32; 3] {
        match self {
            Kind::Sys => COL_SYS,
            Kind::User => COL_USER,
            Kind::Ai => COL_AI,
            Kind::Err => COL_ERR,
        }
    }
}

/// The chat panel state.
pub struct Panel {
    pub lines: Vec<(String, Kind)>,
    pub input: String,
    pub caret_timer: f32,
    pub caret_on: bool,
}

impl Default for Panel {
    fn default() -> Self {
        Self {
            lines: vec![
                ("LITT STUDIO".into(), Kind::Sys),
                ("type 'help' for commands".into(), Kind::Sys),
                ("or just describe a game:".into(), Kind::Sys),
                ("  make random".into(), Kind::Ai),
                ("  make a game about zombie malls".into(), Kind::Ai),
                ("  load kingsfall-hollow".into(), Kind::Ai),
            ],
            input: String::new(),
            caret_timer: 0.0,
            caret_on: true,
        }
    }
}

impl Panel {
    pub fn log(&mut self, s: &str, kind: Kind) {
        for part in s.lines() {
            self.lines.push((part.to_string(), kind));
        }
        if self.lines.len() > 400 {
            self.lines.drain(..self.lines.len() - 400);
        }
    }

    /// Rasterize the whole panel into vertex data (pixel-space, y down).
    pub fn raster(&mut self, w: u32, h: u32, scale: f32) -> Vec<f32> {
        let mut v = Vec::with_capacity(64 * 1024);
        let wf = w as f32;
        let hf = h as f32;
        push_quad(&mut v, 0.0, 0.0, wf, hf, COL_BG);
        // title bar
        push_quad(&mut v, 0.0, 0.0, wf, LINE_H * 1.6, COL_BAR);
        self.text(&mut v, "LITT STUDIO", CHAT_LEFT, 5.0, scale, COL_SYS);

        let cols = ((wf - 16.0) / (FONT_W * scale)) as usize;
        let row_h = LINE_H * 1.35;
        // input line pinned to bottom
        let input_y = hf - row_h * 1.6;
        push_quad(&mut v, 0.0, input_y - 4.0, wf, hf, COL_BAR);
        let shown = format!("> {}", self.input);
        self.text(&mut v, &shown, CHAT_LEFT, input_y + 4.0, scale, COL_USER);
        if self.caret_on {
            let cw = FONT_W * scale;
            let cx = CHAT_LEFT + text_width(&shown, scale) + 1.0;
            push_quad(&mut v, cx, input_y + 4.0, cx + cw * 0.8,
                      input_y + 4.0 + 7.0 * scale, COL_USER);
        }

        // visible log lines, newest at bottom above the input area
        let avail_rows = (((input_y - LINE_H * 1.6) / row_h) as usize).max(1);
        let mut wrapped: Vec<(String, Kind)> = Vec::new();
        for (line, kind) in self.lines.iter().rev().take(120) {
            let parts = wrap(line, cols.max(10));
            for p in parts.iter().rev() {
                wrapped.push((p.clone(), *kind));
            }
            if wrapped.len() >= avail_rows {
                break;
            }
        }
        let mut y = input_y - row_h;
        for (line, kind) in wrapped.iter().take(avail_rows) {
            if y < LINE_H * 1.6 {
                break;
            }
            self.text(&mut v, line, CHAT_LEFT, y, scale, kind.color());
            y -= row_h;
        }
        v
    }

    fn text(&self, v: &mut Vec<f32>, s: &str, x: f32, y: f32, scale: f32, c: [f32; 3]) {
        let mut x = x;
        for bch in s.bytes() {
            draw_char(v, bch, x, y, scale, c);
            x += FONT_W * scale;
        }
    }
}

// ---------------------------------------------------------------------------
// Camera: slow auto-orbit around the world bounds
// ---------------------------------------------------------------------------
pub struct OrbitCam {
    pub angle: f32,
    pub dist: f32,
    pub height: f32,
    pub center: [f32; 3],
    pub spin: bool,
}

impl OrbitCam {
    pub fn from_bounds(min: [f32; 3], max: [f32; 3]) -> Self {
        let center = [
            (min[0] + max[0]) * 0.5,
            (min[1] + max[1]) * 0.5,
            (min[2] + max[2]) * 0.5,
        ];
        let r = ((max[0] - min[0]).powi(2)
            + (max[1] - min[1]).powi(2)
            + (max[2] - min[2]).powi(2))
        .sqrt()
            * 0.5;
        Self { angle: 0.7, dist: (r * 2.4).max(18.0), height: r * 0.9 + 4.0, center, spin: true }
    }

    pub fn tick(&mut self, dt: f32) {
        if self.spin {
            self.angle += dt * 0.12;
        }
        self.caret_blink(dt);
    }

    fn caret_blink(&mut self, dt: f32) {
        let _ = dt; // panel owns caret timing; kept here for symmetry
    }

    /// Column-major MVP for the right-side viewport.
    pub fn mvp(&self, aspect: f32) -> [f32; 16] {
        let eye = [
            self.center[0] + self.angle.cos() * self.dist,
            self.center[1] + self.height,
            self.center[2] + self.angle.sin() * self.dist,
        ];
        mul(perspective(60.0_f32.to_radians(), aspect, 0.1, 6000.0), look_at(eye, self.center))
    }
}

/// Column-major perspective matrix (GL convention).
pub fn perspective(fovy: f32, aspect: f32, near: f32, far: f32) -> [f32; 16] {
    let f = 1.0 / fovy.tan();
    let nf = 1.0 / (near - far);
    [
        f / aspect, 0.0, 0.0, 0.0,
        0.0, f, 0.0, 0.0,
        0.0, 0.0, (far + near) * nf, -1.0,
        0.0, 0.0, 2.0 * far * near * nf, 0.0,
    ]
}

/// Column-major look-at view matrix.
pub fn look_at(eye: [f32; 3], target: [f32; 3]) -> [f32; 16] {
    let up = [0.0, 1.0, 0.0];
    let sub = |a: [f32; 3], b: [f32; 3]| [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
    let norm = |a: [f32; 3]| {
        let l = (a[0] * a[0] + a[1] * a[1] + a[2] * a[2]).sqrt().max(1e-6);
        [a[0] / l, a[1] / l, a[2] / l]
    };
    let cross = |a: [f32; 3], b: [f32; 3]| {
        [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    };
    let dot = |a: [f32; 3], b: [f32; 3]| a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    let z = norm(sub(eye, target));
    let x = norm(cross(up, z));
    let y = cross(z, x);
    [
        x[0], y[0], z[0], 0.0,
        x[1], y[1], z[1], 0.0,
        x[2], y[2], z[2], 0.0,
        -dot(x, eye), -dot(y, eye), -dot(z, eye), 1.0,
    ]
}

/// Column-major ortho mapping pixels (y down) to NDC.
pub fn ortho_pixels(w: u32, h: u32) -> [f32; 16] {
    let wf = w as f32;
    let hf = h as f32;
    [
        2.0 / wf, 0.0, 0.0, 0.0,
        0.0, -2.0 / hf, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        -1.0, 1.0, 0.0, 1.0,
    ]
}

pub fn mul(a: [f32; 16], b: [f32; 16]) -> [f32; 16] {
    let mut o = [0f32; 16];
    for col in 0..4 {
        for row in 0..4 {
            let mut s = 0.0;
            for k in 0..4 {
                s += a[k * 4 + row] * b[col * 4 + k];
            }
            o[col * 4 + row] = s;
        }
    }
    o
}

// ---------------------------------------------------------------------------
// Scene -> vertex soup (world triangles colored by material albedo)
// ---------------------------------------------------------------------------
pub fn scene_to_verts(
    scene: &litt_pathtracer::Scene,
    env: Option<&crate::world_bridge::EnvLight>,
) -> (Vec<f32>, Option<OrbitCam>) {
    let mut v = Vec::with_capacity(scene.triangles.len() * 18);
    let mut min = [f32::MAX; 3];
    let mut max = [f32::MIN; 3];
    let mat = |id: u32| -> Vec3 {
        scene
            .materials
            .get(id as usize)
            .map(|m| m.albedo)
            .unwrap_or(Vec3(0.6, 0.6, 0.6))
    };

    // --- lighting model (baked per triangle) ------------------------------
    // Sun direction/color from the generator's environment when available;
    // pleasant defaults otherwise.
    let (sun_dir, sun_col, sky_col) = match env {
        Some(e) => {
            let el = e.sun_elevation_deg.to_radians();
            let az = e.sun_azimuth_deg.to_radians();
            let d = Vec3(
                az.cos() * el.cos(),
                el.sin().max(0.08),
                az.sin() * el.cos(),
            );
            let k = e.intensity;
            (
                d,
                Vec3(1.05 * k, 0.97 * k, 0.86 * k),
                Vec3(
                    0.35 + e.sky_top.0 * 0.5,
                    0.42 + e.sky_top.1 * 0.5,
                    0.52 + e.sky_top.2 * 0.5,
                ),
            )
        }
        None => (
            Vec3(0.42, 0.72, 0.55),
            Vec3(1.05, 0.99, 0.90),
            Vec3(0.62, 0.70, 0.82),
        ),
    };
    // normalize sun dir once
    let sl = (sun_dir.0 * sun_dir.0 + sun_dir.1 * sun_dir.1 + sun_dir.2 * sun_dir.2)
        .sqrt()
        .max(1e-5);
    let sd = Vec3(sun_dir.0 / sl, sun_dir.1 / sl, sun_dir.2 / sl);

    // Pass 1: shade every triangle (sun diffuse + hemispheric ambient),
    // keep its corners and color for the haze pass.
    let mut shaded: Vec<([[f32; 3]; 3], [f32; 3])> = Vec::with_capacity(scene.triangles.len());
    let mut cx = 0.0f32;
    let mut cy = 0.0f32;
    let mut cz = 0.0f32;
    for t in &scene.triangles {
        let (tv0, tv1, tv2, tnorm, tmid) = (t.v0, t.v1, t.v2, t.normal, t.material_id);
        let c0 = mat(tmid);
        let n = tnorm;
        let nl = (n.0 * n.0 + n.1 * n.1 + n.2 * n.2).sqrt().max(1e-6);
        let nx = n.0 / nl;
        let ny = n.1 / nl;
        let nz = n.2 / nl;

        // sun diffuse (two-sided so backfaces never go pure black)
        let ndl = (nx * sd.0 + ny * sd.1 + nz * sd.2).abs();
        // hemispheric ambient: sky above, dim bounce below
        let amb = 0.34 + 0.22 * ny.clamp(-1.0, 1.0);
        let col = [
            c0.0 * (sky_col.0 * amb + sun_col.0 * 0.85 * ndl),
            c0.1 * (sky_col.1 * amb + sun_col.1 * 0.85 * ndl),
            c0.2 * (sky_col.2 * amb + sun_col.2 * 0.85 * ndl),
        ];
        let cen = [(tv0.0 + tv1.0 + tv2.0) / 3.0,
                   (tv0.1 + tv1.1 + tv2.1) / 3.0,
                   (tv0.2 + tv1.2 + tv2.2) / 3.0];
        cx += cen[0];
        cy += cen[1];
        cz += cen[2];
        let corners = [
            [tv0.0, tv0.1, tv0.2],
            [tv1.0, tv1.1, tv1.2],
            [tv2.0, tv2.1, tv2.2],
        ];
        shaded.push((corners, col));
        for p in [tv0, tv1, tv2] {
            for i in 0..3 {
                let val = match i {
                    0 => p.0,
                    1 => p.1,
                    _ => p.2,
                };
                if val < min[i] { min[i] = val; }
                if val > max[i] { max[i] = val; }
            }
        }
    }

    // Pass 2: distance haze toward the horizon color (depth proxy: distance
    // from the scene centroid - the camera orbits it, so it reads as fog).
    let n3 = (shaded.len() as f32).max(1.0);
    let centre = [cx / n3, cy / n3, cz / n3];
    let radius = (((max[0] - min[0]).powi(2)
        + (max[1] - min[1]).powi(2)
        + (max[2] - min[2]).powi(2))
        .sqrt()
        * 0.5)
        .max(1e-4);
    let horizon = [
        (sky_col.0 * 0.8 + 0.15).min(1.0),
        (sky_col.1 * 0.8 + 0.14).min(1.0),
        (sky_col.2 * 0.8 + 0.13).min(1.0),
    ];
    for tri in &shaded {
        let (corners, col3) = tri;
        let d = ((corners[0][0] + corners[1][0] + corners[2][0]) / 3.0 - centre[0]).powi(2)
            + ((corners[0][1] + corners[1][1] + corners[2][1]) / 3.0 - centre[1]).powi(2)
            + ((corners[0][2] + corners[1][2] + corners[2][2]) / 3.0 - centre[2]).powi(2);
        let d = d.sqrt();
        let f = (d / radius).clamp(0.0, 1.0);
        let haze = f * f * 0.45;
        let c = [
            (col3[0] + (horizon[0] - col3[0]) * haze).min(1.0),
            (col3[1] + (horizon[1] - col3[1]) * haze).min(1.0),
            (col3[2] + (horizon[2] - col3[2]) * haze).min(1.0),
        ];
        for p in corners {
            v.extend_from_slice(&[p[0], p[1], p[2], c[0], c[1], c[2]]);
        }
    }
    let cam = if scene.triangles.is_empty() {
        None
    } else {
        Some(OrbitCam::from_bounds(min, max))
    };
    (v, cam)
}

// ---------------------------------------------------------------------------
// Command bus: run shipped tools on background threads, stream output here
// ---------------------------------------------------------------------------
pub enum StudioMsg {
    Line(String, Kind),
    Done { ok: bool, game_dir: Option<String> },
}

pub struct JobBus {
    pub rx: Receiver<StudioMsg>,
    pub running: bool,
}

impl JobBus {
    pub fn new() -> Self {
        let (_, rx) = std::sync::mpsc::channel();
        Self { rx, running: false }
    }

    pub fn poll(&mut self) -> Vec<StudioMsg> {
        let mut out = Vec::new();
        loop {
            match self.rx.try_recv() {
                Ok(m) => {
                    if matches!(m, StudioMsg::Done { .. }) {
                        self.running = false;
                    }
                    out.push(m);
                }
                Err(TryRecvError::Empty) | Err(TryRecvError::Disconnected) => break,
            }
        }
        out
    }

    /// Spawn `python <script> <args...>` streaming combined output lines.
    pub fn spawn_tool(&mut self, script: &str, args: &[String], finish_hint: Option<String>) {
        use std::io::BufRead;
        use std::process::{Command, Stdio};
        let (tx, rx) = std::sync::mpsc::channel::<StudioMsg>();
        let _ = tx.send(StudioMsg::Line(format!("$ python {} {}", script, args.join(" ")), Kind::Sys));
        let script = script.to_string();
        let args = args.to_vec();
        self.running = true;
        self.rx = rx;
        std::thread::spawn(move || {
            #[cfg(target_os = "windows")]
            let merged = Command::new("cmd")
                .args(["/C", "python"])
                .arg(&script)
                .args(&args)
                .stdout(Stdio::piped())
                .stderr(Stdio::null())
                .spawn();
            #[cfg(not(target_os = "windows"))]
            let merged = Command::new("python")
                .arg(&script)
                .args(&args)
                .stdout(Stdio::piped())
                .stderr(Stdio::null())
                .spawn();

            match merged {
                Ok(mut child) => {
                    if let Some(out) = child.stdout.take() {
                        let rdr = std::io::BufReader::new(out);
                        for line in rdr.lines().map_while(Result::ok) {
                            let kind = if line.starts_with('{') || line.contains("PASS")
                                || line.contains("ready") || line.contains("built")
                                || line.contains("[native]") || line.contains("[make]")
                            {
                                Kind::Ai
                            } else {
                                Kind::Sys
                            };
                            if tx.send(StudioMsg::Line(line, kind)).is_err() {
                                break;
                            }
                        }
                    }
                    let ok = child.wait().map(|s| s.success()).unwrap_or(false);
                    let _ = tx.send(StudioMsg::Done { ok, game_dir: None });
                }
                Err(e) => {
                    let _ = tx.send(StudioMsg::Line(format!("spawn failed: {e}"), Kind::Err));
                    let _ = tx.send(StudioMsg::Done { ok: false, game_dir: None });
                }
            }
            if let Some(hint) = finish_hint {
                let _ = tx.send(StudioMsg::Line(hint, Kind::Sys));
            }
        });
    }

    /// make_game.py --random
    pub fn build_random(&mut self) {
        self.spawn_tool(
            "template/tools/worldgen/make_game.py",
            &["--random".to_string()],
            Some("done - type 'load <name>' or check Project/".into()),
        );
    }

    /// make_game.py --about "<text>"
    pub fn build_about(&mut self, about: &str) {
        // scope the build from the human's own wording ("make me a FULL
        // dark souls game" -> --scale full) so a one-line prompt yields a
        // proportioned game without a follow-up questionnaire.
        let t = about.to_lowercase();
        let scale = if ["full", "big", "huge", "epic", "long", "entire",
                        "whole"]
            .iter()
            .any(|w| t.contains(w))
        {
            "full"
        } else if ["small", "quick", "short", "tiny", "minimal", "demo"]
            .iter()
            .any(|w| t.contains(w))
        {
            "small"
        } else {
            "medium"
        };
        self.spawn_tool(
            "template/tools/worldgen/make_game.py",
            &[
                "--about".to_string(),
                about.to_string(),
                "--scale".to_string(),
                scale.to_string(),
            ],
            Some("done - type 'load <name>' when it finishes".into()),
        );
    }
}

impl Default for JobBus {
    fn default() -> Self { Self::new() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn wrap_respects_columns_and_breaks_long_words() {
        assert_eq!(wrap("short", 20), vec!["short"]);
        let w = wrap("aaa bbb ccc ddd", 7);
        assert!(w.iter().all(|l| l.chars().count() <= 7));
        assert_eq!(w.join(" "), "aaa bbb ccc ddd");
        let long = wrap("abcdefghij", 4);
        assert!(long.iter().all(|l| l.chars().count() <= 4));
    }

    #[test]
    fn fonts_index_safely_for_any_byte() {
        let mut v = Vec::new();
        for b in 0u8..=255 {
            draw_char(&mut v, b, 0.0, 0.0, 1.0, [1.0; 3]);
        }
        assert!(!v.is_empty());
    }

    #[test]
    fn matrices_are_finite_and_invertible_ish() {
        let cam = OrbitCam::from_bounds([-10.0; 3], [10.0; 3]);
        let m = cam.mvp(16.0 / 9.0);
        assert!(m.iter().all(|x| x.is_finite()));
        let o = ortho_pixels(430, 720);
        assert!(o.iter().all(|x| x.is_finite()));
        let p = perspective(1.0, 1.77, 0.1, 100.0);
        assert!(p.iter().all(|x| x.is_finite()));
    }

    #[test]
    fn scene_verts_colors_and_bounds() {
        use litt_pathtracer::*;
        let mut s = Scene::new();
        s.materials.push(MaterialEntry {
            albedo: Vec3(1.0, 0.0, 0.0),
            roughness: 1.0,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
        });
        s.triangles.push(Triangle {
            v0: Vec3(-1.0, 0.0, 0.0),
            v1: Vec3(1.0, 0.0, 0.0),
            v2: Vec3(0.0, 2.0, 0.0),
            normal: Vec3(0.0, 1.0, 0.0),
            material_id: 0,
        });
        let (v, cam) = scene_to_verts(&s, None);
        assert_eq!(v.len(), 18);
        let cam = cam.expect("cam");
        assert_eq!(cam.center, [0.0, 1.0, 0.0]);
        // red channel survives the new bake (sun + ambient keep it bright);
        // green stays dark for an albedo of pure red
        assert!(v[3] > 0.7);
        assert!(v[4] < 0.05);
    }
}
