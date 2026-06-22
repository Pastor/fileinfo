# check_build_env.ps1 - проверка и установка окружения для сборки fileinfo
$ErrorActionPreference = "Continue"
$allOk = $true

function Check($label, $test, $fix) {
    if (& $test) {
        Write-Host "[OK]  $label" -ForegroundColor Green
    } else {
        Write-Host "[MISS] $label" -ForegroundColor Red
        $script:allOk = $false
        if ($fix) { Write-Host "      -> $fix" -ForegroundColor Yellow }
    }
}

Write-Host "`n=== fileinfo build environment check ===`n"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
Check "vswhere.exe" { Test-Path $vsWhere } "Install Visual Studio Build Tools from https://aka.ms/vs"

$msbuild = $null
if (Test-Path $vsWhere) {
    $msbuild = & $vsWhere -all -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null | Select-Object -Last 1
}
Check "MSBuild.exe" { $msbuild -and (Test-Path $msbuild) } "Install C++ build tools workload via Visual Studio Installer"

$msvcFound = $false
if (Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio") {
    $cl = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Recurse -Filter "cl.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    $msvcFound = ($null -ne $cl)
}
Check "MSVC compiler (cl.exe)" { $msvcFound } "Install MSVC build tools via Visual Studio Installer"

$sdkBase = "C:\Program Files (x86)\Windows Kits\10\Include"
$sdkVersions = @()
if (Test-Path $sdkBase) {
    $sdkVersions = Get-ChildItem $sdkBase -Directory | Select-Object -ExpandProperty Name
}
$sdkLabel = "Windows SDK 10.x (found: " + ($sdkVersions -join ", ") + ")"
Check $sdkLabel { $sdkVersions.Count -gt 0 } "Install Windows SDK via Visual Studio Installer"

$toolsetsFound = @()
if (Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio") {
    $toolsetsFound = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Recurse -Directory -Filter "PlatformToolsets" -ErrorAction SilentlyContinue |
        Get-ChildItem -Directory | Select-Object -ExpandProperty Name
}
$toolsetLabel = "PlatformToolset (found: " + ($toolsetsFound -join ", ") + ")"
Check $toolsetLabel { $toolsetsFound.Count -gt 0 } "Install a C++ workload in Visual Studio Installer"

# --- Exiv2 ---
$repoRoot   = $PSScriptRoot
$exiv2Root  = Join-Path $repoRoot "3rdParty\exiv2"
$exiv2Inc   = Join-Path $exiv2Root "include\exiv2\exiv2.hpp"
$exiv2Lib64 = Join-Path $exiv2Root "lib64\exiv2.lib"

if ((Test-Path $exiv2Inc) -and (Test-Path $exiv2Lib64)) {
    Write-Host "[OK]  Exiv2 ($exiv2Root)" -ForegroundColor Green
} else {
    Write-Host "[MISS] Exiv2 (optional, for EXIF tab) -- installing..." -ForegroundColor Yellow

    # Only x64 Windows package is distributed on GitHub Releases
    $EXIV2_VERSION = "0.28.5"
    $ZIP_NAME      = "exiv2-$EXIV2_VERSION-2022msvc-AMD64.zip"
    $DOWNLOAD_URL  = "https://github.com/Exiv2/exiv2/releases/download/v$EXIV2_VERSION/$ZIP_NAME"

    $tmpDir  = Join-Path $env:TEMP "exiv2_install"
    $zipPath = Join-Path $tmpDir $ZIP_NAME
    $extDir  = Join-Path $tmpDir "extracted"

    if (Test-Path $tmpDir) { Remove-Item $tmpDir -Recurse -Force }
    New-Item $tmpDir -ItemType Directory | Out-Null

    Write-Host "  Downloading $ZIP_NAME ..." -NoNewline
    try {
        Invoke-WebRequest -Uri $DOWNLOAD_URL -OutFile $zipPath -UseBasicParsing -ErrorAction Stop
        Write-Host " OK" -ForegroundColor Green
    } catch {
        Write-Host " FAILED: $_" -ForegroundColor Red
        Write-Host "[WARN] Exiv2 not installed -- EXIF tab will show placeholder." -ForegroundColor Yellow
        $script:allOk = $false
        $zipPath = $null
    }

    if ($zipPath -and (Test-Path $zipPath)) {
        Write-Host "  Extracting ..."
        Expand-Archive -Path $zipPath -DestinationPath $extDir -Force

        # The zip has a single top-level folder: exiv2-X.Y.Z-2022msvc-AMD64
        $srcRoot = Get-ChildItem $extDir -Directory | Select-Object -First 1 -ExpandProperty FullName

        New-Item -ItemType Directory -Force (Join-Path $exiv2Root "include") | Out-Null
        New-Item -ItemType Directory -Force (Join-Path $exiv2Root "lib")     | Out-Null
        New-Item -ItemType Directory -Force (Join-Path $exiv2Root "lib64")   | Out-Null
        New-Item -ItemType Directory -Force (Join-Path $exiv2Root "bin64")   | Out-Null

        # Headers
        $incSrc = Join-Path $srcRoot "include"
        if (Test-Path $incSrc) {
            Copy-Item "$incSrc\*" (Join-Path $exiv2Root "include") -Recurse -Force
            Write-Host "  Headers installed." -ForegroundColor Green
        }

        # x64 .lib  (goes to lib64\ as expected by fileinfo.vcxproj)
        $libSrc = Join-Path $srcRoot "lib"
        if (Test-Path $libSrc) {
            Get-ChildItem $libSrc -Filter "*.lib" | Copy-Item -Destination (Join-Path $exiv2Root "lib64") -Force
            Write-Host "  x64 lib installed." -ForegroundColor Green
        }

        # x64 .dll  (goes to bin64\ for runtime, and also to lib64\ for convenience)
        $binSrc = Join-Path $srcRoot "bin"
        if (Test-Path $binSrc) {
            Get-ChildItem $binSrc -Filter "*.dll" | Copy-Item -Destination (Join-Path $exiv2Root "bin64") -Force
            Get-ChildItem $binSrc -Filter "*.dll" | Copy-Item -Destination (Join-Path $exiv2Root "lib64") -Force
        }

        Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

        if (Test-Path $exiv2Inc) {
            Write-Host "[OK]  Exiv2 $EXIV2_VERSION installed to $exiv2Root" -ForegroundColor Green
            Write-Host "      NOTE: to enable EXIF tab, add EXIV2_AVAILABLE to PreprocessorDefinitions" -ForegroundColor Cyan
            Write-Host "            and uncomment exiv2.lib in AdditionalDependencies in fileinfo.vcxproj" -ForegroundColor Cyan
        } else {
            Write-Host "[WARN] Exiv2 installation incomplete." -ForegroundColor Yellow
            $script:allOk = $false
        }
    }
}

Write-Host ""
if ($allOk) {
    Write-Host "All required dependencies found. Ready to build." -ForegroundColor Green
} else {
    Write-Host "Non-critical items missing (see above). Core build will succeed." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Build commands ==="
if ($msbuild) {
    $sln = Join-Path $PSScriptRoot "fileinfo.sln"
    Write-Host "MSBuild:  $msbuild"
    Write-Host "Solution: $sln"
    Write-Host ""
    Write-Host "Release x64:"
    Write-Host "  & `"$msbuild`" `"$sln`" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:WindowsTargetPlatformVersion=10.0.26100.0 /m /v:m"
    Write-Host ""
    Write-Host "Debug x64:"
    Write-Host "  & `"$msbuild`" `"$sln`" /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:WindowsTargetPlatformVersion=10.0.26100.0 /m /v:m"
    Write-Host ""
    Write-Host "Output: Release_x64\fileinfo.exe  or  Debug_x64\fileinfo.exe"
}
