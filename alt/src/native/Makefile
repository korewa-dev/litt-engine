# Litt native core - POSIX build (Linux/macOS/mingw).
# `make` builds littcli + littview; `make test` runs unit tests.
CXX ?= c++
CC  ?= cc
AR  ?= ar
CFLAGS = -std=c11 -O2 -Wall -Wextra -I.
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I.
LDLIBS ?= -lm
THREAD_LIBS ?= -pthread
CORE = littcore/litt_json.c littcore/litt_obj.c littcore/litt_world.c
DITHER = littcore/litt_dither.cpp littcore/litt_dither_renderer.cpp

CPP_CORE = \
	littcore/litt_profiler.cpp \
	littcore/litt_particle_system.cpp \
	littcore/litt_culling.cpp \
	littcore/litt_terrain_system.cpp \
	littcore/litt_texture.cpp \
	littcore/litt_render_pass.cpp \
	littcore/litt_shader_system.cpp \
	littcore/litt_lighting.cpp \
	littcore/litt_asset_pipeline.cpp \
	littcore/litt_dither.cpp \
	littcore/litt_dither_renderer.cpp \
	littcore/litt_gpu.cpp
CPP_CORE_OBJS = $(patsubst littcore/%.cpp,bin/core_%.o,$(CPP_CORE))

CPP_PUBLIC_HEADERS = \
	litt.h litt_engine_systems.h litt_game.h litt_math.h litt_ecs.h litt_event.h \
	litt_memory.h litt_memory_pool.h litt_scene.h litt_engine.h litt_renderer.h \
	litt_gpu.h litt_gpu_software.h litt_lighting.h litt_material.h litt_texture.h \
	litt_render_pass.h litt_asset.h litt_asset_pipeline.h litt_physics.h litt_audio.h \
	litt_input.h litt_ui.h litt_profiler.h litt_lod.h litt_culling.h \
	litt_particle_system.h litt_terrain_system.h litt_shader_system.h litt_dither.h \
	litt_serialization.h litt_scripting.h litt_scripting_vm.h litt_networking.h \
	litt_config.h litt_world.h litt_json.h litt_obj.h


all: bin/littcli bin/littview

# GUI/integration SDK: stable C bridge as a shared library plus public header.
# DESTDIR may be set by packagers; PREFIX defaults to /usr/local.
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/litt


bin/core_%.o: littcore/%.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin/liblittcore.a: bin/litt_json.o bin/litt_obj.o bin/litt_world.o $(CPP_CORE_OBJS)
	$(AR) rcs $@ $^
	@echo "[build] bin/liblittcore.a OK"

cpp-sdk: bin/liblittcore.a
	@mkdir -p dist/include/litt dist/lib
	cp $(addprefix littcore/,$(CPP_PUBLIC_HEADERS)) dist/include/litt/
	cp bin/liblittcore.a dist/lib/liblittcore.a
	@echo "[build] dist Litt C++ SDK OK"

cpp-sdk-test: cpp-sdk
	$(CXX) $(CXXFLAGS) -DLITT_INSTALLED_SDK=1 -I dist/include \
		littcore/litt_gameplay_feature_tests.cpp dist/lib/liblittcore.a \
		-o bin/cpp_sdk_contract $(LDFLAGS) $(LDLIBS) $(THREAD_LIBS)
	cd bin && ./cpp_sdk_contract

bin/litt_json.o: littcore/litt_json.c littcore/litt_json.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

bin/litt_obj.o: littcore/litt_obj.c littcore/litt_obj.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

bin/litt_world.o: littcore/litt_world.c littcore/litt_world.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

bin/littcli: bin/litt_json.o bin/litt_obj.o bin/litt_world.o littcli.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "[build] bin/littcli OK"

bin/littview_main.o: littview.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -c $< -o $@

bin/littview: bin/litt_json.o bin/litt_obj.o bin/litt_world.o bin/littview_main.o
	$(CXX) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "[build] bin/littview OK"

bin/litt_json_pic.o: littcore/litt_json.c littcore/litt_json.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

bin/litt_obj_pic.o: littcore/litt_obj.c littcore/litt_obj.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

bin/litt_world_pic.o: littcore/litt_world.c littcore/litt_world.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

bin/litt_c_pic.o: litt_c.cpp litt_c.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

bin/liblitt.so: bin/litt_json_pic.o bin/litt_obj_pic.o bin/litt_world_pic.o bin/litt_c_pic.o
	$(CXX) -shared $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "[build] bin/liblitt.so OK"

sdk: bin/liblitt.so
	@mkdir -p dist/include/litt dist/lib
	cp litt_c.h dist/include/litt/litt_c.h
	cp bin/liblitt.so dist/lib/liblitt.so
	@echo "[build] dist Litt C SDK OK"

install-sdk: sdk
	install -d "$(DESTDIR)$(INCLUDEDIR)" "$(DESTDIR)$(LIBDIR)"
	install -m 0644 litt_c.h "$(DESTDIR)$(INCLUDEDIR)/litt_c.h"
	install -m 0755 bin/liblitt.so "$(DESTDIR)$(LIBDIR)/liblitt.so"

test: bin/litt_json.o bin/litt_obj.o bin/litt_world.o tests.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o bin/littcore_tests $(LDFLAGS) $(LDLIBS)
	cd bin && ./littcore_tests

selftest: bin/littview
	cd bin && ./littview selftest

network-test: littcore/networking_tests.cpp littcore/litt_networking.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) littcore/networking_tests.cpp -o bin/networking_tests $(LDFLAGS) $(LDLIBS) -pthread
	cd bin && ./networking_tests

asset-test: littcore/litt_asset_pipeline.cpp littcore/asset_pipeline_tests.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o bin/asset_pipeline_tests $(LDFLAGS) $(LDLIBS)
	cd bin && ./asset_pipeline_tests

gpu-contract-test: littcore/litt_gpu.cpp littcore/litt_gpu_contract_tests.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o bin/litt_gpu_contract_tests $(LDFLAGS) $(LDLIBS)
	cd bin && ./litt_gpu_contract_tests

# Dither3D software demo
bin/dither3d_demo: dither3d_demo.cpp littcore/litt_dither.cpp littcore/litt_dither_renderer.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "[build] bin/dither3d_demo OK"

shader-artifact-test: littcore/litt_shader_compilation.cpp littcore/litt_shader_artifact_tests.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o bin/shader_artifact_tests $(LDFLAGS) $(LDLIBS)
	cd bin && ./shader_artifact_tests

dither3d_demo: bin/dither3d_demo

clean:
	rm -rf bin dist

.PHONY: all sdk cpp-sdk cpp-sdk-test install-sdk test selftest network-test asset-test gpu-contract-test shader-artifact-test clean dither3d_demo
