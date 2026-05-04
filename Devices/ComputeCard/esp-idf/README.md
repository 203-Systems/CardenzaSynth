# ComputeCard ESP-IDF Build

This directory is a standalone ESP-IDF firmware project for the ComputeCard hardware target.

## Build

From the repository root in PowerShell:

```powershell
& 'C:\espressif\v5.3.4\export.ps1'
Push-Location .\Devices\ComputeCard\esp-idf
idf.py set-target esp32s3
idf.py build
Pop-Location
```

`idf.py set-target esp32s3` is important on a fresh checkout or after changing `sdkconfig.defaults`, because it regenerates `sdkconfig` from the checked-in defaults.

## Flash

```powershell
& 'C:\espressif\v5.3.4\export.ps1'
Push-Location .\Devices\ComputeCard\esp-idf
idf.py -p COM_PORT flash
Pop-Location
```

Built artifacts land in `Devices/ComputeCard/esp-idf/build/`.