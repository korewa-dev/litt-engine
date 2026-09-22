# Litt Engine — Game Build Makefile
# Use these targets to build games the correct way.

.PHONY: game validate clean test stabilization-test scene-persistence-test engine-contract-test editor-contract-test ffi-contract-test dither-contract-test gameplay-feature-test release-hardening-test release-package-test cpp-game

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

editor-contract-test:
	$(CC) -std=c11 -O2 -Wall -Wextra -I alt/src/native/littcore -c alt/src/native/littcore/litt_json.c -o /tmp/litt_json_editor_contract.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I alt/src/native -I alt/src/native/littcore alt/src/native/litteditor_contract_tests.cpp /tmp/litt_json_editor_contract.o -o editor_contract_tests.exe
	./editor_contract_tests.exe

ffi-contract-test:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I alt/include alt/src/native/litt_ffi.cpp alt/src/litt_ffi_contract_tests.cpp -o ffi_contract_tests.exe
	./ffi_contract_tests.exe

dither-contract-test:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I alt/src/native/littcore alt/src/native/littcore/litt_dither.cpp alt/src/native/littcore/litt_dither_renderer.cpp alt/src/native/littcore/litt_dither_contract_tests.cpp -o dither_contract_tests.exe
	./dither_contract_tests.exe

gameplay-feature-test:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -I alt/src/native/littcore \
		alt/src/native/littcore/litt_gameplay_feature_tests.cpp \
		alt/src/native/littcore/litt_particle_system.cpp \
		alt/src/native/littcore/litt_culling.cpp \
		alt/src/native/littcore/litt_terrain_system.cpp \
		alt/src/native/littcore/litt_texture.cpp \
		alt/src/native/littcore/litt_render_pass.cpp \
		alt/src/native/littcore/litt_shader_system.cpp \
		alt/src/native/littcore/litt_lighting.cpp \
		alt/src/native/littcore/litt_gpu.cpp \
		-o gameplay_feature_tests.exe
	./gameplay_feature_tests.exe

release-hardening-test:
	$(CC) -std=c11 -O2 -Wall -Wextra -I alt/src/native/littcore -c alt/src/native/littcore/litt_json.c -o /tmp/litt_json_release_hardening.o
	$(CXX) -std=c++17 -O2 -Wall -Wextra -pthread -I alt/src/native/littcore \
		alt/src/native/littcore/litt_release_hardening_tests.cpp \
		/tmp/litt_json_release_hardening.o -o release_hardening_tests.exe
	./release_hardening_tests.exe

release-package-test:
	$(MAKE) -C alt/src/native cpp-sdk
	rm -f /tmp/litt-sdk-a.tar.gz /tmp/litt-sdk-b.tar.gz
	python3 alt/tools/release_package.py --root . --out /tmp/litt-sdk-a.tar.gz
	python3 alt/tools/release_package.py --root . --out /tmp/litt-sdk-b.tar.gz
	cmp /tmp/litt-sdk-a.tar.gz /tmp/litt-sdk-b.tar.gz

cpp-game:
	g++ -std=c++17 -I alt/src/native/littcore -o $(GAME).exe alt/Project/$(GAME)/engine/game.cpp alt/src/native/littcore/litt_obj.c alt/src/native/littcore/litt_json.c alt/src/native/littcore/litt_world.c -lgdi32 -luser32 -lwinmm
