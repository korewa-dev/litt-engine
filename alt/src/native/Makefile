# Litt native core - POSIX build (Linux/macOS/mingw).
# `make` builds littcli + littview; `make test` runs unit tests.
CXX ?= c++
CC  ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -I.
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I.
CORE = littcore/litt_json.c littcore/litt_obj.c littcore/litt_world.c
DITHER = littcore/litt_dither.cpp littcore/litt_dither_renderer.cpp

all: bin/littcli bin/littview

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
	$(CC) $(CFLAGS) $^ -o $@
	@echo "[build] bin/littcli OK"

bin/littview: bin/litt_json.o bin/litt_obj.o bin/litt_world.o littview.cpp
	$(CXX) $(CXXFLAGS) -c littview.cpp -o bin/littview_main.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	@echo "[build] bin/littview OK"

test: bin/litt_json.o bin/litt_obj.o bin/litt_world.o tests.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o bin/littcore_tests
	cd bin && ./littcore_tests

selftest: bin/littview
	cd bin && ./littview selftest

# Dither3D demo
bin/dither3d_demo: bin/litt_json.o bin/litt_obj.o bin/litt_world.o litt_dither.o litt_dither_renderer.o dither3d_demo.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[build] bin/dither3d_demo OK"

dither3d_demo: bin/dither3d_demo

clean:
	rm -rf bin

.PHONY: all test selftest clean dither3d_demo
