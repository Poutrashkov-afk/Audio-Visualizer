CC = gcc
PKGCONFIG = pkg-config

CFLAGS = -Wall -Wextra -O2 -std=c17 \
         $(shell $(PKGCONFIG) --cflags sdl2 SDL2_ttf portaudio-2.0 fftw3f)

LDFLAGS = $(shell $(PKGCONFIG) --libs sdl2 SDL2_ttf portaudio-2.0 fftw3f) -lm

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = $(BUILD_DIR)/visualizer.exe

.PHONY: all clean run

all: $(TARGET)
	@echo ""
	@echo "====================================="
	@echo "  Build successful!"
	@echo "  Run: ./build/visualizer.exe"
	@echo "====================================="

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: all
	./$(TARGET)