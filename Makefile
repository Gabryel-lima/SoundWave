# Makefile para SoundWave - Visualizador de Áudio

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = 

# Diretórios
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Bibliotecas
LIBS = -lavformat -lavcodec -lavutil -lswresample -lfftw3 -lm -lSDL2

# Flags para FFmpeg
CFLAGS += $(shell pkg-config --cflags libavformat libavcodec libavutil libswresample 2>/dev/null)
CFLAGS += $(shell pkg-config --cflags sdl2 2>/dev/null)
CFLAGS += $(shell pkg-config --cflags fftw3 2>/dev/null)

# Linking para FFmpeg e SDL2
LDFLAGS += $(shell pkg-config --libs libavformat libavcodec libavutil libswresample 2>/dev/null)
LDFLAGS += $(shell pkg-config --libs sdl2 2>/dev/null)
LDFLAGS += $(shell pkg-config --libs fftw3 2>/dev/null)

# Fallback se pkg-config não funcionar
ifeq ($(LDFLAGS),)
LDFLAGS = -lavformat -lavcodec -lavutil -lswresample -lfftw3 -lm -lSDL2
endif

# Arquivos fonte
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/soundwave

# Regra padrão
all: $(TARGET)

# Criar diretórios necessários
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compilar objeto
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Linkar executável
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)

# Limpar
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Instalar dependências (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y \
		libavformat-dev \
		libavcodec-dev \
		libavutil-dev \
		libswresample-dev \
		libfftw3-dev \
		libsdl2-dev \
		pkg-config \
		gcc \
		make

# Executar
run: $(TARGET)
	$(TARGET) "Feelings V4.mp3"

.PHONY: all clean install-deps run

