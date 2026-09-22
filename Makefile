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
	rm -rf /tmp/litt-sdk-extract /tmp/litt-sdk-a.tar.gz /tmp/litt-sdk-b.tar.gz /tmp/litt-package-consumer.cpp /tmp/litt-package-consumer
	python3 alt/tools/release_package.py --root . --out /tmp/litt-sdk-a.tar.gz
	python3 alt/tools/release_package.py --root . --out /tmp/litt-sdk-b.tar.gz
	cmp /tmp/litt-sdk-a.tar.gz /tmp/litt-sdk-b.tar.gz
	mkdir -p /tmp/litt-sdk-extract
	tar -xzf /tmp/litt-sdk-a.tar.gz -C /tmp/litt-sdk-extract
	python3 -c 'import json,subprocess; m=json.load(open("/tmp/litt-sdk-extract/litt-sdk/manifest.json")); assert m["source_sha"] == subprocess.check_output(["git","rev-parse","HEAD"], text=True).strip(); assert m["package_version"] == 1'
	printf '%s\n' '#include <litt/litt_profiler.h>' 'int main() { auto& p = litt::Profiler::get_instance(); p.reset(); p.begin_sample("package"); p.end_sample("package"); return p.get_stats("package").sample_count == 1u ? 0 : 1; }' > /tmp/litt-package-consumer.cpp
	cd /tmp && $(CXX) -std=c++17 -I /tmp/litt-sdk-extract/litt-sdk/include /tmp/litt-package-consumer.cpp /tmp/litt-sdk-extract/litt-sdk/lib/liblittcore.a -lm -pthread -o /tmp/litt-package-consumer
	cd /tmp && /tmp/litt-package-consumer

cpp-game:
	g++ -std=c++17 -I alt/src/native/littcore -o $(GAME).exe alt/Project/$(GAME)/engine/game.cpp alt/src/native/littcore/litt_obj.c alt/src/native/littcore/litt_json.c alt/src/native/littcore/litt_world.c -lgdi32 -luser32 -lwinmm
