param(
  [string]$OutputPath = "OS/UI/BuiltInFonts.cpp",
  [string]$TerminusArchivePath = "$env:USERPROFILE\Downloads\terminus-font-4.49.1.tar.gz",
  [string]$TerminusCacheRoot = "$env:TEMP\ComputeCardFonts"
)

$ErrorActionPreference = 'Stop'

$fontDefs = @(
  @{ Name = 'kUiFontSmall'; BdfFileName = 'ter-u14n.bdf' },
  @{ Name = 'kUiFontBody'; BdfFileName = 'ter-u16n.bdf' },
  @{ Name = 'kUiFontTitle'; BdfFileName = 'ter-u24n.bdf' },
  @{ Name = 'kUiFontValue'; BdfFileName = 'ter-u32n.bdf' }
)

$chars = 32..126 | ForEach-Object { [int]$_ }

function Ensure-TerminusFontRoot {
  param(
    [hashtable[]]$Definitions,
    [string]$ArchivePath,
    [string]$CacheRoot
  )

  if (-not (Test-Path $ArchivePath)) {
    throw "Required Terminus archive not found: $ArchivePath"
  }

  $extractDir = Join-Path $CacheRoot "terminus-font"
  $fontRoot = Join-Path $extractDir "terminus-font-4.49.1"

  $missingFont = $false
  foreach ($definition in $Definitions) {
    if (-not (Test-Path (Join-Path $fontRoot $definition.BdfFileName))) {
      $missingFont = $true
      break
    }
  }

  if ($missingFont) {
    New-Item -ItemType Directory -Path $CacheRoot -Force | Out-Null

    if (Test-Path $extractDir) {
      Remove-Item $extractDir -Recurse -Force
    }

    New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
    tar -xf $ArchivePath -C $extractDir
  }

  foreach ($definition in $Definitions) {
    $fontPath = Join-Path $fontRoot $definition.BdfFileName
    if (-not (Test-Path $fontPath)) {
      throw "Required Terminus BDF file not found after extraction: $fontPath"
    }
  }

  return $fontRoot
}

function Parse-BdfFont {
  param([string]$FontPath)

  $fontBoundingBoxHeight = 0
  $fontAscent = 0
  $glyphs = @{}
  $currentGlyph = $null
  $readingBitmap = $false

  foreach ($rawLine in Get-Content $FontPath) {
    $line = $rawLine.Trim()

    if ($line.StartsWith('FONTBOUNDINGBOX ')) {
      $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
      $fontBoundingBoxHeight = [int]$parts[2]
      continue
    }

    if ($line.StartsWith('FONT_ASCENT ')) {
      $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
      $fontAscent = [int]$parts[1]
      continue
    }

    if ($line.StartsWith('STARTCHAR ')) {
      $currentGlyph = @{
        Encoding = -1
        DWidth = 0
        Width = 0
        Height = 0
        XOffset = 0
        YOffset = 0
        BitmapRows = New-Object System.Collections.Generic.List[string]
      }
      $readingBitmap = $false
      continue
    }

    if ($null -eq $currentGlyph) {
      continue
    }

    if ($line.StartsWith('ENCODING ')) {
      $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
      $currentGlyph.Encoding = [int]$parts[1]
      continue
    }

    if ($line.StartsWith('DWIDTH ')) {
      $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
      $currentGlyph.DWidth = [int]$parts[1]
      continue
    }

    if ($line.StartsWith('BBX ')) {
      $parts = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
      $currentGlyph.Width = [int]$parts[1]
      $currentGlyph.Height = [int]$parts[2]
      $currentGlyph.XOffset = [int]$parts[3]
      $currentGlyph.YOffset = [int]$parts[4]
      continue
    }

    if ($line -eq 'BITMAP') {
      $readingBitmap = $true
      continue
    }

    if ($line -eq 'ENDCHAR') {
      if ($currentGlyph.Encoding -ge 0) {
        $glyphs[$currentGlyph.Encoding] = $currentGlyph
      }
      $currentGlyph = $null
      $readingBitmap = $false
      continue
    }

    if ($readingBitmap) {
      $currentGlyph.BitmapRows.Add($line)
    }
  }

  if ($fontBoundingBoxHeight -le 0) {
    throw "Failed to parse FONTBOUNDINGBOX from $FontPath"
  }

  if ($fontAscent -le 0) {
    throw "Failed to parse FONT_ASCENT from $FontPath"
  }

  return @{
    LineHeight = $fontBoundingBoxHeight
    FontAscent = $fontAscent
    Glyphs = $glyphs
  }
}

function Convert-BdfGlyphToBitmapBytes {
  param([hashtable]$Glyph)

  if ($Glyph.Width -le 0 -or $Glyph.Height -le 0) {
    return @()
  }

  $bitmapBytes = New-Object System.Collections.Generic.List[byte]
  foreach ($rowHex in $Glyph.BitmapRows) {
    $bitsInRow = $rowHex.Length * 4
    $rowValue = [Convert]::ToUInt64($rowHex, 16)
    for ($column = 0; $column -lt $Glyph.Width; ++$column) {
      $shift = $bitsInRow - 1 - $column
      $bit = ($rowValue -shr $shift) -band 1
      $bitmapBytes.Add([byte]($(if ($bit -eq 1) { 255 } else { 0 })))
    }
  }

  return $bitmapBytes.ToArray()
}

function New-EmptyGlyph {
  param([int]$Advance)

  return @{
    Width = 0
    Height = 0
    XOffset = 0
    YOffset = 0
    XAdvance = $Advance
    Bitmap = @()
  }
}

function Build-GlyphData {
  param(
    [hashtable]$Font,
    [int]$CodePoint
  )

  $glyph = $Font.Glyphs[$CodePoint]
  if ($null -eq $glyph) {
    if ($Font.Glyphs.ContainsKey([int][char]'?')) {
      $glyph = $Font.Glyphs[[int][char]'?']
    }
    elseif ($Font.Glyphs.ContainsKey(32)) {
      $glyph = $Font.Glyphs[32]
    }
    else {
      return New-EmptyGlyph -Advance 0
    }
  }

  if ($glyph.Width -le 0 -or $glyph.Height -le 0) {
    return New-EmptyGlyph -Advance $glyph.DWidth
  }

  return @{
    Width = $glyph.Width
    Height = $glyph.Height
    XOffset = $glyph.XOffset
    YOffset = $Font.FontAscent - ($glyph.Height + $glyph.YOffset)
    XAdvance = $glyph.DWidth
    Bitmap = Convert-BdfGlyphToBitmapBytes -Glyph $glyph
  }
}

function Join-Bytes {
  param([byte[]]$Values)
  if ($Values.Length -eq 0) {
    return ''
  }
  return ($Values | ForEach-Object { "0x{0:X2}" -f $_ }) -join ', '
}

$fontRoot = Ensure-TerminusFontRoot -Definitions $fontDefs -ArchivePath $TerminusArchivePath -CacheRoot $TerminusCacheRoot

$output = New-Object System.Text.StringBuilder
[void]$output.AppendLine('#include "OS/UI/FontAtlas.h"')
[void]$output.AppendLine()
[void]$output.AppendLine('namespace ui {')
[void]$output.AppendLine('namespace {')

foreach ($fontDef in $fontDefs) {
  $fontPath = Join-Path $fontRoot $fontDef.BdfFileName
  $font = Parse-BdfFont -FontPath $fontPath
  $bitmapBytes = New-Object System.Collections.Generic.List[byte]
  $glyphEntries = New-Object System.Collections.Generic.List[string]

  foreach ($codePoint in $chars) {
    $glyph = Build-GlyphData -Font $font -CodePoint $codePoint
    $offset = $bitmapBytes.Count
    foreach ($value in $glyph.Bitmap) {
      $bitmapBytes.Add($value)
    }

    $glyphEntries.Add("  { $offset, $($glyph.Width), $($glyph.Height), $($glyph.XOffset), $($glyph.YOffset), $($glyph.XAdvance) }")
  }

  [void]$output.AppendLine("const std::uint8_t $($fontDef.Name)Bitmap[] = {")
  $allBytes = $bitmapBytes.ToArray()
  for ($index = 0; $index -lt $allBytes.Length; $index += 20) {
    $count = [Math]::Min(20, $allBytes.Length - $index)
    $chunk = New-Object byte[] $count
    [Array]::Copy($allBytes, $index, $chunk, 0, $count)
    [void]$output.AppendLine("  $(Join-Bytes $chunk),")
  }
  [void]$output.AppendLine('};')
  [void]$output.AppendLine()
  [void]$output.AppendLine("const RasterGlyph $($fontDef.Name)Glyphs[] = {")
  foreach ($entry in $glyphEntries) {
    [void]$output.AppendLine($entry + ',')
  }
  [void]$output.AppendLine('};')
  [void]$output.AppendLine()
  [void]$output.AppendLine("const RasterFont $($fontDef.Name) = { 32, 126, $($font.LineHeight), $($fontDef.Name)Bitmap, $($fontDef.Name)Glyphs };")
  [void]$output.AppendLine()
}

[void]$output.AppendLine('}  // namespace')
[void]$output.AppendLine()
[void]$output.AppendLine('const RasterFont& UiFontForScale(int scale)')
[void]$output.AppendLine('{')
[void]$output.AppendLine('  if (scale <= 1) return kUiFontSmall;')
[void]$output.AppendLine('  if (scale == 2) return kUiFontBody;')
[void]$output.AppendLine('  if (scale == 3) return kUiFontTitle;')
[void]$output.AppendLine('  return kUiFontValue;')
[void]$output.AppendLine('}')
[void]$output.AppendLine()
[void]$output.AppendLine('}  // namespace ui')

Set-Content -Path $OutputPath -Value $output.ToString() -Encoding ascii
Write-Host "Generated $OutputPath from Terminus BDF bitmaps"
