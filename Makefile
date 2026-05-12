DEVICE ?= ComputeCard
IDF_TARGET ?= esp32s3
PORT ?=
AMY_MULTICORE ?= 0

ifeq ($(DEVICE),ComputeCard)
BUILD := build/ComputeCard
DEVICE_PROJECT_DIR := Devices/ComputeCard

ifeq ($(OS),Windows_NT)
define RUN_IDF
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& { Set-Location '$(abspath $(DEVICE_PROJECT_DIR))'; idf.py $(1) }"
endef
else
define RUN_IDF
cd $(DEVICE_PROJECT_DIR) && idf.py $(1)
endef
endif
else ifeq ($(DEVICE),ComputeCardSim)
BUILD := build/ComputeCardSim
SIM_BUILD_TARGET := ComputeCardSimHost
SIM_RUN_EXE := $(BUILD)/Devices/ComputeCardSim/ComputeCardSimHost
ifeq ($(OS),Windows_NT)
SIM_BUILD_TARGET := ComputeCardSimDesktop
SIM_RUN_EXE := $(BUILD)/Devices/ComputeCardSim/Debug/ComputeCardSimDesktop.exe
endif
else
$(error Unsupported DEVICE '$(DEVICE)'. Expected ComputeCard or ComputeCardSim)
endif

IDF_PORT_ARG := $(if $(PORT),-p '$(PORT)',)
COMPUTECARD_IDF_ARGS := -B '$(abspath $(BUILD))'
ifeq ($(AMY_MULTICORE),1)
COMPUTECARD_IDF_ARGS += -DCCOS_AMY_EXPERIMENTAL_MULTICORE=ON
else
COMPUTECARD_IDF_ARGS += -DCCOS_AMY_EXPERIMENTAL_MULTICORE=OFF
endif

.PHONY: all build run set-target flash bootloader-flash app-flash erase monitor flash-monitor clean fullclean validate-architecture firmware firmware-target host computecard computecard-firmware computecard-flash computecard-monitor computecard-flash-monitor computecard-host computecard-clean computecard-fullclean

all: build

build:
ifeq ($(DEVICE),ComputeCard)
	$(call RUN_IDF,$(COMPUTECARD_IDF_ARGS) build)
else
	cmake -S . -B $(BUILD) -DDEVICE=$(DEVICE)
	cmake --build $(BUILD) --target $(SIM_BUILD_TARGET) --config Debug
endif

run: build
ifeq ($(DEVICE),ComputeCard)
	@echo run is only supported for DEVICE=ComputeCardSim
	@exit 1
else
	"$(abspath $(SIM_RUN_EXE))"
endif

set-target:
ifeq ($(DEVICE),ComputeCard)
	$(call RUN_IDF,$(COMPUTECARD_IDF_ARGS) set-target $(IDF_TARGET))
else
	@echo set-target is only supported for DEVICE=ComputeCard
	@exit 1
endif

flash bootloader-flash app-flash erase monitor:
ifeq ($(DEVICE),ComputeCard)
	$(call RUN_IDF,$(COMPUTECARD_IDF_ARGS) $(IDF_PORT_ARG) $@)
else
	@echo $@ is only supported for DEVICE=ComputeCard
	@exit 1
endif

flash-monitor:
ifeq ($(DEVICE),ComputeCard)
	$(call RUN_IDF,$(COMPUTECARD_IDF_ARGS) $(IDF_PORT_ARG) flash monitor)
else
	@echo flash-monitor is only supported for DEVICE=ComputeCard
	@exit 1
endif

host:
	@echo ComputeCard hardware HAL is ESP-IDF only. Use DEVICE=ComputeCardSim for host validation.
	@exit 1

validate-architecture:
	cmake -DCCOS_ROOT="$(abspath .)" -P cmake/ValidateArchitecture.cmake

clean:
ifeq ($(OS),Windows_NT)
	if exist "$(BUILD)" rmdir /s /q "$(BUILD)"
else
	rm -rf $(BUILD)
endif

fullclean:
ifeq ($(DEVICE),ComputeCard)
	$(call RUN_IDF,$(COMPUTECARD_IDF_ARGS) fullclean)
else
	$(MAKE) DEVICE=$(DEVICE) clean
endif

firmware: build
firmware-target: set-target

computecard:
	$(MAKE) DEVICE=ComputeCard build

computecard-firmware:
	$(MAKE) DEVICE=ComputeCard build

computecard-flash:
	$(MAKE) DEVICE=ComputeCard flash PORT=$(PORT)

computecard-monitor:
	$(MAKE) DEVICE=ComputeCard monitor PORT=$(PORT)

computecard-flash-monitor:
	$(MAKE) DEVICE=ComputeCard flash-monitor PORT=$(PORT)

computecard-host:
	$(MAKE) host

computecard-clean:
	$(MAKE) DEVICE=ComputeCard clean

computecard-fullclean:
	$(MAKE) DEVICE=ComputeCard fullclean
