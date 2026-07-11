APP := raceslate
BUILD := build/host
CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -Isrc -Ivendor -O0 -g
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf libcurl)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf libcurl)
CORE_SRCS := src/rs_season.c src/rs_standings.c src/rs_app.c vendor/cJSON.c

.PHONY: all host test clean

all: host

host: $(BUILD)/$(APP)

$(BUILD)/$(APP): src/main.c src/rs_http.c src/rs_store.c $(CORE_SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $^ $(SDL_LIBS) -lm

test: $(BUILD)/test_core
	$(BUILD)/test_core tests/fixtures

$(BUILD)/test_core: tests/test_core.c $(CORE_SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -rf build dist
