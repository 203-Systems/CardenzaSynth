# ComputeCard Firmware Build

ComputeCard is now the ESP-IDF project root.

## Build

From the repository root, the Makefile now exposes the same flow:

```powershell
# Run this once per shell, or use an ESP-IDF terminal.
# Example: & '<esp-idf-install>\export.ps1'

make DEVICE=ComputeCard set-target IDF_TARGET=esp32s3
make DEVICE=ComputeCard build
make DEVICE=ComputeCard flash PORT=COM5
make DEVICE=ComputeCard monitor PORT=COM5
make DEVICE=ComputeCard flash-monitor PORT=COM5
```

On Windows, the Makefile calls `tools/run_idf.ps1`, which invokes `idf.py` through `IDF_PYTHON_ENV_PATH` and `IDF_PATH` from the active ESP-IDF shell. The repo `.venv` is not required for firmware builds.

Or run `idf.py` directly from the project root:

From the repository root in PowerShell:

```powershell
# Example: & '<esp-idf-install>\export.ps1'
Push-Location .\Devices\ComputeCard
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" set-target esp32s3
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B ..\..\build\Cardputer build
Pop-Location
```

`idf.py set-target esp32s3` is still important on a fresh checkout or after changing `sdkconfig.defaults`, because it regenerates `sdkconfig` from the checked-in defaults.

## Flash

```powershell
# Example: & '<esp-idf-install>\export.ps1'
Push-Location .\Devices\ComputeCard
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B ..\..\build\Cardputer -p COM_PORT flash
Pop-Location
```

Built artifacts land in `build/Cardputer/`.
