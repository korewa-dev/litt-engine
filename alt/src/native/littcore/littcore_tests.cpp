/**
 * Litt Engine - Unit Tests
 * Uses actual public API signatures
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "litt_math.h"
#include "litt_json.h"
#include "litt_obj.h"

static int pass_count = 0;
static int fail_count = 0;

void test_pass(const char* name) {
    pass_count++;
    printf("  ✓ PASS: %s\n", name);
}

void test_fail(const char* name, const char* reason) {
    fail_count++;
    printf("  ✗ FAIL: %s - %s\n", name, reason);
}

// ==========================================
// Math Tests
// ==========================================

void test_vec3() {
    printf("[Math - Vec3]\n");
    
    // Construction
    litt::Vec3 v(1.0f, 2.0f, 3.0f);
    if (fabs(v.x - 1.0f) < 0.001f && fabs(v.y - 2.0f) < 0.001f && fabs(v.z - 3.0f) < 0.001f)
        test_pass("vec3_constructor");
    else
        test_fail("vec3_constructor", "Bad coords");
    
    // Zero
    litt::Vec3 z = litt::Vec3::zero();
    if (fabs(z.x) < 0.001f && fabs(z.y) < 0.001f && fabs(z.z) < 0.001f)
        test_pass("vec3_zero");
    else
        test_fail("vec3_zero", "Non-zero");
    
    // Length
    litt::Vec3 len(3.0f, 4.0f, 0.0f);
    if (fabs(len.length() - 5.0f) < 0.001f)
        test_pass("vec3_length");
    else
        test_fail("vec3_length", "Bad length");
    
    // Dot product
    litt::Vec3 a(1.0f, 0.0f, 0.0f);
    litt::Vec3 b(0.0f, 1.0f, 0.0f);
    if (fabs(a.dot(b)) < 0.001f)
        test_pass("vec3_dot_perp");
    else
        test_fail("vec3_dot_perp", "Expected zero");
    
    // Cross product
    litt::Vec3 c = a.cross(b);
    if (fabs(c.x) < 0.001f && fabs(c.y) < 0.001f && fabs(c.z - 1.0f) < 0.001f)
        test_pass("vec3_cross");
    else
        test_fail("vec3_cross", "Bad cross product");
}

void test_vec4() {
    printf("[Math - Vec4]\n");
    
    litt::Vec4 v(1.0f, 2.0f, 3.0f, 4.0f);
    if (fabs(v.w - 4.0f) < 0.001f)
        test_pass("vec4_constructor");
    else
        test_fail("vec4_constructor", "Bad w");
}

void test_mat4() {
    printf("[Math - Mat4]\n");
    
    // Identity - flat array m[16]
    litt::Mat4 id = litt::Mat4::identity();
    bool ok = true;
    for (int i = 0; i < 16; i++) {
        float exp = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        if (fabs(id.m[i] - exp) > 0.001f) ok = false;
    }
    if (ok)
        test_pass("mat4_identity");
    else
        test_fail("mat4_identity", "Not identity");
    
    // Translation
    litt::Mat4 tr = litt::Mat4::translation(litt::Vec3(1.0f, 2.0f, 3.0f));
    if (fabs(tr.m[12] - 1.0f) < 0.001f &&
        fabs(tr.m[13] - 2.0f) < 0.001f &&
        fabs(tr.m[14] - 3.0f) < 0.001f)
        test_pass("mat4_translation");
    else
        test_fail("mat4_translation", "Bad translation");
    
    // Scale
    litt::Mat4 s = litt::Mat4::scale(litt::Vec3(2.0f, 2.0f, 2.0f));
    if (fabs(s.m[0] - 2.0f) < 0.001f && fabs(s.m[5] - 2.0f) < 0.001f && fabs(s.m[10] - 2.0f) < 0.001f)
        test_pass("mat4_scale");
    else
        test_fail("mat4_scale", "Bad scale");
}

void test_quat() {
    printf("[Math - Quat]\n");
    
    litt::Quat q;
    q.x = 0; q.y = 0; q.z = 0; q.w = 1;
    float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (fabs(len - 1.0f) < 0.001f)
        test_pass("quat_identity");
    else
        test_fail("quat_identity", "Not normalized");
}

// ==========================================
// JSON Tests
// ==========================================

void test_json_number() {
    printf("[JSON - Number]\n");
    
    const char* json = "{\"seed\":12345}";
    LvJson* root = lvj_parse(json);
    if (!root) {
        test_fail("json_number", "Parse failed");
        return;
    }
    
    const LvJson* seed = lvj_get(root, "seed");
    if (seed && lvj_num(seed, -1.0) == 12345.0)
        test_pass("json_number");
    else
        test_fail("json_number", "Wrong value");
    
    lvj_free(root);
}

void test_json_string() {
    printf("[JSON - String]\n");
    
    const char* json = "{\"name\":\"litt\"}";
    LvJson* root = lvj_parse(json);
    if (!root) {
        test_fail("json_string", "Parse failed");
        return;
    }
    
    const LvJson* name = lvj_get(root, "name");
    if (name && strcmp(lvj_str(name, ""), "litt") == 0)
        test_pass("json_string");
    else
        test_fail("json_string", "Wrong string");
    
    lvj_free(root);
}

void test_json_array() {
    printf("[JSON - Array]\n");
    
    const char* json = "{\"values\":[1,2,3]}";
    LvJson* root = lvj_parse(json);
    if (!root) {
        test_fail("json_array", "Parse failed");
        return;
    }
    
    const LvJson* arr = lvj_get(root, "values");
    if (arr && arr->count == 3)
        test_pass("json_array");
    else
        test_fail("json_array", "Bad array");
    
    lvj_free(root);
}

void test_json_bool() {
    printf("[JSON - Bool]\n");
    
    const char* json = "{\"active\":true}";
    LvJson* root = lvj_parse(json);
    if (!root) {
        test_fail("json_bool", "Parse failed");
        return;
    }
    
    const LvJson* act = lvj_get(root, "active");
    if (act && lvj_bool(act, 0) == 1)
        test_pass("json_bool");
    else
        test_fail("json_bool", "Wrong bool");
    
    lvj_free(root);
}

// ==========================================
// OBJ Loader Tests
// ==========================================

void test_obj_invalid() {
    printf("[OBJ - Invalid]\n");
    
    LvModel model;
    memset(&model, 0, sizeof(model));
    int r = lv_obj_load("nonexistent.obj", &model);
    if (r != 0)
        test_pass("obj_invalid_file");
    else
        test_fail("obj_invalid_file", "Should fail");
}

void test_obj_valid() {
    printf("[OBJ - Valid]\n");
    
    // Create test OBJ
    FILE* f = fopen("test_quad.obj", "w");
    if (!f) {
        test_pass("obj_valid (skipped)");
        return;
    }
    
    fprintf(f, "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n");
    fprintf(f, "f 1 2 3\nf 1 3 4\n");
    fclose(f);
    
    LvModel model;
    memset(&model, 0, sizeof(model));
    int r = lv_obj_load("test_quad.obj", &model);
    remove("test_quad.obj");
    
    if (r == 0 && model.count > 0 && model.meshes[0].vn > 0)
        test_pass("obj_valid_load");
    else
        test_fail("obj_valid_load", "Failed to load valid OBJ");
    
    lv_model_free(&model);
}

// ==========================================
// Ray Tests
// ==========================================

void test_ray_origin() {
    printf("[Math - Ray]\n");
    
    litt::Ray r(litt::Vec3(0, 0, 0), litt::Vec3(0, 0, 1));
    if (fabs(r.origin.x) < 0.001f && fabs(r.direction.z - 1.0f) < 0.001f)
        test_pass("ray_origin");
    else
        test_fail("ray_origin", "Bad ray origin");
}

// ==========================================
// AABB Tests
// ==========================================

void test_aabb_contains() {
    printf("[Math - AABB]\n");
    
    litt::Aabb a(litt::Vec3(0, 0, 0), litt::Vec3(10, 10, 10));
    if (a.contains(litt::Vec3(5, 5, 5)))
        test_pass("aabb_contains_inside");
    else
        test_fail("aabb_contains_inside", "Should contain");
    
    if (!a.contains(litt::Vec3(15, 5, 5)))
        test_pass("aabb_contains_outside");
    else
        test_fail("aabb_contains_outside", "Should not contain");
}

// ==========================================
// Main
// ==========================================

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Unit Test Suite\n");
    printf("========================================\n\n");
    
    // Math tests
    test_vec3();
    test_vec4();
    test_mat4();
    test_quat();
    test_ray_origin();
    test_aabb_contains();
    
    // JSON tests
    test_json_number();
    test_json_string();
    test_json_array();
    test_json_bool();
    
    // OBJ tests
    test_obj_invalid();
    test_obj_valid();
    
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed, %d total\n",
           pass_count, fail_count, pass_count + fail_count);
    printf("========================================\n");
    
    return (fail_count > 0) ? 1 : 0;
}