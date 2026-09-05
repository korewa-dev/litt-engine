#version 450
#extension GL_EXT_scalar_block_layout : require

// Physics Integrate Compute Shader
// Applies gravity, damping, and integrates positions on GPU

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

struct Transform {
    vec3 position;
    vec4 rotation;
    vec3 scale;
    float pad;
};

layout(set = 0, binding = 0, scalar) buffer Bodies {
    PhysicsBody bodies[];
} uBodies;

layout(set = 0, binding = 1, scalar) buffer Transforms {
    Transform transforms[];
} uTransforms;

layout(set = 0, binding = 2, scalar) uniform Params {
    uint body_count;
    vec3 gravity;
    float dt;
    uint pad;
} uParams;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uParams.body_count) return;

    PhysicsBody body = uBodies.bodies[idx];
    if (body.mass <= 0.0) return;

    vec3 gravity = uParams.gravity * body.gravity_scale;
    vec3 acceleration = gravity;

    // Semi-implicit Euler
    body.linear_velocity = (body.linear_velocity + acceleration * uParams.dt)
        * (1.0 - body.linear_damping * uParams.dt);

    float max_speed = 100.0;
    if (length(body.linear_velocity) > max_speed) {
        body.linear_velocity = normalize(body.linear_velocity) * max_speed;
    }

    transforms[idx].position = transforms[idx].position + body.linear_velocity * uParams.dt;

    // Ground collision
    if (transforms[idx].position.y < 0.0) {
        transforms[idx].position.y = 0.0;
        body.linear_velocity.y = -body.linear_velocity.y * body.restitution;
        body.linear_velocity.x *= (1.0 - body.friction);
        body.linear_velocity.z *= (1.0 - body.friction);
    }

    uBodies.bodies[idx].linear_velocity = body.linear_velocity;
    uBodies.bodies[idx].angular_velocity = body.angular_velocity;
}
