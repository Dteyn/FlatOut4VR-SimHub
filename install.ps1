[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$registrationId = '5059abb9-d53b-4e70-abb7-23ecfec6d4be'
$packageRoot = $PSScriptRoot
$sourceDirectory = Join-Path $packageRoot 'FlatOut4VR'
$definitionsDirectory = Join-Path $env:LOCALAPPDATA 'SimHub\ExternalSims\Definitions'
$registrationDirectory = Join-Path $env:LOCALAPPDATA 'SimHub\ExternalSims\Registrations'
$destinationDirectory = Join-Path $definitionsDirectory 'FlatOut4VR'
$definitionFile = Join-Path $destinationDirectory 'FlatOut4VR.simdef'
$registrationFile = Join-Path $registrationDirectory "$registrationId.shlink"

if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    throw 'LOCALAPPDATA is not available. Run this installer from a normal Windows user session.'
}

$runtimeFiles = @('FlatOut4VR.simdef', 'FlatOut4VR-Banner.jpg', 'extractor.exe', 'extractor.ini')
foreach ($file in $runtimeFiles) {
    $sourceFile = Join-Path $sourceDirectory $file
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
        throw "Required release file not found: $sourceFile"
    }
}

New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $registrationDirectory -Force | Out-Null

foreach ($file in $runtimeFiles) {
    $sourceFile = Join-Path $sourceDirectory $file
    $destinationFile = Join-Path $destinationDirectory $file
    if ($file -eq 'extractor.ini' -and (Test-Path -LiteralPath $destinationFile -PathType Leaf)) {
        Write-Host "Preserved existing configuration: $destinationFile"
        continue
    }
    Copy-Item -LiteralPath $sourceFile -Destination $destinationFile -Force
}

Set-Content -LiteralPath $registrationFile -Value $definitionFile -NoNewline -Encoding ASCII

Write-Host 'FlatOut 4 VR SimHub Extractor installed.'
Write-Host "Definition:   $definitionFile"
Write-Host "Registration: $registrationFile"
Write-Host 'Start SimHub and activate FlatOut 4: Total Insanity VR.'
