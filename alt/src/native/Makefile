# Litt native core - POSIX build (Linux/macOS/mingw).
# `make` builds littcli + littview; `make test` runs unit tests.
CXX ?= c++
CC  ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -I.
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I.
LDLIBS ?= -lm
CORE = littcore/litt_json.c littcore/litt_obj.c littcore/litt_world.c
DITHER = littcore/litt_dither.cpp littcore/litt_dither_renderer.cpp

all: bin/littcli bin/littview

# GUI/integration SDK: stable C bridge as a shared library plus public header.
# DESTDIR may be set by packagers; PREFIX defaults to /usr/local.
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include/litt

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

# Dither3D demo
bin/dither3d_demo: bin/litt_json.o bin/litt_obj.o bin/litt_world.o litt_dither.o litt_dither_renderer.o dither3d_demo.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[build] bin/dither3d_demo OK"

dither3d_demo: bin/dither3d_demo

clean:
	rm -rf bin dist

.PHONY: all sdk install-sdk test selftest network-test asset-test gpu-contract-test clean dither3d_demo
