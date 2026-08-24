# Litt native core - POSIX build (Linux/macOS/mingw).
# `make` builds littcli; `make test` runs unit tests.
CC ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -I.
CORE = littcore/litt_json.c littcore/litt_obj.c littcore/litt_world.c

all: bin/littcli

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

test: bin/litt_json.o bin/litt_obj.o bin/litt_world.o tests.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o bin/littcore_tests
	cd bin && ./littcore_tests

clean:
	rm -rf bin

.PHONY: all test clean
