// ============================================================================
// Litt Engine - Math Library Performance Benchmarks   (native/benchmarks.cpp)
// Benchmarks litt::Vec3/Mat4/Quat/Aabb vs equivalent hand-written raw floats.
// Build: clang++ -std=c++17 -O2 benchmarks.cpp -I. -o bin/mathbench
// ============================================================================

#include "littcore/litt_math.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

using litt::Aabb;
using litt::Mat4;
using litt::Quat;
using litt::Vec3;

static volatile double g_sink = 0.0;   // every result folds into this: no dead code

// Quat::slerp is not in litt_math.h yet - shortest-arc implementation kept here.
static Quat quat_slerp(const Quat& a, const Quat& b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    float bx = b.x, by = b.y, bz = b.z, bw = b.w;
    if (d < 0.0f) { d = -d; bx = -bx; by = -by; bz = -bz; bw = -bw; }
    float theta = std::acos(d < 1.0f ? d : 1.0f);
    if (theta < 1e-5f) return a;
    float st = std::sin(theta);
    float w0 = std::sin((1.0f - t) * theta) / st, w1 = std::sin(t * theta) / st;
    return Quat(a.x * w0 + bx * w1, a.y * w0 + by * w1, a.z * w0 + bz * w1, a.w * w0 + bw * w1).normalized();
}

static void raw_slerp(const float a[4], const float b[4], float t, float out[4]) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    float b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    if (d < 0.0f) { d = -d; b0 = -b0; b1 = -b1; b2 = -b2; b3 = -b3; }
    float theta = std::acos(d < 1.0f ? d : 1.0f);
    if (theta < 1e-5f) { out[0] = a[0]; out[1] = a[1]; out[2] = a[2]; out[3] = a[3]; return; }
    float st = std::sin(theta);
    float w0 = std::sin((1.0f - t) * theta) / st, w1 = std::sin(t * theta) / st;
    out[0] = a[0] * w0 + b0 * w1; out[1] = a[1] * w0 + b1 * w1;
    out[2] = a[2] * w0 + b2 * w1; out[3] = a[3] * w0 + b3 * w1;
}

template <typename Fn>
static double bench_ns(size_t iters, Fn fn) {
    for (size_t i = 0; i < iters / 10; ++i) fn(i);      // warmup pass
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < iters; ++i) fn(i);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(iters);
}

static struct Row { const char* group, *name; double ns_lib, ns_raw; } g_rows[16];
static int g_nrows = 0;

template <typename LibFn, typename RawFn>
static void run(const char* group, const char* name, size_t iters, LibFn lf, RawFn rf) {
    g_rows[g_nrows++] = {group, name, bench_ns(iters, lf), bench_ns(iters, rf)};
}

int main() {
    constexpr size_t N = 64;                // rotating operand pool: no constant folding
    constexpr size_t ITERS = 2000000;       // timed iterations per benchmark (1M+)
    uint32_t seed = 0x4C495454u;            // "LITT" - deterministic operand data
    auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return float(seed % 2000u) * 0.001f - 1.0f; };

    // Operand pools + raw-float mirrors ---------------------------------------
    Vec3 va[N], vb[N], vp[N];
    Mat4 ma[N], mb[N];
    Quat qa[N], qb[N];
    Aabb ba[N], bb[N];
    float ra[N][3], rb[N][3], rp[N][3];
    float qra[N][4], qrb[N][4], qs_out[N][4];
    float mra[N][16], mrb[N][16];
    float amin[N][3], amax[N][3], bmin[N][3], bmax[N][3];

    for (size_t i = 0; i < N; ++i) {
        va[i] = Vec3(rnd(), rnd(), rnd());
        vb[i] = Vec3(rnd(), rnd(), rnd());
        vp[i] = va[i] * 0.5f;
        ma[i] = Mat4::translation(va[i]) * Mat4::rot_y(va[i].x * 3.0f)
              * Mat4::scale(vb[i] * 0.25f + Vec3(1, 1, 1));
        mb[i] = Mat4::translation(vb[i]) * Mat4::rot_x(vb[i].y * 2.0f);
        qa[i] = Quat::from_axis_angle(va[i].normalized(), va[i].x * 2.0f);
        qb[i] = Quat::from_axis_angle(vb[i].normalized(), vb[i].y * 2.0f);
        Vec3 c = va[i] * 2.0f, e = vb[i] * 0.75f + Vec3(1, 1, 1);
        ba[i] = Aabb(c - e, c + e);
        bb[i] = Aabb(c - e + Vec3(0.9f, 0.9f, 0.9f), c + e + Vec3(0.9f, 0.9f, 0.9f));
        for (int k = 0; k < 3; ++k) {
            ra[i][k] = va[i][k]; rb[i][k] = vb[i][k]; rp[i][k] = vp[i][k];
            amin[i][k] = ba[i].min[k]; amax[i][k] = ba[i].max[k];
            bmin[i][k] = bb[i].min[k]; bmax[i][k] = bb[i].max[k];
        }
        for (int k = 0; k < 16; ++k) mra[i][k] = ma[i].m[k], mrb[i][k] = mb[i].m[k];
        qra[i][0] = qa[i].x; qra[i][1] = qa[i].y; qra[i][2] = qa[i].z; qra[i][3] = qa[i].w;
        qrb[i][0] = qb[i].x; qrb[i][1] = qb[i].y; qrb[i][2] = qb[i].z; qrb[i][3] = qb[i].w;
    }

    float acc = 0.0f;   // accumulators keep results observable

    // Vec3 ---------------------------------------------------------------------
    run("Vec3", "add", ITERS,
        [&](size_t i) { Vec3 r = va[i & 63] + vb[i & 63]; acc += r.x + r.y + r.z; },
        [&](size_t i) { size_t k = i & 63;
            acc += ra[k][0] + rb[k][0] + ra[k][1] + rb[k][1] + ra[k][2] + rb[k][2]; });
    run("Vec3", "mul", ITERS,
        [&](size_t i) { Vec3 r = va[i & 63] * 2.71828f; acc += r.x + r.y + r.z; },
        [&](size_t i) { size_t k = i & 63;
            acc += ra[k][0] * 2.71828f + ra[k][1] * 2.71828f + ra[k][2] * 2.71828f; });
    run("Vec3", "dot", ITERS,
        [&](size_t i) { acc += va[i & 63].dot(vb[i & 63]); },
        [&](size_t i) { size_t k = i & 63;
            acc += ra[k][0] * rb[k][0] + ra[k][1] * rb[k][1] + ra[k][2] * rb[k][2]; });
    run("Vec3", "cross", ITERS,
        [&](size_t i) { Vec3 r = va[i & 63].cross(vb[i & 63]); acc += r.x + r.y + r.z; },
        [&](size_t i) { size_t k = i & 63;
            acc += ra[k][1] * rb[k][2] - ra[k][2] * rb[k][1]
                 + ra[k][2] * rb[k][0] - ra[k][0] * rb[k][2]
                 + ra[k][0] * rb[k][1] - ra[k][1] * rb[k][0]; });
    run("Vec3", "normalize", ITERS,
        [&](size_t i) { Vec3 r = va[i & 63].normalized(); acc += r.x + r.y + r.z; },
        [&](size_t i) { size_t k = i & 63;
            float l = std::sqrt(ra[k][0] * ra[k][0] + ra[k][1] * ra[k][1] + ra[k][2] * ra[k][2]);
            if (l > MATH_EPS) acc += (ra[k][0] + ra[k][1] + ra[k][2]) / l; });

    // Mat4 ---------------------------------------------------------------------
    run("Mat4", "multiply", ITERS,
        [&](size_t i) { Mat4 r = ma[i & 63] * mb[i & 63];
                        acc += r.m[0] + r.m[5] + r.m[10] + r.m[15]; },
        [&](size_t i) { size_t n = i & 63; float t[16];
            for (int j = 0; j < 4; ++j) for (int c = 0; c < 4; ++c) {
                float s = 0;
                for (int q = 0; q < 4; ++q) s += mra[n][q + j * 4] * mrb[n][c + q * 4];
                t[c + j * 4] = s; }
            acc += t[0] + t[5] + t[10] + t[15]; });
    run("Mat4", "transform", ITERS,
        [&](size_t i) { Vec3 r = ma[i & 63] * vp[i & 63]; acc += r.x + r.y + r.z; },
        [&](size_t i) { size_t k = i & 63;
            acc += mra[k][0] * rp[k][0] + mra[k][4] * rp[k][1] + mra[k][8] * rp[k][2] + mra[k][12]
                 + mra[k][1] * rp[k][0] + mra[k][5] * rp[k][1] + mra[k][9] * rp[k][2] + mra[k][13]
                 + mra[k][2] * rp[k][0] + mra[k][6] * rp[k][1] + mra[k][10] * rp[k][2] + mra[k][14]; });

    // Quat ---------------------------------------------------------------------
    run("Quat", "multiply", ITERS,
        [&](size_t i) { Quat r = qa[i & 63] * qb[i & 63]; acc += r.x + r.y + r.z + r.w; },
        [&](size_t i) { size_t k = i & 63; const float *a = qra[k], *b = qrb[k];
            acc += a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1]
                 + a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0]
                 + a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3]
                 + a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]; });
    run("Quat", "slerp", ITERS,
        [&](size_t i) { float t = float(i & 63) * (1.0f / 64.0f);
                        Quat r = quat_slerp(qa[i & 63], qb[i & 63], t);
                        acc += r.x + r.y + r.z + r.w; },
        [&](size_t i) { size_t k = i & 63;
            raw_slerp(qra[k], qrb[k], float(k) * (1.0f / 64.0f), qs_out[k]);
            acc += qs_out[k][0] + qs_out[k][1] + qs_out[k][2] + qs_out[k][3]; });

    // AABB ---------------------------------------------------------------------
    run("AABB", "intersects", ITERS,
        [&](size_t i) { acc += ba[i & 63].intersects(bb[i & 63]) ? 1.0f : 0.0f; },
        [&](size_t i) { size_t k = i & 63;
            bool h = amax[k][0] >= bmin[k][0] && amin[k][0] <= bmax[k][0]
                  && amax[k][1] >= bmin[k][1] && amin[k][1] <= bmax[k][1]
                  && amax[k][2] >= bmin[k][2] && amin[k][2] <= bmax[k][2];
            acc += h ? 1.0f : 0.0f; });
    run("AABB", "contains", ITERS,
        [&](size_t i) { acc += ba[i & 63].contains(vp[i & 63]) ? 1.0f : 0.0f; },
        [&](size_t i) { size_t k = i & 63;
            bool h = rp[k][0] >= amin[k][0] && rp[k][0] <= amax[k][0]
                  && rp[k][1] >= amin[k][1] && rp[k][1] <= amax[k][1]
                  && rp[k][2] >= amin[k][2] && rp[k][2] <= amax[k][2];
            acc += h ? 1.0f : 0.0f; });

    g_sink += acc;   // fold everything so nothing can be optimized away

    // Report -------------------------------------------------------------------
    std::printf("=========================================================================\n");
    std::printf(" LITT ENGINE :: MATH LIBRARY BENCHMARKS\n");
    std::printf(" %zu timed iterations per test (+%zu warmup), %zu-entry operand pools\n",
                ITERS, ITERS / 10, N);
    std::printf("=========================================================================\n");
    std::printf(" %-6s %-11s %12s %12s %11s\n", "GROUP", "OPERATION", "LIB ns/op", "RAW ns/op", "OVERHEAD");
    std::printf("-------------------------------------------------------------------------\n");
    const char* last_group = "";
    for (int r = 0; r < g_nrows; ++r) {
        const Row& row = g_rows[r];
        bool same = std::strcmp(row.group, last_group) == 0;
        last_group = row.group;
        std::printf(" %-6s %-11s %12.3f %12.3f %10.2fx\n",
                    same ? "" : row.group, row.name, row.ns_lib, row.ns_raw,
                    row.ns_lib / row.ns_raw);
    }
    std::printf("-------------------------------------------------------------------------\n");
    std::printf(" OVERHEAD = litt time / equivalent raw-float time (1.00x = parity) | sink: %.3f\n",
                (double)g_sink);
    return 0;
}
