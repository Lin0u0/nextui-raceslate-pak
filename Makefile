APP := raceslate
BUILD := build/host
CC ?= cc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -Isrc -Ivendor -O0 -g
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf libcurl)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf libcurl)
CORE_SRCS := src/rs_season.c src/rs_standings.c src/rs_results.c src/rs_weather.c src/rs_reference.c src/rs_circuit_atlas.c src/rs_profiles.c src/rs_timezone.c src/rs_settings.c src/rs_app.c vendor/cJSON.c
APP_SRCS := src/main.c src/rs_http.c src/rs_store.c $(CORE_SRCS)
TG5040_TOOLCHAIN_ROOT ?= build/tg5040-toolchain
TG5040_TOOLCHAIN := $(TG5040_TOOLCHAIN_ROOT)/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
TG5040_CC ?= $(TG5040_TOOLCHAIN)/bin/aarch64-none-linux-gnu-gcc
TG5040_SDK_ROOT ?= build/tg5040-sdk
TG5040_SDK_USR := $(TG5040_SDK_ROOT)/sdk_usr/usr
TG5040_CURL_PREFIX ?= third_party/tg5040/curl
TG5040_BIN := build/tg5040/raceslate
VERSION := $(shell tr -d '\r\n' < VERSION)

.PHONY: all host test tg5040 tg5040-toolchain tg5040-sdk tg5040-libcurl tg5040-bootstrap nextui-release verify-nextui-release clean

all: host

host: $(BUILD)/$(APP)

$(BUILD)/$(APP): $(APP_SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $^ $(SDL_LIBS) -lm

test: $(BUILD)/test_core
	$(BUILD)/test_core tests/fixtures
	python3 tests/verify_offline_seasons.py assets/baseline/seasons assets/reference/profiles.tsv

tg5040-toolchain:
	./scripts/setup_tg5040_toolchain.sh "$(TG5040_TOOLCHAIN_ROOT)"

tg5040-sdk:
	./scripts/bootstrap_tg5040_sdk.sh "$(TG5040_SDK_ROOT)"

tg5040-libcurl:
	CC="$(TG5040_CC)" CROSS_PREFIX="$(TG5040_TOOLCHAIN)/bin/aarch64-none-linux-gnu-" TG5040_SDK_ROOT="$(TG5040_SDK_ROOT)" ./scripts/build_tg5040_libcurl.sh third_party/tg5040

tg5040-bootstrap: tg5040-toolchain tg5040-sdk tg5040-libcurl

tg5040: $(APP_SRCS)
	@test -x "$(TG5040_CC)" || { echo "missing compiler; run make tg5040-bootstrap" >&2; exit 1; }
	@test -f "$(TG5040_CURL_PREFIX)/lib/libcurl.a" || { echo "missing tg5040 libcurl" >&2; exit 1; }
	@mkdir -p build/tg5040
	$(TG5040_CC) -std=c11 -Wall -Wextra -Werror -Isrc -Ivendor -I$(TG5040_SDK_USR)/include/SDL2 -I$(TG5040_CURL_PREFIX)/include -D_REENTRANT -O2 -mcpu=cortex-a53 -o $(TG5040_BIN) $(APP_SRCS) \
	  $(TG5040_SDK_USR)/lib/libSDL2.so $(TG5040_SDK_USR)/lib/libSDL2_ttf.so $(TG5040_SDK_USR)/lib/libfreetype.so $(TG5040_SDK_USR)/lib/libbz2.so \
	  $(TG5040_CURL_PREFIX)/lib/libcurl.a $(TG5040_SDK_USR)/lib/libssl.so.1.1 $(TG5040_SDK_USR)/lib/libcrypto.so.1.1 $(TG5040_SDK_USR)/lib/libz.so -ldl -lpthread -lm

nextui-release: tg5040
	@rm -rf build/tg5040/stage/nextui dist/RaceSlate.pakz
	@mkdir -p build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/bin/tg5040 build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/lib/tg5040 build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/res build/tg5040/stage/nextui/Tools/tg5040/.media dist
	cp $(TG5040_BIN) build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/bin/tg5040/raceslate
	cp packaging/nextui/launch.sh build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/launch.sh
	sed 's/@VERSION@/$(VERSION)/g' packaging/nextui/pak.json > build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/pak.json
	cp assets/icons/raceslate.png build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/icon.png
	cp assets/icons/raceslate.png build/tg5040/stage/nextui/Tools/tg5040/.media/RaceSlate.png
	cp -R assets/fonts assets/circuits assets/baseline assets/reference build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/res/
	cp assets/cacert.pem THIRD_PARTY_NOTICES.md build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/res/
	cp -R licenses build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/res/
	@for lib in libSDL2-2.0.so.0 libSDL2_ttf-2.0.so.0 libfreetype.so.6 libbz2.so.1.0 libssl.so.1.1 libcrypto.so.1.1 libz.so.1; do src=$$(find "$(TG5040_SDK_USR)/lib" -maxdepth 1 -name "$$lib" -print -quit); test -n "$$src" && cp -L "$$src" build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/lib/tg5040/; done
	cp -L "$$($(TG5040_CC) -print-file-name=libgcc_s.so.1)" build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/lib/tg5040/libgcc_s.so.1
	chmod +x build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/launch.sh build/tg5040/stage/nextui/Tools/tg5040/RaceSlate.pak/bin/tg5040/raceslate
	cd build/tg5040/stage/nextui && zip -qr "$(abspath dist/RaceSlate.pakz)" Tools
	$(MAKE) verify-nextui-release

verify-nextui-release:
	./tests/verify_nextui_release.sh

$(BUILD)/test_core: tests/test_core.c src/rs_store.c $(CORE_SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -rf build dist
