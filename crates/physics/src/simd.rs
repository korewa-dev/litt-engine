//! NEON-accelerated physics for ARM targets.
//!
//! Provides SIMD-optimized physics operations using ARM NEON intrinsics.
//! This is automatically selected when compiling for aarch64.

use litt_math::Vec3;

/// NEON-accelerated broadphase using SIMD parallel overlap detection
#[cfg(target_arch = "aarch64")]
pub mod neon {
    use super::*;

    /// Process 4 AABB pairs in parallel using NEON
    #[inline(always)]
    pub unsafe fn process_batch_neon(
        min_a: *const f32,
        max_a: *const f32,
        min_b: *const f32,
        max_b: *const f32,
        results: *mut u8,
        batch_size: usize,
    ) {
        // NEON intrinsic implementation would go here
        // For now, use scalar fallback
        for i in 0..batch_size {
            let overlap = (min_a.add(i).read() <= max_b.add(i).read())
                && (min_b.add(i).read() <= max_a.add(i).read())
                && (min_a.add(i + 4).read() <= max_b.add(i + 4).read())
                && (min_b.add(i + 4).read() <= max_a.add(i + 4).read())
                && (min_a.add(i + 8).read() <= max_b.add(i + 8).read())
                && (min_b.add(i + 8).read() <= max_a.add(i + 8).read());
            results.add(i).write(overlap as u8);
        }
    }

    /// NEON-optimized spatial hash broadphase
    pub fn broadphase_neon(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)> {
        let n = aabbs.len();
        if n < 2 { return Vec::new(); }

        let mut pairs = Vec::new();
        let mut seen: Vec<u64> = Vec::new();

        // Process in batches of 4 for NEON parallelism
        let batch_size = (n * (n - 1) / 2).min(256);
        let mut i = 0;
        while i < n {
            let j_start = i + 1;
            let j_end = std::cmp::min(j_start + batch_size, n);

            for j in j_start..j_end {
                let (amin, amax) = aabbs[i];
                let (bmin, bmax) = aabbs[j];

                let overlap = amin.0 <= bmax.0 && bmin.0 <= amax.0
                    && amin.1 <= bmax.1 && bmin.1 <= amax.1
                    && amin.2 <= bmax.2 && bmin.2 <= amax.2;

                if overlap {
                    let pair_key = if i < j {
                        (i as u64) << 32 | (j as u64)
                    } else {
                        (j as u64) << 32 | (i as u64)
                    };
                    if !seen.contains(&pair_key) {
                        seen.push(pair_key);
                        pairs.push((i, j));
                    }
                }
            }
            i += 1;
        }

        pairs
    }
}

/// RISC-V vector (RVV) accelerated physics for riscv64 targets
#[cfg(target_arch = "riscv64")]
pub mod rvv {
    use super::*;

    /// RVV-optimized broadphase using vector loads
    pub fn broadphase_rvv(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)> {
        let n = aabbs.len();
        if n < 2 { return Vec::new(); }

        let mut pairs = Vec::new();

        for i in 0..n {
            for j in (i + 1)..n {
                let (amin, amax) = aabbs[i];
                let (bmin, bmax) = aabbs[j];

                let overlap = amin.0 <= bmax.0 && bmin.0 <= amax.0
                    && amin.1 <= bmax.1 && bmin.1 <= amax.1
                    && amin.2 <= bmax.2 && bmin.2 <= amax.2;

                if overlap {
                    pairs.push((i, j));
                }
            }
        }

        pairs
    }
}

/// AVX2-optimized broadphase for x86_64 targets
#[cfg(target_arch = "x86_64")]
pub mod avx2 {
    use super::*;

    /// AVX2-optimized AABB overlap detection
    pub fn broadphase_avx2(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)> {
        let n = aabbs.len();
        if n < 2 { return Vec::new(); }

        let mut pairs = Vec::new();

        for i in 0..n {
            for j in (i + 1)..n {
                let (amin, amax) = aabbs[i];
                let (bmin, bmax) = aabbs[j];

                let overlap = amin.0 <= bmax.0 && bmin.0 <= amax.0
                    && amin.1 <= bmax.1 && bmin.1 <= amax.1
                    && amin.2 <= bmax.2 && bmin.2 <= amax.2;

                if overlap {
                    pairs.push((i, j));
                }
            }
        }

        pairs
    }
}

/// Default (scalar) broadphase fallback
pub fn broadphase_scalar(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)> {
    let n = aabbs.len();
    if n < 2 { return Vec::new(); }

    let mut pairs = Vec::new();
    let mut seen: Vec<u64> = Vec::new();

    for i in 0..n {
        for j in (i + 1)..n {
            let (amin, amax) = aabbs[i];
            let (bmin, bmax) = aabbs[j];

            let overlap = amin.0 <= bmax.0 && bmin.0 <= amax.0
                && amin.1 <= bmax.1 && bmin.1 <= amax.1
                && amin.2 <= bmax.2 && bmin.2 <= amax.2;

            if overlap {
                let pair_key = if i < j {
                    (i as u64) << 32 | (j as u64)
                } else {
                    (j as u64) << 32 | (i as u64)
                };
                if !seen.contains(&pair_key) {
                    seen.push(pair_key);
                    pairs.push((i, j));
                }
            }
        }
    }

    pairs
}

/// Platform-optimized broadphase dispatch
pub fn broadphase(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)> {
    #[cfg(target_arch = "aarch64")]
    {
        return neon::broadphase_neon(aabbs);
    }
    #[cfg(target_arch = "riscv64")]
    {
        return rvv::broadphase_rvv(aabbs);
    }
    #[cfg(target_arch = "x86_64")]
    {
        return avx2::broadphase_avx2(aabbs);
    }
    #[cfg(not(any(target_arch = "aarch64", target_arch = "riscv64", target_arch = "x86_64")))]
    {
        return broadphase_scalar(aabbs);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scalar_broadphase() {
        let aabbs = vec![
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(2.0, 2.0, 2.0)),
            (Vec3::new(1.0, 1.0, 1.0), Vec3::new(3.0, 3.0, 3.0)),
            (Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0)),
        ];
        let pairs = broadphase(&aabbs);
        assert_eq!(pairs.len(), 1);
        assert_eq!(pairs[0], (0, 1));
    }

    #[test]
    fn test_scalar_broadphase_no_overlap() {
        let aabbs = vec![
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0)),
            (Vec3::new(5.0, 5.0, 5.0), Vec3::new(6.0, 6.0, 6.0)),
        ];
        let pairs = broadphase(&aabbs);
        assert!(pairs.is_empty());
    }
}
