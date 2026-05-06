# ComputeCard Firmware Build

ComputeCard is now the ESP-IDF project root.

## Build

From the repository root in PowerShell:

```powershell
& 'C:\espressif\v5.3.4\export.ps1'
Push-Location .\Devices\ComputeCard
idf.py set-target esp32s3
idf.py -B ..\..\build\Cardputer build
Pop-Location
```

`idf.py set-target esp32s3` is still important on a fresh checkout or after changing `sdkconfig.defaults`, because it regenerates `sdkconfig` from the checked-in defaults.

## Flash

```powershell
& 'C:\espressif\v5.3.4\export.ps1'
Push-Location .\Devices\ComputeCard
idf.py -B ..\..\build\Cardputer -p COM_PORT flash
Pop-Location
```

Built artifacts land in `build/Cardputer/`.