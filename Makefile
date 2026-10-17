DEVICE ?= Cardenza
IDF_TARGET ?= esp32s3
PORT ?=
AMY_MULTICORE ?= 0

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
else
$(error Unsupported DEVICE '$(DEVICE)'. Expected Cardenza or CardenzaSim)
endif

IDF_PORT_ARG := $(if $(PORT),-p '$(PORT)',)
CARDENZA_IDF_ARGS := -B '$(abspath $(BUILD))'
ifeq ($(AMY_MULTICORE),1)
CARDENZA_IDF_ARGS += -DCARDENZA_AMY_EXPERIMENTAL_MULTICORE=ON
else
CARDENZA_IDF_ARGS += -DCARDENZA_AMY_EXPERIMENTAL_MULTICORE=OFF
endif

.PHONY: all build run set-target flash bootloader-flash app-flash erase monitor flash-monitor clean fullclean validate-architecture firmware firmware-target host cardenza cardenza-firmware cardenza-flash cardenza-monitor cardenza-flash-monitor cardenza-host cardenza-clean cardenza-fullclean

all: build

build:
ifeq ($(DEVICE),Cardenza)
	$(call RUN_IDF,$(CARDENZA_IDF_ARGS) build)
else
	cmake -S . -B $(BUILD) -DDEVICE=$(DEVICE)
	cmake --build $(BUILD) --target $(SIM_BUILD_TARGET) --config Debug
endif

run: build
ifeq ($(DEVICE),Cardenza)
	@echo run is only supported for DEVICE=CardenzaSim
	@exit 1
else
	"$(abspath $(SIM_RUN_EXE))"
endif

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
