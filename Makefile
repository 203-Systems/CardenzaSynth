DEVICE ?= ComputeCard
IDF_EXPORT_PS1 ?= C:/espressif/v5.3.4/export.ps1

CARDPUTER_FIRMWARE_BUILD := build/Cardputer
CARDPUTER_VALIDATE_BUILD := out/validate-computecard
CARDPUTER_IDF_PROJECT := Devices/ComputeCard
POWERSHELL := C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass

.PHONY: all cardputer cardputer-firmware cardputer-host cardputer-clean

all: cardputer

cardputer: cardputer-firmware

cardputer-firmware:
ifeq ($(OS),Windows_NT)
	$(POWERSHELL) -Command "& '$(IDF_EXPORT_PS1)'; Set-Location '$(CARDPUTER_IDF_PROJECT)'; idf.py -B '$(abspath $(CARDPUTER_FIRMWARE_BUILD))' build"
else
	cd $(CARDPUTER_IDF_PROJECT) && idf.py -B $(abspath $(CARDPUTER_FIRMWARE_BUILD)) build
endif

cardputer-host:
	cmake -S . -B $(CARDPUTER_VALIDATE_BUILD) -DDEVICE=$(DEVICE)
	cmake --build $(CARDPUTER_VALIDATE_BUILD) --target ComputeCardHAL --config Debug

cardputer-clean:
ifeq ($(OS),Windows_NT)
	if exist "$(CARDPUTER_FIRMWARE_BUILD)" rmdir /s /q "$(CARDPUTER_FIRMWARE_BUILD)"
else
	rm -rf $(CARDPUTER_FIRMWARE_BUILD)
endif