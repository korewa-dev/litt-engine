//! Broadphase collision detection — spatial hash (CPU) and SAP (GPU-ready)
//!
//! CPU: Spatial hash grid for O(n) average-case broadphase
//! GPU-ready: SAP (Sort-and-Prune) structure for compute shader dispatch

use litt_math::Vec3;
use std::collections::HashMap;

const DEFAULT_CELL_SIZE: f32 = 4.0;

#[derive(Debug, Default)]
pub struct SpatialCell {
    pub bodies: Vec<usize>,
}

/// Spatial hash broadphase for CPU fallback
#[derive(Debug)]
pub struct SpatialHashBroadphase {
    pub cell_size: f32,
    pub grid: HashMap<u64, SpatialCell>,
    pub aabbs: Vec<(Vec3, Vec3)>,
}

impl SpatialHashBroadphase {
    pub fn new(cell_size: f32) -> Self {
        Self { cell_size, grid: HashMap::new(), aabbs: Vec::new() }
    }

    pub fn default_cell() -> Self { Self::new(DEFAULT_CELL_SIZE) }

    /// Build the spatial hash from body AABBs
    pub fn build(&mut self, bodies: &[(Vec3, Vec3)]) {
        self.aabbs.clear();
        self.aabbs.extend(bodies.iter().copied());
        self.grid.clear();

        for (i, &(min, max)) in bodies.iter().enumerate() {
            let cells = self.cell_coords(min, max);
            for cell_key in &cells {
                self.grid.entry(*cell_key)
                    .or_insert_with(SpatialCell::default)
                    .bodies.push(i);
            }
        }
    }

    /// Get candidate collision pairs from the spatial hash
    pub fn find_candidates(&self) -> Vec<(usize, usize)> {
        let mut pairs: Vec<(usize, usize)> = Vec::new();
        let mut seen: Vec<u64> = Vec::new();

        for (_cell_key, cell) in &self.grid {
            if cell.bodies.len() < 2 { continue; }
            let bodies = &cell.bodies;
            for i in 0..bodies.len() {
                for j in (i + 1)..bodies.len() {
                    let a = bodies[i];
                    let b = bodies[j];
                    let pair_key = if a < b { (a as u64) << 32 | (b as u64) } else { (b as u64) << 32 | (a as u64) };
                    if seen.contains(&pair_key) { continue; }
                    seen.push(pair_key);
                    if self.aabbs_overlap(a, b) {
                        pairs.push((a, b));
                    }
                }
            }
        }
        pairs
    }

    fn aabbs_overlap(&self, a: usize, b: usize) -> bool {
        let (amin, amax) = self.aabbs[a];
        let (bmin, bmax) = self.aabbs[b];
        amin.0 <= bmax.0 && bmin.0 <= amax.0
            && amin.1 <= bmax.1 && bmin.1 <= amax.1
            && amin.2 <= bmax.2 && bmin.2 <= amax.2
    }

    fn cell_coords(&self, min: Vec3, max: Vec3) -> Vec<u64> {
        let cs = self.cell_size;
        let min_cx = (min.0 / cs).floor() as i32;
        let min_cy = (min.1 / cs).floor() as i32;
        let min_cz = (min.2 / cs).floor() as i32;
        let max_cx = (max.0 / cs).floor() as i32;
        let max_cy = (max.1 / cs).floor() as i32;
        let max_cz = (max.2 / cs).floor() as i32;

        let mut coords = Vec::new();
        for x in min_cx..=max_cx {
            for y in min_cy..=max_cy {
                for z in min_cz..=max_cz {
                    let key = ((x as u64) << 40) | ((y as u64 & 0xFFFF_FFFF) << 8) | (z as u64 & 0xFF);
                    coords.push(key);
                }
            }
        }
        coords
    }
}

impl Default for SpatialHashBroadphase {
    fn default() -> Self { Self::default_cell() }
}

// =============================================================================
// GPU-ready SAP (Sort and Prune) data structure
// =============================================================================

/// SAP entry for GPU compute shader — 16 bytes, cache-friendly
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
#[repr(C)]
pub struct SAPEntry {
    pub body_index: u32,
    pub start: f32,
    pub end: f32,
    pub pad: u32,
}

/// SAP broadphase — sorted arrays for GPU dispatch
#[derive(Debug)]
pub struct SAPBroadphase {
    pub sort_x: Vec<SAPEntry>,
    pub sort_y: Vec<SAPEntry>,
    pub sort_z: Vec<SAPEntry>,
}

impl SAPBroadphase {
    /// Build SAP from AABB data
    pub fn build(aabbs: &[(Vec3, Vec3)]) -> Self {
        let n = aabbs.len();
        let mut entries: Vec<SAPEntry> = aabbs.iter().enumerate()
            .map(|(i, &(min, max))| SAPEntry {
                body_index: i as u32,
                start: min.0,
                end: max.0,
                pad: 0,
            })
            .collect();

        // Sort by X
        let mut sort_x = entries.clone();
        sort_x.sort_by(|a, b| a.start.partial_cmp(&b.start).unwrap_or(std::cmp::Ordering::Equal));

        // Sort by Y
        let mut sort_y = entries.clone();
        for e in &mut sort_y {
            let idx = e.body_index as usize;
            e.start = aabbs[idx].0.1;
            e.end = aabbs[idx].1.1;
        }
        sort_y.sort_by(|a, b| a.start.partial_cmp(&b.start).unwrap_or(std::cmp::Ordering::Equal));

        // Sort by Z
        let mut sort_z = entries.clone();
        for e in &mut sort_z {
            let idx = e.body_index as usize;
            e.start = aabbs[idx].0.2;
            e.end = aabbs[idx].1.2;
        }
        sort_z.sort_by(|a, b| a.start.partial_cmp(&b.start).unwrap_or(std::cmp::Ordering::Equal));

        Self { sort_x, sort_y, sort_z }
    }

    /// Find overlapping pairs using sweep-and-prune (CPU reference)
    pub fn find_overlaps(&self) -> Vec<(usize, usize)> {
        let mut pairs: Vec<(usize, usize)> = Vec::new();
        let n = self.sort_x.len();
        for i in 0..n {
            for j in (i + 1)..n {
                let a = self.sort_x[i].body_index as usize;
                let b = self.sort_x[j].body_index as usize;
                if self.overlaps_y_z(a, b) {
                    let pair = if a < b { (a, b) } else { (b, a) };
                    if !pairs.contains(&pair) { pairs.push(pair); }
                }
            }
        }
        pairs
    }

    fn overlaps_y_z(&self, a: usize, b: usize) -> bool {
        let ay = self.y_range(a);
        let by = self.y_range(b);
        let az = self.z_range(a);
        let bz = self.z_range(b);
        ay.0 < by.1 && by.0 < ay.1 && az.0 < bz.1 && bz.0 < az.1
    }

    fn y_range(&self, idx: usize) -> (f32, f32) {
        for e in &self.sort_y {
            if e.body_index as usize == idx { return (e.start, e.end); }
        }
        (0.0, 0.0)
    }

    fn z_range(&self, idx: usize) -> (f32, f32) {
        for e in &self.sort_z {
            if e.body_index as usize == idx { return (e.start, e.end); }
        }
        (0.0, 0.0)
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
    fn test_spatial_hash_single_body() {
        let mut bh = SpatialHashBroadphase::default_cell();
        bh.build(&[(Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0))]);
        let pairs = bh.find_candidates();
        assert!(pairs.is_empty());
    }

    #[test]
    fn test_spatial_hash_overlapping() {
        let mut bh = SpatialHashBroadphase::default_cell();
        // Two overlapping AABBs
        bh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(2.0, 2.0, 2.0)),
            (Vec3::new(1.0, 1.0, 1.0), Vec3::new(3.0, 3.0, 3.0)),
        ]);
        let pairs = bh.find_candidates();
        assert_eq!(pairs.len(), 1);
        assert!(pairs[0] == (0, 1));
    }

    #[test]
    fn test_spatial_hash_no_overlap() {
        let mut bh = SpatialHashBroadphase::default_cell();
        // Two separated AABBs
        bh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0)),
            (Vec3::new(5.0, 5.0, 5.0), Vec3::new(6.0, 6.0, 6.0)),
        ]);
        let pairs = bh.find_candidates();
        assert!(pairs.is_empty());
    }

    #[test]
    fn test_spatial_hash_multiple() {
        let mut bh = SpatialHashBroadphase::default_cell();
        // 4 bodies: 0 and 1 overlap, 2 and 3 overlap, 0 and 2 are far apart
        bh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(2.0, 2.0, 2.0)),
            (Vec3::new(1.0, 1.0, 1.0), Vec3::new(3.0, 3.0, 3.0)),
            (Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0)),
            (Vec3::new(11.0, 11.0, 11.0), Vec3::new(13.0, 13.0, 13.0)),
        ]);
        let pairs = bh.find_candidates();
        assert_eq!(pairs.len(), 2);
    }

    #[test]
    fn test_sap_build_and_overlaps() {
        let aabbs = vec![
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(2.0, 2.0, 2.0)),
            (Vec3::new(1.0, 1.0, 1.0), Vec3::new(3.0, 3.0, 3.0)),
            (Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0)),
        ];
        let sap = SAPBroadphase::build(&aabbs);
        let overlaps = sap.find_overlaps();
        // Only bodies 0 and 1 should overlap
        assert!(overlaps.contains(&(0, 1)));
        assert!(!overlaps.contains(&(0, 2)));
        assert!(!overlaps.contains(&(1, 2)));
    }

    #[test]
    fn test_sap_entry_size() {
        assert_eq!(std::mem::size_of::<SAPEntry>(), 16);
    }
}
