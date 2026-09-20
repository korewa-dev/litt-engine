#include "litt_engine2.h"
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

int main() {
    std::printf("[Legacy JSON runtime]\n");

    const char* json = R"({
        "name": "Test World",
        "objects": [
            {"type":"cube", "name":"ground", "pos":[0,0,0], "scale":[10,0.5,10], "color":"green"},
            {"type":"sphere", "name":"ball", "pos":[0,3,0], "scale":[1,1,1], "color":[300,-5,128]}
        ]
    })";

    litt2::World w = litt2::buildWorld(json);
    check(w.name == "Test World", "legacy_world_name");
    check(w.objects.size() == 2, "legacy_object_count");
    check(w.objects[0].type == "cube" && w.objects[0].color.g == 255,
          "legacy_named_color");
    check(w.objects[1].color.r == 255 &&
          w.objects[1].color.g == 0 &&
          w.objects[1].color.b == 128,
          "legacy_color_clamped");

    check(litt2::Json::parse("{\"x\":1}").type == litt2::Json::OBJ,
          "legacy_valid_json");
    check(litt2::Json::parse("{\"x\":1} trailing").type == litt2::Json::NUL,
          "legacy_trailing_garbage_rejected");
    check(litt2::Json::parse("{\"x\":01}").type == litt2::Json::NUL,
          "legacy_leading_zero_rejected");
    check(litt2::Json::parse("{\"x\":\"unterminated}").type == litt2::Json::NUL,
          "legacy_unterminated_string_rejected");
    check(litt2::Json::parse("{\"x\":1e}").type == litt2::Json::NUL,
          "legacy_bad_exponent_rejected");
    check(litt2::Json::parse("{\"x\":true").type == litt2::Json::NUL,
          "legacy_unclosed_object_rejected");
    check(litt2::buildWorld("not-json").objects.empty(),
          "legacy_bad_world_is_empty");

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
