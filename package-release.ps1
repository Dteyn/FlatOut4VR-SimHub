[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$version = '0.4.0'

if (-not $SkipBuild) {
    & (Join-Path $projectRoot 'build-vs2022.bat') Release
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE."
    }
}

$extractor = Join-Path $projectRoot 'out\Release\extractor.exe'
$requiredFiles = @(
    $extractor,
    (Join-Path $projectRoot 'simdef\FlatOut4VR.simdef'),
    (Join-Path $projectRoot 'simdef\FlatOut4VR-Banner.jpg'),
    (Join-Path $projectRoot 'simdef\extractor.ini'),
    (Join-Path $projectRoot 'registration\5059abb9-d53b-4e70-abb7-23ecfec6d4be.shlink'),
    (Join-Path $projectRoot 'install.ps1'),
    (Join-Path $projectRoot 'install.bat'),
    (Join-Path $projectRoot 'INSTALL.txt'),
    (Join-Path $projectRoot 'LICENSE')
)
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required package file not found: $file"
    }
}

$packageName = "FlatOut-4-VR-SimHub-v$version"
$packagesDir = Join-Path $projectRoot 'out\Packages'
$packageDir = Join-Path $packagesDir $packageName
$archivePath = Join-Path $packagesDir "$packageName.zip"

if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
 $runtimeDir = Join-Path $packageDir 'FlatOut4VR'
New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null

Copy-Item -LiteralPath $extractor -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'simdef\FlatOut4VR.simdef') -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'simdef\FlatOut4VR-Banner.jpg') -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'simdef\extractor.ini') -Destination $runtimeDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'registration\5059abb9-d53b-4e70-abb7-23ecfec6d4be.shlink') -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'install.ps1') -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'install.bat') -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'INSTALL.txt') -Destination $packageDir
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination (Join-Path $packageDir 'LICENSE.txt')

Compress-Archive -LiteralPath $packageDir -DestinationPath $archivePath -CompressionLevel Optimal
Write-Host "Release package created:"
Write-Host "  $archivePath"
