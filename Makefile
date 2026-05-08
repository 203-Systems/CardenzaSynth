DEVICE ?= ComputeCard
IDF_TARGET ?= esp32s3
PORT ?=

ifeq ($(DEVICE),ComputeCard)
BUILD := build/Cardputer
DEVICE_PROJECT_DIR := Devices/ComputeCard
ifeq ($(OS),Windows_NT)
IDF := powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(abspath tools/run_idf.ps1)" -ProjectDir "$(abspath $(DEVICE_PROJECT_DIR))"
else
IDF := cd $(DEVICE_PROJECT_DIR) && idf.py
endif
else ifeq ($(DEVICE),ComputeCardSim)
BUILD := out/validate-computecard-sim
SIM_BUILD_TARGET := ComputeCardSimHost
ifeq ($(OS),Windows_NT)
SIM_BUILD_TARGET := ComputeCardSimDesktop
endif
else
$(error Unsupported DEVICE '$(DEVICE)'. Expected ComputeCard or ComputeCardSim)
endif

IDF_PORT_ARG := $(if $(PORT),-p $(PORT),)
COMPUTECARD_IDF_ARGS := -B "$(abspath $(BUILD))"

.PHONY: all build set-target flash bootloader-flash app-flash erase monitor flash-monitor clean fullclean validate-architecture firmware firmware-target host cardputer cardputer-firmware cardputer-flash cardputer-monitor cardputer-flash-monitor cardputer-host cardputer-clean cardputer-fullclean

all: build

build:
ifeq ($(DEVICE),ComputeCard)
	$(IDF) $(COMPUTECARD_IDF_ARGS) build
else
	cmake -S . -B $(BUILD) -DDEVICE=$(DEVICE)
	cmake --build $(BUILD) --target $(SIM_BUILD_TARGET) --config Debug
endif

set-target:
ifeq ($(DEVICE),ComputeCard)
	$(IDF) $(COMPUTECARD_IDF_ARGS) set-target $(IDF_TARGET)
else
	@echo set-target is only supported for DEVICE=ComputeCard
	@exit 1
endif

flash bootloader-flash app-flash erase monitor:
ifeq ($(DEVICE),ComputeCard)
	$(IDF) $(COMPUTECARD_IDF_ARGS) $(IDF_PORT_ARG) $@
else
	@echo $@ is only supported for DEVICE=ComputeCard
	@exit 1
endif

flash-monitor:
ifeq ($(DEVICE),ComputeCard)
	$(IDF) $(COMPUTECARD_IDF_ARGS) $(IDF_PORT_ARG) flash monitor
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
	$(IDF) $(COMPUTECARD_IDF_ARGS) fullclean
else
	$(MAKE) DEVICE=$(DEVICE) clean
endif

firmware: build
firmware-target: set-target

cardputer:
	$(MAKE) DEVICE=ComputeCard build

cardputer-firmware:
	$(MAKE) DEVICE=ComputeCard build

cardputer-flash:
	$(MAKE) DEVICE=ComputeCard flash PORT=$(PORT)

cardputer-monitor:
	$(MAKE) DEVICE=ComputeCard monitor PORT=$(PORT)

cardputer-flash-monitor:
	$(MAKE) DEVICE=ComputeCard flash-monitor PORT=$(PORT)

cardputer-host:
	$(MAKE) host

cardputer-clean:
	$(MAKE) DEVICE=ComputeCard clean

cardputer-fullclean:
	$(MAKE) DEVICE=ComputeCard fullclean
