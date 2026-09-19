// Litt Engine generated-game integration regression tests.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "litt_game.h"

using namespace litt;
namespace fs = std::filesystem;

static int failures = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::printf("  ok %s\n", name);
    } else {
        std::printf("  FAIL %s\n", name);
        ++failures;
    }
}

static bool write_text(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Game Contract]\n");

    const fs::path root = fs::current_path() / "litt_game_contract_tmp";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "assets" / "scenes", ec);
    fs::create_directories(root / "assets" / "models", ec);
    check(!ec, "fixture_directories_created");

    const std::string scene =
        "{\n"
        "  \"nodes\": [\n"
        "    {\"name\":\"Root\",\"position\":[0,0,0],\"tags\":[]},\n"
        "    {\"name\":\"Player_Start\",\"position\":[1,2,3],"
        "\"rotation\":[0,0,0,1],\"scale\":[1,1,1],"
        "\"tags\":[\"player\",\"start\"]},\n"
        "    {\"name\":\"Floor\",\"position\":[0,0,0],"
        "\"rotation\":[0,0,0,1],\"scale\":[1,1,1],"
        "\"tags\":[\"floor\",\"model:test_floor\"]}\n"
        "  ]\n"
        "}\n";

    const std::string state =
        "{\"gameplay\":{\"physics\":{"
        "\"gravity\":22,\"jump_velocity\":8,\"run_speed\":7}}}\n";

    check(write_text(root / "assets" / "scenes" / "world.lscn.json", scene),
          "fixture_scene_written");
    check(write_text(root / "world_state.json", state),
          "fixture_state_written");

    {
        Game game;
        check(!game.initialize(root.string()),
              "game_rejects_missing_required_model");
    }

    const std::string obj =
        "g floor\n"
        "v -2 0 -2\n"
        "v 2 0 -2\n"
        "v 2 0 2\n"
        "v -2 0 2\n"
        "f 1 2 3\n"
        "f 1 3 4\n";
    check(write_text(root / "assets" / "models" / "test_floor.obj", obj),
          "fixture_model_written");

    {
        Game game;
        check(game.initialize(root.string()),
              "game_accepts_valid_required_model");
    }

    fs::remove_all(root, ec);

    std::printf("\nResults: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
