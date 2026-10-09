# Labirintos
#   make          compila build/labirinto
#   make test     testes de unidade e de integração (inclui validar os PNGs)
#   make clean    apaga build/

ifeq ($(origin CXX),default)
CXX = clang++
endif
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=

BUILD := build
LIB_SRC := src/maze.cpp src/generators.cpp src/solvers.cpp src/render.cpp src/png.cpp src/raster.cpp src/stats.cpp
TEST_SRC := tests/main.cpp tests/test_geradores.cpp tests/test_resolvedores.cpp tests/test_png.cpp tests/test_desenho.cpp
LIB_OBJ := $(LIB_SRC:src/%.cpp=$(BUILD)/obj/%.o)
TEST_OBJ := $(TEST_SRC:tests/%.cpp=$(BUILD)/obj/tests/%.o)

all: $(BUILD)/labirinto

$(BUILD)/labirinto: $(LIB_OBJ) $(BUILD)/obj/main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD)/testes: $(LIB_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD)/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/obj/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -MMD -MP -c $< -o $@

test: $(BUILD)/labirinto $(BUILD)/testes
	./$(BUILD)/testes
	sh tests/integracao.sh ./$(BUILD)/labirinto

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/obj/*.d $(BUILD)/obj/tests/*.d)

.PHONY: all test clean
