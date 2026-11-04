DEVICE ?= Cardenza
IDF_TARGET ?= esp32s3
PORT ?=
AMY_MULTICORE ?= 0
REMOTE ?= macbook

# Browser simulator (DEVICE=CardenzaWeb): Emscripten from $(EMSDK), page in $(WEB_BUILD)/site.
EMSDK ?= $(HOME)/emsdk
EMCMAKE ?= $(or $(shell command -v emcmake 2>/dev/null),$(EMSDK)/upstream/emscripten/emcmake)
CMAKE ?= cmake
WEB_BUILD ?= build/web
WEB_PORT ?= 8000

ifeq ($(DEVICE),Cardenza)
BUILD := build/Cardenza
DEVICE_PROJECT_DIR := Devices/Cardenza

ifeq ($(OS),Windows_NT)
define RUN_IDF
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& { Set-Location '$(abspath $(DEVICE_PROJECT_DIR))'; idf.py $(1) }"
endef
else
define RUN_IDF
cd $(DEVICE_PROJECT_DIR) && idf.py $(1)
endef
endif
else ifeq ($(DEVICE),CardenzaSim)
BUILD := build/CardenzaSim
SIM_BUILD_TARGET := CardenzaSimHost
SIM_RUN_EXE := $(BUILD)/Devices/CardenzaSim/CardenzaSimHost
ifeq ($(OS),Windows_NT)
SIM_BUILD_TARGET := CardenzaSimDesktop
SIM_RUN_EXE := $(BUILD)/Devices/CardenzaSim/Debug/CardenzaSimDesktop.exe
endif
else ifeq ($(DEVICE),CardenzaWeb)
BUILD := $(WEB_BUILD)
else
$(error Unsupported DEVICE '$(DEVICE)'. Expected Cardenza, CardenzaSim or CardenzaWeb)
endif

IDF_PORT_ARG := $(if $(PORT),-p '$(PORT)',)
CARDENZA_IDF_ARGS := -B '$(abspath $(BUILD))'
ifeq ($(AMY_MULTICORE),1)
CARDENZA_IDF_ARGS += -DCARDENZA_AMY_EXPERIMENTAL_MULTICORE=ON
else
CARDENZA_IDF_ARGS += -DCARDENZA_AMY_EXPERIMENTAL_MULTICORE=OFF
endif

.PHONY: all build run set-target flash remote-flash bootloader-flash app-flash erase monitor flash-monitor clean fullclean validate-architecture firmware firmware-target host cardenza cardenza-firmware cardenza-flash cardenza-monitor cardenza-flash-monitor cardenza-host cardenza-clean cardenza-fullclean web web-serve web-deploy learn hal-check hal-update

all: build

# CardenzaHAL (Devices/Cardenza/cardenza) is the upstream submodule
# https://github.com/203-Systems/CardenzaHAL, a read-only dependency (AGENT.md): no local changes, no
# local commits, never a push. hal-check enforces it before every firmware build; hal-update pulls.
HAL_DIR := Devices/Cardenza/cardenza
HAL_NO_PUSH := no-push://CardenzaHAL-is-a-read-only-dependency

hal-check:
	@test -e $(HAL_DIR)/.git || git submodule update --init $(HAL_DIR)
	@git -C $(HAL_DIR) config remote.origin.pushurl $(HAL_NO_PUSH)
	@test -z "$$(git -C $(HAL_DIR) status --porcelain)" || { \
	  echo "hal-check: $(HAL_DIR) has local changes. CardenzaHAL is a read-only dependency (AGENT.md);" >&2; \
	  echo "hal-check: work above it in Devices/Cardenza and restore it: git -C $(HAL_DIR) checkout -- . && git -C $(HAL_DIR) clean -fd" >&2; \
	  git -C $(HAL_DIR) status --short >&2; exit 1; }
	@git -C $(HAL_DIR) merge-base --is-ancestor HEAD origin/main 2>/dev/null || \
	  { git -C $(HAL_DIR) fetch -q origin && git -C $(HAL_DIR) merge-base --is-ancestor HEAD origin/main; } || { \
	  echo "hal-check: $(HAL_DIR) is on a commit that is not on upstream main (a local commit?)." >&2; \
	  echo "hal-check: CardenzaHAL is read-only: make hal-update (or git submodule update $(HAL_DIR))." >&2; exit 1; }

# Pull the latest upstream CardenzaHAL; commit the new submodule pointer in this repository.
hal-update:
	git -C $(HAL_DIR) fetch origin
	git -C $(HAL_DIR) checkout -q --detach origin/main
	@git -C $(HAL_DIR) config remote.origin.pushurl $(HAL_NO_PUSH)
	@echo "CardenzaHAL: $$(git -C $(HAL_DIR) log --oneline -1). Commit the submodule pointer ($(HAL_DIR)) here."

build:
ifeq ($(DEVICE),Cardenza)
	@$(MAKE) --no-print-directory hal-check
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) build)
else ifeq ($(DEVICE),CardenzaWeb)
	"$(EMCMAKE)" $(CMAKE) -S . -B $(BUILD) -DDEVICE=CardenzaWeb
	$(CMAKE) --build $(BUILD) --target CardenzaWeb
	@echo "CardenzaWeb page: $(BUILD)/site (make web-serve)"
else
	cmake -S . -B $(BUILD) -DDEVICE=$(DEVICE)
	cmake --build $(BUILD) --target $(SIM_BUILD_TARGET) --config Debug
endif

run: build
ifeq ($(DEVICE),Cardenza)
	@echo run is only supported for DEVICE=CardenzaSim or CardenzaWeb
	@exit 1
else ifeq ($(DEVICE),CardenzaWeb)
	$(MAKE) web-serve
else
	"$(abspath $(SIM_RUN_EXE))"
endif

web:
	$(MAKE) DEVICE=CardenzaWeb build

# The Learn manual (docs/learn) into $(WEB_BUILD)/site/learn: screenshots and reference tables come
# from the host build (CardenzaSimShot, CardenzaLearnDump in build/linux-groove).
LEARN_HOST_BUILD ?= build/linux-groove
learn:
	$(CMAKE) --build $(LEARN_HOST_BUILD) --target CardenzaSimShot CardenzaLearnDump
	python3 tools/learn/build.py --out $(WEB_BUILD)/site/learn --host-build $(LEARN_HOST_BUILD)

# Production page on Vercel (https://cardenza-synth.203.io); see tools/web-deploy.sh for the setup.
web-deploy:
	tools/web-deploy.sh

# Any static server works; the page needs no special headers.
web-serve:
	@test -f "$(WEB_BUILD)/site/cardenza.wasm" || { echo "No web build in $(WEB_BUILD)/site: run make web first"; exit 1; }
	@echo "Serving $(WEB_BUILD)/site on http://localhost:$(WEB_PORT)/"
	python3 -m http.server $(WEB_PORT) --bind 127.0.0.1 --directory "$(WEB_BUILD)/site"

set-target:
ifeq ($(DEVICE),Cardenza)
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) set-target $(IDF_TARGET))
else
	@echo set-target is only supported for DEVICE=Cardenza
	@exit 1
endif

flash bootloader-flash app-flash erase monitor:
ifeq ($(DEVICE),Cardenza)
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) $(IDF_PORT_ARG) $@)
else
	@echo $@ is only supported for DEVICE=Cardenza
	@exit 1
endif

flash-monitor:
ifeq ($(DEVICE),Cardenza)
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) $(IDF_PORT_ARG) flash monitor)
else
	@echo flash-monitor is only supported for DEVICE=Cardenza
	@exit 1
endif

remote-flash: build
ifeq ($(DEVICE),Cardenza)
	REMOTE='$(REMOTE)' tools/remote-flash/push.sh
else
	@echo remote-flash is only supported for DEVICE=Cardenza
	@exit 1
endif

host:
	@echo Cardenza hardware HAL is ESP-IDF only. Use DEVICE=CardenzaSim for host validation.
	@exit 1

validate-architecture:
	cmake -DCARDENZA_ROOT="$(abspath .)" -P cmake/ValidateArchitecture.cmake

clean:
ifeq ($(OS),Windows_NT)
	if exist "$(BUILD)" rmdir /s /q "$(BUILD)"
else
	rm -rf $(BUILD)
endif

fullclean:
ifeq ($(DEVICE),Cardenza)
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) fullclean)
else
	$(MAKE) DEVICE=$(DEVICE) clean
endif

firmware: build
firmware-target: set-target

cardenza:
	$(MAKE) DEVICE=Cardenza build

cardenza-firmware:
	$(MAKE) DEVICE=Cardenza build

cardenza-flash:
	$(MAKE) DEVICE=Cardenza flash PORT=$(PORT)

cardenza-monitor:
	$(MAKE) DEVICE=Cardenza monitor PORT=$(PORT)

cardenza-flash-monitor:
	$(MAKE) DEVICE=Cardenza flash-monitor PORT=$(PORT)

cardenza-host:
	$(MAKE) host

cardenza-clean:
	$(MAKE) DEVICE=Cardenza clean

cardenza-fullclean:
	$(MAKE) DEVICE=Cardenza fullclean
