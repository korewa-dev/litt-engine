# Litt Engine — Game Build Makefile
# Use these targets to build games the correct way.

.PHONY: game validate clean test stabilization-test scene-persistence-test engine-contract-test cpp-game

GAME ?= mygame

game:
	python alt/tools/template/tools/worldgen/make_game.py --about "$(DESC)" --out-dir alt/Project/$(GAME)

validate:
	python alt/tools/template/tools/assets/verify_project.py $(GAME)

clean:
	rm -rf alt/Project/$(GAME)

test:
	g++ -std=c++17 -I alt/src/native/littcore -o tests.exe alt/src/native/littcore/litt_engine_tests.cpp alt/src/native/littcore/litt_math.cpp alt/src/native/littcore/litt_json.c -lgdi32 -luser32 -lwinmm

stabilization-test:
	gcc -std=c11 -I alt/src/native/littcore -c alt/src/native/littcore/litt_json.c -o /tmp/litt_json_stabilization.o
	g++ -std=c++17 -I alt/src/native/littcore -o stabilization_tests.exe alt/src/native/littcore/litt_stabilization_runner.cpp /tmp/litt_json_stabilization.o

scene-persistence-test:
	gcc -std=c11 -I alt/src/native/littcore -c alt/src/native/littcore/litt_json.c -o /tmp/litt_json_scene.o
	g++ -std=c++17 -I alt/src/native/littcore -o scene_persistence_tests.exe alt/src/native/littcore/litt_scene_persistence_tests.cpp /tmp/litt_json_scene.o

engine-contract-test:
	$(CC) -std=c11 -O2 -Wall -Wextra -I alt/src/native/littcore -c alt/src/native/littcore/litt_json.c -o /tmp/litt_json_engine_contract.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I alt/src/native/littcore alt/src/native/littcore/litt_engine_tests.cpp alt/src/native/littcore/litt_profiler.cpp /tmp/litt_json_engine_contract.o -o engine_contract_tests.exe
	./engine_contract_tests.exe

cpp-game:
	g++ -std=c++17 -I alt/src/native/littcore -o $(GAME).exe alt/Project/$(GAME)/engine/game.cpp alt/src/native/littcore/litt_obj.c alt/src/native/littcore/litt_json.c alt/src/native/littcore/litt_world.c -lgdi32 -luser32 -lwinmm
