//! BVH (Bounding Volume Hierarchy) builder for GPU physics broadphase.
//!
//! Provides a hierarchical bounding volume structure for O(log n) collision
//! queries instead of O(n²) brute force. This is essential for large scenes
//! with many physics bodies.

use litt_math::Vec3;

/// Node in the BVH tree
#[derive(Debug)]
pub enum BvhNode {
    /// Internal node with left and right children
    Internal {
        /// Bounding AABB of this node
        aabb_min: Vec3,
        /// Bounding AABB of this node
        aabb_max: Vec3,
        /// Left child index
        left: usize,
        /// Right child index
        right: usize,
    },
    /// Leaf node containing a single body
    Leaf {
        /// Bounding AABB of this leaf
        aabb_min: Vec3,
        /// Bounding AABB of this leaf
        aabb_max: Vec3,
        /// Body index
        body_idx: usize,
    },
}

/// Bounding Volume Hierarchy for physics broadphase
#[derive(Debug)]
pub struct Bvh {
    /// Tree nodes
    pub nodes: Vec<BvhNode>,
    /// Sorted body indices (for SAH building)
    pub sort_indices: Vec<usize>,
    /// AABB of entire tree
    pub aabb_min: Vec3,
    pub aabb_max: Vec3,
    /// Number of leaf nodes (bodies)
    pub leaf_count: usize,
}

impl Default for Bvh {
    fn default() -> Self { Self::new() }
}

impl Bvh {
    /// Create a new empty BVH
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            sort_indices: Vec::new(),
            aabb_min: Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY),
            aabb_max: Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY),
            leaf_count: 0,
        }
    }

    /// Build BVH from a list of AABBs
    ///
    /// Uses Surface Area Heuristic (SAH) for optimal tree construction.
    pub fn build(&mut self, aabbs: &[(Vec3, Vec3)]) {
        let count = aabbs.len();
        if count == 0 {
            self.nodes.clear();
            self.leaf_count = 0;
            return;
        }

        // Reset AABB
        self.aabb_min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
        self.aabb_max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);

        // Build sorted indices
        self.sort_indices = (0..count).collect();

        // Sort by center along longest axis for initial ordering
        self.sort_indices.sort_by(|a, b| {
            let aa_min = aabbs[*a].0;
            let bb_min = aabbs[*b].0;
            let aa_max = aabbs[*a].1;
            let bb_max = aabbs[*b].1;

            // Find longest axis of combined AABB
            let size_a = Vec3::new(
                aa_max.0 - aa_min.0,
                aa_max.1 - aa_min.1,
                aa_max.2 - aa_min.2,
            );
            let size_b = Vec3::new(
                bb_max.0 - bb_min.0,
                bb_max.1 - bb_min.1,
                bb_max.2 - bb_min.2,
            );
            let combined = Vec3::new(
                size_a.0.max(size_b.0),
                size_a.1.max(size_b.1),
                size_a.2.max(size_b.2),
            );
            let axis = if combined.0 >= combined.1 && combined.0 >= combined.2 {
                0
            } else if combined.1 >= combined.0 && combined.1 >= combined.2 {
                1
            } else {
                2
            };
            (aa_min[axis] + aa_max[axis]).partial_cmp(&(bb_min[axis] + bb_max[axis])).unwrap()
        });

        // Build tree using SAH
        self.nodes.clear();
        self.leaf_count = count;
        self.build_sah(aabbs, 0, count);

        // Update overall AABB
        for &(min, max) in aabbs {
            self.aabb_min = Vec3::new(
                self.aabb_min.0.min(min.0),
                self.aabb_min.1.min(min.1),
                self.aabb_min.2.min(min.2),
            );
            self.aabb_max = Vec3::new(
                self.aabb_max.0.max(max.0),
                self.aabb_max.1.max(max.1),
                self.aabb_max.2.max(max.2),
            );
        }
    }

    /// Rebuild BVH with updated AABBs (for dynamic scenes)
    pub fn rebuild(&mut self, aabbs: &[(Vec3, Vec3)]) {
        // Preserve sort order, just update leaf AABBs
        if aabbs.len() != self.leaf_count {
            self.build(aabbs);
            return;
        }

        // Update leaf nodes in place
        for (i, &(min, max)) in aabbs.iter().enumerate() {
            if let BvhNode::Leaf { aabb_min, aabb_max, .. } = &mut self.nodes[i] {
                *aabb_min = min;
                *aabb_max = max;
            }
        }

        // Update internal node AABBs bottom-up
        self.update_parent_aabbs();
    }

    /// Build BVH using Surface Area Heuristic
    fn build_sah(&mut self, aabbs: &[(Vec3, Vec3)], start: usize, end: usize) -> usize {
        if start >= end {
            return self.nodes.len();
        }

        let count = end - start;

        // Compute combined AABB for this range
        let mut aabb_min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
        let mut aabb_max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);

        for i in start..end {
            let idx = self.sort_indices[i];
            aabb_min = Vec3::new(aabb_min.0.min(aabbs[idx].0.0), aabb_min.1.min(aabbs[idx].0.1), aabb_min.2.min(aabbs[idx].0.2));
            aabb_max = Vec3::new(aabb_max.0.max(aabbs[idx].1.0), aabb_max.1.max(aabbs[idx].1.1), aabb_max.2.max(aabbs[idx].1.2));
        }

        if count <= 4 {
            // Create leaf nodes for small ranges
            let mut leaf_indices = Vec::new();
            for i in start..end {
                let idx = self.sort_indices[i];
                let node_idx = self.nodes.len();
                self.nodes.push(BvhNode::Leaf {
                    aabb_min: aabbs[idx].0,
                    aabb_max: aabbs[idx].1,
                    body_idx: idx,
                });
                leaf_indices.push(node_idx);
            }
            return leaf_indices[0];
        }

        // SAH: find optimal split plane
        let mut best_cost = f32::INFINITY;
        let mut best_axis = 0usize;
        let mut best_pos = 0.0;
        let mut best_split = count / 2;

        let aabb_size = Vec3::new(
            aabb_max.0 - aabb_min.0,
            aabb_max.1 - aabb_min.1,
            aabb_max.2 - aabb_min.2,
        );
        let total_sa = aabb_size.0 * aabb_size.1 + aabb_size.1 * aabb_size.2 + aabb_size.2 * aabb_size.0;

        // Try splitting along each axis
        for axis in 0..3 {
            let mut sorted_by_axis = (start..end)
                .map(|i| (self.sort_indices[i], aabbs[self.sort_indices[i]].0[axis]))
                .collect::<Vec<_>>();
            sorted_by_axis.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap());

            let mut left_count = 0;
            let mut right_count = count;
            let mut left_aabb_min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
            let mut left_aabb_max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);
            let mut right_aabb_min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
            let mut right_aabb_max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);

            for (i, (&body_idx, &pos)) in sorted_by_axis.iter().enumerate() {
                let min = aabbs[body_idx].0;
                let max = aabbs[body_idx].1;
                left_aabb_min = Vec3::new(left_aabb_min.0.min(min.0), left_aabb_min.1.min(min.1), left_aabb_min.2.min(min.2));
                left_aabb_max = Vec3::new(left_aabb_max.0.max(max.0), left_aabb_max.1.max(max.1), left_aabb_max.2.max(max.2));
                left_count += 1;

                // Check split after this element
                if i < sorted_by_axis.len() - 1 {
                    let right_pos = sorted_by_axis[i + 1].1;
                    if right_pos - pos < 1e-6 { continue; } // Avoid overlapping splits

                    // Compute right AABB
                    right_aabb_min = Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY);
                    right_aabb_max = Vec3::new(-f32::INFINITY, -f32::INFINITY, -f32::INFINITY);
                    for &bi in &sorted_by_axis[i + 1..] {
                        let min = aabbs[bi.0].0;
                        let max = aabbs[bi.0].1;
                        right_aabb_min = Vec3::new(right_aabb_min.0.min(min.0), right_aabb_min.1.min(min.1), right_aabb_min.2.min(min.2));
                        right_aabb_max = Vec3::new(right_aabb_max.0.max(max.0), right_aabb_max.1.max(max.1), right_aabb_max.2.max(max.2));
                    }
                    right_count = sorted_by_axis.len() - i - 1;

                    let left_sa = {
                        let s = Vec3::new(
                            left_aabb_max.0 - left_aabb_min.0,
                            left_aabb_max.1 - left_aabb_min.1,
                            left_aabb_max.2 - left_aabb_min.2,
                        );
                        s.0 * s.1 + s.1 * s.2 + s.2 * s.0
                    };
                    let right_sa = {
                        let s = Vec3::new(
                            right_aabb_max.0 - right_aabb_min.0,
                            right_aabb_max.1 - right_aabb_min.1,
                            right_aabb_max.2 - right_aabb_min.2,
                        );
                        s.0 * s.1 + s.1 * s.2 + s.2 * s.0
                    };

                    let cost = (left_count as f32 * left_sa + right_count as f32 * right_sa) / total_sa.max(1e-6);
                    if cost < best_cost {
                        best_cost = cost;
                        best_axis = axis;
                        best_pos = pos;
                        best_split = left_count;
                    }
                }
            }
        }

        // Re-sort by best axis and split
        self.sort_indices[start..end].sort_by(|a, b| {
            aabbs[*a].0[best_axis].partial_cmp(&aabbs[*b].0[best_axis]).unwrap()
        });

        let mid = start + best_split;

        // Create internal node
        let node_idx = self.nodes.len();
        self.nodes.push(BvhNode::Internal {
            aabb_min, aabb_max,
            left: 0, right: 0, // Will be filled after recursive calls
        });

        // Build children
        let left_idx = self.build_sah(aabbs, start, mid);
        let right_idx = self.build_sah(aabbs, mid, end);

        // Update internal node
        if let BvhNode::Internal { left, right, .. } = &mut self.nodes[node_idx] {
            *left = left_idx;
            *right = right_idx;
        }

        node_idx
    }

    /// Update parent AABBs bottom-up after leaf updates
    fn update_parent_aabbs(&mut self) {
        // Walk nodes in reverse order (leaves first)
        for i in (0..self.nodes.len()).rev() {
            match &self.nodes[i] {
                BvhNode::Internal { left, right, .. } => {
                    let left_min = match &self.nodes[*left] {
                        BvhNode::Leaf { aabb_min, .. } => *aabb_min,
                        BvhNode::Internal { aabb_min, .. } => *aabb_min,
                    };
                    let left_max = match &self.nodes[*left] {
                        BvhNode::Leaf { aabb_max, .. } => *aabb_max,
                        BvhNode::Internal { aabb_max, .. } => *aabb_max,
                    };
                    let right_min = match &self.nodes[*right] {
                        BvhNode::Leaf { aabb_min, .. } => *aabb_min,
                        BvhNode::Internal { aabb_min, .. } => *aabb_min,
                    };
                    let right_max = match &self.nodes[*right] {
                        BvhNode::Leaf { aabb_max, .. } => *aabb_max,
                        BvhNode::Internal { aabb_max, .. } => *aabb_max,
                    };

                    if let BvhNode::Internal { aabb_min: min, aabb_max: max, .. } = &mut self.nodes[i] {
                        *min = Vec3::new(left_min.0.min(right_min.0), left_min.1.min(right_min.1), left_min.2.min(right_min.2));
                        *max = Vec3::new(left_max.0.max(right_max.0), left_max.1.max(right_max.1), left_max.2.max(right_max.2));
                    }
                }
                _ => {}
            }
        }
    }

    /// Traverse BVH and collect overlapping AABBs
    pub fn find_overlaps(&self, target_aabb_min: Vec3, target_aabb_max: Vec3, results: &mut Vec<usize>) {
        if self.nodes.is_empty() { return; }
        self.traverse_overlaps(0, target_aabb_min, target_aabb_max, results);
    }

    fn traverse_overlaps(&self, node_idx: usize, target_min: Vec3, target_max: Vec3, results: &mut Vec<usize>) {
        match &self.nodes[node_idx] {
            BvhNode::Internal { aabb_min, aabb_max, left, right, .. } => {
                // Check overlap
                if !self.aabbs_overlap(*aabb_min, *aabb_max, target_min, target_max) {
                    return;
                }
                self.traverse_overlaps(*left, target_min, target_max, results);
                self.traverse_overlaps(*right, target_min, target_max, results);
            }
            BvhNode::Leaf { aabb_min, aabb_max, body_idx, .. } => {
                if self.aabbs_overlap(*aabb_min, *aabb_max, target_min, target_max) {
                    results.push(*body_idx);
                }
            }
        }
    }

    fn aabbs_overlap(a_min: Vec3, a_max: Vec3, b_min: Vec3, b_max: Vec3) -> bool {
        a_min.0 <= b_max.0 && b_min.0 <= a_max.0
            && a_min.1 <= b_max.1 && b_min.1 <= a_max.1
            && a_min.2 <= b_max.2 && b_min.2 <= a_max.2
    }

    /// Get the root node index
    pub fn root(&self) -> Option<usize> {
        if self.nodes.is_empty() { None } else { Some(0) }
    }

    /// Get leaf count
    pub fn leaf_count(&self) -> usize {
        self.leaf_count
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bvh_build_empty() {
        let mut bvh = Bvh::new();
        bvh.build(&[]);
        assert_eq!(bvh.leaf_count, 0);
    }

    #[test]
    fn test_bvh_build_single() {
        let mut bvh = Bvh::new();
        bvh.build(&[(Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0))]);
        assert_eq!(bvh.leaf_count, 1);
    }

    #[test]
    fn test_bvh_build_multiple() {
        let mut bvh = Bvh::new();
        bvh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0)),
            (Vec3::new(2.0, 0.0, 0.0), Vec3::new(3.0, 1.0, 1.0)),
            (Vec3::new(0.5, 0.5, 0.5), Vec3::new(1.5, 1.5, 1.5)),
        ]);
        assert_eq!(bvh.leaf_count, 3);
    }

    #[test]
    fn test_bvh_overlap_detection() {
        let mut bvh = Bvh::new();
        bvh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(2.0, 2.0, 2.0)),
            (Vec3::new(10.0, 10.0, 10.0), Vec3::new(12.0, 12.0, 12.0)),
        ]);

        let mut results = Vec::new();
        bvh.find_overlaps(Vec3::new(0.5, 0.5, 0.5), Vec3::new(1.5, 1.5, 1.5), &mut results);
        assert_eq!(results.len(), 1);
        assert_eq!(results[0], 0);

        results.clear();
        bvh.find_overlaps(Vec3::new(10.5, 10.5, 10.5), Vec3::new(11.5, 11.5, 11.5), &mut results);
        assert_eq!(results.len(), 1);
        assert_eq!(results[0], 1);
    }

    #[test]
    fn test_bvh_rebuild() {
        let mut bvh = Bvh::new();
        bvh.build(&[
            (Vec3::new(0.0, 0.0, 0.0), Vec3::new(1.0, 1.0, 1.0)),
            (Vec3::new(5.0, 5.0, 5.0), Vec3::new(6.0, 6.0, 6.0)),
        ]);

        // Move first body
        let updated = vec![
            (Vec3::new(10.0, 10.0, 10.0), Vec3::new(11.0, 11.0, 11.0)),
            (Vec3::new(5.0, 5.0, 5.0), Vec3::new(6.0, 6.0, 6.0)),
        ];
        bvh.rebuild(&updated);

        let mut results = Vec::new();
        bvh.find_overlaps(Vec3::new(10.5, 10.5, 10.5), Vec3::new(10.6, 10.6, 10.6), &mut results);
        assert_eq!(results.len(), 1);
    }
}
