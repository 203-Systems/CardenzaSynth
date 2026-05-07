DEVICE ?= ComputeCard
IDF_TARGET ?= esp32s3
PORT ?=
IDF_EXPORT_PS1 ?= C:/espressif/v5.3.4/export.ps1
POWERSHELL := C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass

ifeq ($(DEVICE),ComputeCard)
DEVICE_BUILD_DIR := build/Cardputer
DEVICE_PROJECT_DIR := Devices/ComputeCard
SUPPORTS_FLASH := 1
else ifeq ($(DEVICE),ComputeCardSim)
DEVICE_BUILD_DIR := out/validate-computecard-sim
SIM_BUILD_TARGET := ComputeCardSimHost
ifeq ($(OS),Windows_NT)
SIM_BUILD_TARGET := ComputeCardSimDesktop
endif
else
$(error Unsupported DEVICE '$(DEVICE)'. Expected ComputeCard or ComputeCardSim)
endif

IDF_PORT_ARG := $(if $(PORT),-p $(PORT),)

.PHONY: all build set-target flash monitor flash-monitor clean firmware firmware-target host cardputer cardputer-firmware cardputer-flash cardputer-monitor cardputer-flash-monitor cardputer-host cardputer-clean

all: build

build:
ifeq ($(DEVICE),ComputeCard)
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(DEVICE_PROJECT_DIR)'; idf.py -B '$(abspath $(DEVICE_BUILD_DIR))' build"
else
	cd $(DEVICE_PROJECT_DIR) && idf.py -B $(abspath $(DEVICE_BUILD_DIR)) build
endif
else
	cmake -S . -B $(DEVICE_BUILD_DIR) -DDEVICE=$(DEVICE)
	cmake --build $(DEVICE_BUILD_DIR) --target $(SIM_BUILD_TARGET) --config Debug
endif

set-target:
ifeq ($(DEVICE),ComputeCard)
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(DEVICE_PROJECT_DIR)'; idf.py -B '$(abspath $(DEVICE_BUILD_DIR))' set-target $(IDF_TARGET)"
else
	cd $(DEVICE_PROJECT_DIR) && idf.py -B $(abspath $(DEVICE_BUILD_DIR)) set-target $(IDF_TARGET)
endif
else
	@echo set-target is only supported for DEVICE=ComputeCard
	@exit 1
endif

flash:
ifeq ($(DEVICE),ComputeCard)
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(DEVICE_PROJECT_DIR)'; idf.py -B '$(abspath $(DEVICE_BUILD_DIR))' $(IDF_PORT_ARG) flash"
else
	cd $(DEVICE_PROJECT_DIR) && idf.py -B $(abspath $(DEVICE_BUILD_DIR)) $(IDF_PORT_ARG) flash
endif
else
	@echo flash is only supported for non-simulator devices
	@exit 1
endif

monitor:
ifeq ($(DEVICE),ComputeCard)
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(DEVICE_PROJECT_DIR)'; idf.py -B '$(abspath $(DEVICE_BUILD_DIR))' $(IDF_PORT_ARG) monitor"
else
	cd $(DEVICE_PROJECT_DIR) && idf.py -B $(abspath $(DEVICE_BUILD_DIR)) $(IDF_PORT_ARG) monitor
endif
else
	@echo monitor is only supported for non-simulator devices
	@exit 1
endif

flash-monitor:
ifeq ($(DEVICE),ComputeCard)
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(DEVICE_PROJECT_DIR)'; idf.py -B '$(abspath $(DEVICE_BUILD_DIR))' $(IDF_PORT_ARG) flash monitor"
else
	cd $(DEVICE_PROJECT_DIR) && idf.py -B $(abspath $(DEVICE_BUILD_DIR)) $(IDF_PORT_ARG) flash monitor
endif
else
	@echo flash-monitor is only supported for non-simulator devices
	@exit 1
endif

host:
	cmake -S . -B out/validate-computecard -DDEVICE=ComputeCard
	cmake --build out/validate-computecard --target ComputeCardHAL --config Debug

clean:
ifeq ($(OS),Windows_NT)
	if exist "$(DEVICE_BUILD_DIR)" rmdir /s /q "$(DEVICE_BUILD_DIR)"
else
	rm -rf $(DEVICE_BUILD_DIR)
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
