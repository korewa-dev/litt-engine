#version 450
#extension GL_EXT_scalar_block_layout : require

// Physics Broadphase -- Spatial Hash Compute Shader
// Runs on RDNA compute units for GPU-accelerated broadphase collision detection

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct PhysicsBody {
    uint shape_type;
    float mass;
    float inv_mass;
    vec3 linear_velocity;
    vec3 angular_velocity;
    float linear_damping;
    float angular_damping;
    float friction;
    float restitution;
    uint layer;
    uint is_trigger;
    float gravity_scale;
    float shape_data[4];
};

layout(set = 0, binding = 0, scalar) buffer Bodies {
    PhysicsBody bodies[];
} uBodies;

layout(set = 0, binding = 1, scalar) buffer Grid {
    uint grid[];
} uGrid;

layout(set = 0, binding = 2, scalar) uniform Params {
    uint body_count;
    float cell_size;
    uint grid_size;
    uint pad;
} uParams;

uint hash_vec3(vec3 p) {
    uint h = uint(p.x * 73856093.0);
    h ^= uint(p.y * 19349663.0);
    h ^= uint(p.z * 83492791.0);
    return h;
}

uint hash_cell(vec3 cell_coords) {
    return hash_vec3(cell_coords) ^ (hash_vec3(cell_coords.yzx) << 1);
}

void main() {
    uint body_idx = gl_GlobalInvocationID.x;
    if (body_idx >= uParams.body_count) return;

    PhysicsBody body = uBodies.bodies[body_idx];
    float cell_size = uParams.cell_size;
    vec3 center = body.linear_velocity;

    vec3 half_extent = vec3(0.5);
    if (body.shape_type == 0) {
        half_extent = vec3(body.shape_data[0], body.shape_data[1], body.shape_data[2]);
    } else if (body.shape_type == 1) {
        half_extent = vec3(body.shape_data[0]);
    } else if (body.shape_type == 2) {
        half_extent = vec3(body.shape_data[0], body.shape_data[1], body.shape_data[0]);
    }

    vec3 min_pos = center - half_extent;
    vec3 max_pos = center + half_extent;
    vec3 min_cell = min_pos / cell_size;
    vec3 max_cell = max_pos / cell_size;

    for (float x = floor(min_cell.x); x <= float(floor(max_cell.x)); x++) {
        for (float y = floor(min_cell.y); y <= float(floor(max_cell.y)); y++) {
            for (float z = floor(min_cell.z); z <= float(floor(max_cell.z)); z++) {
                vec3 cell = vec3(x, y, z);
                uint cell_hash = hash_cell(cell);
                uint grid_idx = cell_hash % uParams.grid_size;
                uint existing = uGrid.grid[grid_idx];
                if (existing == 0xFFFFFFFFu) {
                    uGrid.grid[grid_idx] = body_idx | 0x80000000u;
                }
            }
        }
    }
}
