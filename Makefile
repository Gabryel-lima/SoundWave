# Atalho para os comandos de CMake mais usados.
#
# O sistema de build de verdade e o CMake -- este Makefile existe so para manter
# o habito de `make` do SoundWave original funcionando. Qualquer coisa fora do
# basico: use cmake diretamente.

BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build test run clean install-deps help

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build
	./$(BUILD_DIR)/soundwave_tests

# Demonstracao completa sem precisar de nenhum arquivo de audio.
run: build
	./$(BUILD_DIR)/soundwave info
	./$(BUILD_DIR)/soundwave map 440
	./$(BUILD_DIR)/soundwave compare "gen:sweep:0:3" --out out/compare

clean:
	rm -rf $(BUILD_DIR) out

# Todas sao OPCIONAIS: o nucleo e os testes compilam sem nenhuma delas.
#   zlib  -> PNG comprimido    SDL2 -> comando `live`    FFTW3 -> backend de FFT
install-deps:
	sudo apt-get update
	sudo apt-get install -y cmake g++ pkg-config zlib1g-dev libsdl2-dev libfftw3-dev

help:
	@echo "make build          compila (padrao)"
	@echo "make test           roda a suite de testes"
	@echo "make run            demonstracao sem arquivos de audio"
	@echo "make clean          remove build/ e out/"
	@echo "make install-deps   dependencias OPCIONAIS (Debian/Ubuntu)"
	@echo ""
	@echo "BUILD_TYPE=Debug make build    para compilar com simbolos"
