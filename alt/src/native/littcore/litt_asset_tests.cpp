#include "litt_asset.h"
#include "litt_obj.h"
#include <cassert>
#include <cstdio>
#include <fstream>

using namespace litt;

static void write_file(const char* path, const void* data, size_t size) {
    FILE* f = std::fopen(path, "wb");
    assert(f);
    assert(std::fwrite(data, 1, size, f) == size);
    std::fclose(f);
}

int main() {
    AssetManager assets;

    // Failed loads stay failed and are never cached as successful assets.
    assert(!assets.loadModel("/tmp/litt_missing.obj"));
    assert(assets.models.empty());
    assert(!assets.loadTexture("/tmp/litt_missing.tga"));
    assert(assets.textures.empty());
    assert(!assets.loadModel("x"));
    assert(!assets.loadTexture("x"));
    assert(!assets.loadShader("missing.vert", "missing.frag"));
    assert(assets.shaders.empty());

    // OBJ: bounded fan triangulation and relative position indices.
    const char obj[] =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "f -4 -3 -2 -1\n";
    write_file("/tmp/litt_asset_quad.obj", obj, sizeof(obj) - 1);
    auto model = assets.loadModel("/tmp/litt_asset_quad.obj");
    assert(model && model->positions.size() == 4 && model->indices.size() == 6);
    assert(model->indices[0] == 0 && model->indices[1] == 1 && model->indices[2] == 2);
    assert(model->indices[3] == 0 && model->indices[4] == 2 && model->indices[5] == 3);

    // TGA: supported subset is uncompressed 24/32-bit true-color. This 2x2
    // bottom-origin BGR fixture must emerge top-origin RGB.
    const unsigned char tga[] = {
        0,0,2, 0,0,0,0,0, 0,0,0,0, 2,0,2,0,24,0,
        // bottom row: blue, white
        255,0,0, 255,255,255,
        // top row: red, green
        0,0,255, 0,255,0
    };
    write_file("/tmp/litt_asset_rgb.tga", tga, sizeof(tga));
    auto tex = assets.loadTexture("/tmp/litt_asset_rgb.tga");
    assert(tex && tex->width == 2 && tex->height == 2 && tex->channels == 3);
    assert(tex->data.size() == 12);
    assert(tex->data[0] == 255 && tex->data[1] == 0 && tex->data[2] == 0);
    assert(tex->data[3] == 0 && tex->data[4] == 255 && tex->data[5] == 0);

    const unsigned char truncated[] = {0,0,2};
    write_file("/tmp/litt_asset_bad.tga", truncated, sizeof(truncated));
    assert(!assets.loadTexture("/tmp/litt_asset_bad.tga"));
    assert(assets.textures.find("/tmp/litt_asset_bad.tga") == assets.textures.end());

    LvModel nativeModel = {nullptr, 0};
    assert(lv_obj_load("/tmp/litt_asset_quad.obj", &nativeModel) == 0);
    assert(nativeModel.count > 0);
    lv_model_free(&nativeModel);

    const char badObj[] = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99\n";
    write_file("/tmp/litt_asset_bad.obj", badObj, sizeof(badObj) - 1);
    assert(lv_obj_load("/tmp/litt_asset_bad.obj", &nativeModel) != 0);

    std::remove("/tmp/litt_asset_quad.obj");
    std::remove("/tmp/litt_asset_rgb.tga");
    std::remove("/tmp/litt_asset_bad.tga");
    std::remove("/tmp/litt_asset_bad.obj");
    return 0;
}
