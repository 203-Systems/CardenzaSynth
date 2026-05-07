# ComputeCard Firmware Build

ComputeCard is now the ESP-IDF project root.

## Build

From the repository root, the Makefile now exposes the same flow:

```powershell
# Run this once per shell, or use an ESP-IDF terminal.
& 'C:\espressif\v5.3.4\export.ps1'

make DEVICE=ComputeCard set-target IDF_TARGET=esp32s3
make DEVICE=ComputeCard build
make DEVICE=ComputeCard flash PORT=COM5
make DEVICE=ComputeCard monitor PORT=COM5
make DEVICE=ComputeCard flash-monitor PORT=COM5
```

The Makefile does not run `export.ps1`; it expects `idf.py` to already be available in the current shell. The repo `.venv` is not required for firmware builds.

Or run `idf.py` directly from the project root:

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
