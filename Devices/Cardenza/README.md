# Cardenza Firmware Build

Cardenza is now the ESP-IDF project root.

## Build

From the repository root, the Makefile now exposes the same flow:

```powershell
# Run this once per shell, or use an ESP-IDF terminal.
# Example: & '<esp-idf-install>\export.ps1'

make DEVICE=Cardenza set-target IDF_TARGET=esp32s3
make DEVICE=Cardenza build
make DEVICE=Cardenza flash PORT=COM5
make DEVICE=Cardenza monitor PORT=COM5
make DEVICE=Cardenza flash-monitor PORT=COM5
```

On Windows, open an exported ESP-IDF shell first and run `make DEVICE=Cardenza ...`. The Makefile changes into `Devices/Cardenza` and invokes `idf.py` directly, so the repo `.venv` is not required for firmware builds.

Or run `idf.py` directly from the project root:

From the repository root in PowerShell:

```powershell
# Example: & '<esp-idf-install>\export.ps1'
Push-Location .\Devices\Cardenza
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" set-target esp32s3
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B ..\..\build\Cardenza build
Pop-Location
```

`idf.py set-target esp32s3` is still important on a fresh checkout or after changing `sdkconfig.defaults`, because it regenerates `sdkconfig` from the checked-in defaults.

## Flash

```powershell
# Example: & '<esp-idf-install>\export.ps1'
Push-Location .\Devices\Cardenza
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B ..\..\build\Cardenza -p COM_PORT flash
Pop-Location
```

Built artifacts land in `build/Cardenza/`.
