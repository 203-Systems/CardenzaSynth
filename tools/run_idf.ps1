param(
    [string]$ProjectDir = (Get-Location).Path,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$IdfArgs
)

$ErrorActionPreference = 'Stop'

if (-not $env:IDF_PATH) {
    throw 'IDF_PATH is not set. Run the ESP-IDF export script or open an ESP-IDF terminal first.'
}

if (-not $env:IDF_PYTHON_ENV_PATH) {
    throw 'IDF_PYTHON_ENV_PATH is not set. Run the ESP-IDF export script or open an ESP-IDF terminal first.'
}

$python = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$idf = Join-Path $env:IDF_PATH 'tools\idf.py'

if (-not (Test-Path $python)) {
    throw "ESP-IDF Python executable was not found: $python"
}

if (-not (Test-Path $idf)) {
    throw "idf.py was not found: $idf"
}

Push-Location $ProjectDir
try {
    & $python $idf @IdfArgs
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}