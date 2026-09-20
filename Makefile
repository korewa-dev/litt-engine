# Litt Engine developer entry points.
# The supported runtime is headless-first; native release contracts live under
# alt/src/native and are shared with CI.

PYTHON ?= python3
CXX ?= c++
GAME ?= mygame
DESC ?=

NATIVE_DIR := alt/src/native
CORE_DIR := $(NATIVE_DIR)/littcore
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS ?= -I $(CORE_DIR)

ifeq ($(OS),Windows_NT)
PLATFORM_LIBS := -lgdi32 -luser32 -lwinmm
else
PLATFORM_LIBS :=
endif

.PHONY: game validate clean clean-game clean-native test stabilization-test \
        native-release engine-system-test cpp-game release-gate

game:
	$(PYTHON) alt/tools/template/tools/worldgen/make_game.py --about "$(DESC)" --out-dir alt/Project/$(GAME)

validate:
	$(PYTHON) alt/tools/template/tools/assets/verify_project.py alt/Project/$(GAME)

clean: clean-native

clean-game:
	rm -rf alt/Project/$(GAME)

clean-native:
	$(MAKE) -C $(NATIVE_DIR) clean
	rm -f stabilization_tests.exe engine_system_tests.exe

stabilization-test:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o stabilization_tests.exe $(CORE_DIR)/litt_stabilization_tests.cpp $(PLATFORM_LIBS)
	./stabilization_tests.exe

engine-system-test:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o engine_system_tests.exe \
		$(CORE_DIR)/litt_engine_tests.cpp $(CORE_DIR)/litt_math.cpp $(PLATFORM_LIBS)
	./engine_system_tests.exe

native-release:
	$(MAKE) -C $(NATIVE_DIR) release-test

test: stabilization-test native-release

release-gate: test engine-system-test
	$(PYTHON) alt/tools/template/tools/worldgen/test_worldkit.py
	$(PYTHON) alt/tools/template/tools/worldgen/test_gen_props.py
	$(NATIVE_DIR)/bin/littcli validate alt/Project/example-village --frames 60

cpp-game:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(GAME).exe \
		alt/Project/$(GAME)/engine/game.cpp \
		$(CORE_DIR)/litt_obj.c $(CORE_DIR)/litt_json.c $(CORE_DIR)/litt_world.c \
		$(PLATFORM_LIBS)
