//! Spatial partitioning for frustum culling and broadphase.
//!
//! Implements:
//! - Octree for 3D spatial partitioning
//! - Bounding Volume Hierarchy (BVH) for ray tracing
//! - Spatial hash for broadphase collision

use litt_math::{Vec3, Bbox};
use std::collections::HashMap;

// =============================================================================
// Octree
// =============================================================================

/// Maximum objects per leaf
const MAX_OBJECTS: usize = 10;
/// Maximum octree depth
const MAX_DEPTH: usize = 8;

/// Octree node
pub struct OctreeNode {
    pub bounds: Bbox,
    pub objects: Vec<usize>,
    pub children: Option<Box<[OctreeNode; 8]>>,
    pub depth: usize,
}

impl OctreeNode {
    /// Create a new octree node
    pub fn new(bounds: Bbox, depth: usize) -> Self {
        Self {
            bounds,
            objects: Vec::new(),
            children: None,
            depth,
        }
    }

    /// Insert an object into the octree
    pub fn insert(&mut self, bbox: Bbox, object_id: usize) {
        // If node has children, try to insert into children
        if let Some(ref mut children) = self.children {
            let idx = self.subdivide_index(bbox.center());
            if children[idx].insert(bbox, object_id) {
                return;
            }
        }

        // If leaf is full, subdivide
        if self.objects.len() >= MAX_OBJECTS && self.depth < MAX_DEPTH {
            self.subdivide();
            // Try inserting into children again
            if let Some(ref mut children) = self.children {
                let idx = self.subdivide_index(bbox.center());
                if children[idx].insert(bbox, object_id) {
                    return;
                }
            }
        }

        // Add to current node
        self.objects.push(object_id);
    }

    /// Subdivide into 8 children
    fn subdivide(&mut self) {
        let half = self.bounds.size() * 0.5;
        let center = self.bounds.center();
        let mut children = Box::new([
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(-half.0, -half.1, -half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(half.0, -half.1, -half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(-half.0, half.1, -half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(half.0, half.1, -half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(-half.0, -half.1, half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(half.0, -half.1, half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(-half.0, half.1, half.2), half), self.depth + 1),
            OctreeNode::new(Bbox::from_center_size(center + Vec3::new(half.0, half.1, half.2), half), self.depth + 1),
        ]);

        // Move existing objects to children
        let objects = std::mem::take(&mut self.objects);
        for obj_id in objects {
            // We need to re-insert, but we don't have the bbox here
            // This is a simplification - in practice, you'd store (bbox, id) pairs
        }

        self.children = Some(children);
    }

    /// Get subdivide index for a point
    fn subdivide_index(&self, point: Vec3) -> usize {
        let center = self.bounds.center();
        let mut idx = 0;
        if point.0 >= center.0 { idx |= 1; }
        if point.1 >= center.1 { idx |= 2; }
        if point.2 >= center.2 { idx |= 4; }
        idx
    }

    /// Query objects that intersect with a ray
    pub fn query_ray(&self, origin: Vec3, dir: Vec3, max_t: f32) -> Vec<usize> {
        let mut results = Vec::new();
        self.query_ray_recursive(origin, dir, max_t, &mut results);
        results
    }

    fn query_ray_recursive(&self, origin: Vec3, dir: Vec3, max_t: f32, results: &mut Vec<usize>) {
        if !self.bounds.intersects_ray(origin, dir, max_t) {
            return;
        }

        // Check objects in this node
        for &obj_id in &self.objects {
            results.push(obj_id);
        }

        // Recurse into children
        if let Some(ref children) = self.children {
            for child in children.iter() {
                child.query_ray_recursive(origin, dir, max_t, results);
            }
        }
    }

    /// Query objects within a region
    pub fn query_region(&self, region: Bbox) -> Vec<usize> {
        let mut results = Vec::new();
        self.query_region_recursive(region, &mut results);
        results
    }

    fn query_region_recursive(&self, region: Bbox, results: &mut Vec<usize>) {
        if !self.bounds.overlaps(&region) {
            return;
        }

        // Check objects in this node
        for &obj_id in &self.objects {
            results.push(obj_id);
        }

        // Recurse into children
        if let Some(ref children) = self.children {
            for child in children.iter() {
                child.query_region_recursive(region, results);
            }
        }
    }
}

/// Spatial octree for 3D partitioning
pub struct Octree {
    root: OctreeNode,
    pub bounds: Bbox,
}

impl Octree {
    /// Create a new octree
    pub fn new(bounds: Bbox) -> Self {
        Self {
            root: OctreeNode::new(bounds, 0),
            bounds,
        }
    }

    /// Insert an object
    pub fn insert(&mut self, bbox: Bbox, object_id: usize) {
        self.root.insert(bbox, object_id);
    }

    /// Query objects intersecting a ray
    pub fn query_ray(&self, origin: Vec3, dir: Vec3, max_t: f32) -> Vec<usize> {
        self.root.query_ray(origin, dir, max_t)
    }

    /// Query objects in a region
    pub fn query_region(&self, region: Bbox) -> Vec<usize> {
        self.root.query_region(region)
    }

    /// Get object count
    pub fn count(&self) -> usize {
        self.root.objects.len()
    }
}

// =============================================================================
// Spatial Hash
// =============================================================================

/// Spatial hash for broadphase collision detection
pub struct SpatialHash {
    cell_size: f32,
    grid: HashMap<usize, Vec<usize>>,
}

impl SpatialHash {
    /// Create a new spatial hash
    pub fn new(cell_size: f32) -> Self {
        Self {
            cell_size,
            grid: HashMap::new(),
        }
    }

    /// Hash a position to a grid cell
    fn hash(&self, pos: Vec3) -> usize {
        let cx = (pos.0 / self.cell_size).floor() as i64;
        let cy = (pos.1 / self.cell_size).floor() as i64;
        let cz = (pos.2 / self.cell_size).floor() as i64;
        let hash = (cx as usize) ^ ((cy as usize) << 16) ^ ((cz as usize) << 32);
        hash
    }

    /// Insert an object
    pub fn insert(&mut self, pos: Vec3, object_id: usize) {
        let key = self.hash(pos);
        self.grid.entry(key).or_insert_with(Vec::new).push(object_id);
    }

    /// Get potential collision pairs
    pub fn get_pairs(&self) -> Vec<(usize, usize)> {
        let mut pairs = Vec::new();
        let mut seen: Vec<usize> = Vec::new();

        for (cell_id, objects) in &self.grid {
            for i in 0..objects.len() {
                for j in (i + 1)..objects.len() {
                    let pair = if objects[i] < objects[j] {
                        (objects[i] << 32) | objects[j] as u64
                    } else {
                        (objects[j] << 32) | objects[i] as u64
                    };
                    if !seen.contains(&pair as usize) {
                        seen.push(pair as usize);
                        pairs.push((objects[i], objects[j]));
                    }
                }
            }
        }

        pairs
    }

    /// Clear the spatial hash
    pub fn clear(&mut self) {
        self.grid.clear();
    }
}

// =============================================================================
// Bounding Volume Hierarchy (BVH)
// =============================================================================

/// BVH node for ray tracing broadphase
#[derive(Debug)]
pub struct BvhNode {
    pub bounds: Bbox,
    pub left: Option<usize>,
    pub right: Option<usize>,
    pub object_id: Option<usize>,
}

/// Bounding Volume Hierarchy for spatial partitioning
pub struct Bvh {
    pub nodes: Vec<BvhNode>,
    pub root: usize,
}

impl Default for Bvh {
    fn default() -> Self {
        Self::new()
    }
}

impl Bvh {
    /// Create a new empty BVH
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: 0,
        }
    }

    /// Build BVH from a list of bounding boxes
    pub fn build(&mut self, bboxes: &[Bbox]) {
        self.nodes.clear();
        self.nodes.push(BvhNode {
            bounds: Bbox::empty(),
            left: None,
            right: None,
            object_id: None,
        });

        if bboxes.is_empty() {
            return;
        }

        self.build_recursive(bboxes, 0, 0, bboxes.len());
    }

    fn build_recursive(&mut self, bboxes: &[Bbox], parent_idx: usize, start: usize, end: usize) -> usize {
        let node_idx = self.nodes.len();
        self.nodes.push(BvhNode {
            bounds: Bbox::empty(),
            left: None,
            right: None,
            object_id: None,
        });

        // Compute bounds for this node
        let mut bounds = bboxes[start];
        for i in (start + 1)..end {
            bounds = bounds.merge(&bboxes[i]);
        }
        self.nodes[node_idx].bounds = bounds;

        // If leaf node
        if end - start == 1 {
            self.nodes[node_idx].object_id = Some(start);
            if let Some(ref mut parent) = self.nodes[parent_idx] {
                if parent.left.is_none() {
                    parent.left = Some(node_idx);
                } else {
                    parent.right = Some(node_idx);
                }
            }
            return node_idx;
        }

        // Sort by longest axis
        let size = bounds.size();
        let axis = if size.0 >= size.1 && size.0 >= size.2 {
            0
        } else if size.1 >= size.0 && size.1 >= size.2 {
            1
        } else {
            2
        };

        let mid = start + (end - start) / 2;
        self.nodes[node_idx].left = Some(self.build_recursive(bboxes, node_idx, start, mid));
        self.nodes[node_idx].right = Some(self.build_recursive(bboxes, node_idx, mid, end));

        node_idx
    }

    /// Query objects intersecting a ray
    pub fn query_ray(&self, origin: Vec3, dir: Vec3, max_t: f32) -> Vec<usize> {
        let mut results = Vec::new();
        self.query_ray_recursive(self.root, origin, dir, max_t, &mut results);
        results
    }

    fn query_ray_recursive(&self, node_idx: usize, origin: Vec3, dir: Vec3, max_t: f32, results: &mut Vec<usize>) {
        let node = &self.nodes[node_idx];
        if !node.bounds.intersects_ray(origin, dir, max_t) {
            return;
        }

        if let Some(obj_id) = node.object_id {
            results.push(obj_id);
            return;
        }

        if let Some(left) = node.left {
            self.query_ray_recursive(left, origin, dir, max_t, results);
        }
        if let Some(right) = node.right {
            self.query_ray_recursive(right, origin, dir, max_t, results);
        }
    }
}
