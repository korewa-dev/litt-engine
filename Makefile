# Litt Engine — Game Build Makefile
# Use these targets to build games the correct way.

.PHONY: game validate clean test stabilization-test cpp-game

# Default game name (override with: make game GAME=mygame)
GAME ?= mygame

# One-command full game
game:
	python alt/tools/template/tools/worldgen/make_game.py --about "$(DESC)" --out-dir alt/Project/$(GAME)

# Validate existing game
validate:
	python alt/tools/template/tools/assets/verify_project.py $(GAME)

# Clean game build
clean:
	rm -rf alt/Project/$(GAME)

# Build C++ engine test
test:
	g++ -std=c++17 -I alt/src/native/littcore -o tests.exe alt/src/native/littcore/litt_engine_tests.cpp alt/src/native/littcore/litt_math.cpp -lgdi32 -luser32 -lwinmm

# Build focused stabilization regression tests (platform-independent core)
stabilization-test:
	g++ -std=c++17 -I alt/src/native/littcore -o stabilization_tests.exe alt/src/native/littcore/litt_stabilization_tests.cpp

# Build game with C++ engine
cpp-game:
	g++ -std=c++17 -I alt/src/native/littcore -o $(GAME).exe alt/Project/$(GAME)/engine/game.cpp alt/src/native/littcore/litt_obj.c alt/src/native/littcore/litt_json.c alt/src/native/littcore/litt_world.c -lgdi32 -luser32 -lwinmm
