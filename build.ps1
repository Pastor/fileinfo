# build.ps1 - управление сборкой fileinfo
#
# Использование:
#   .\build.ps1                        -- clean + build (Release|x64)
#   .\build.ps1 clean                  -- только удалить артефакты
#   .\build.ps1 build                  -- только собрать
#   .\build.ps1 clean build -Configuration Debug -Platform x64
#
param(
    [Parameter(Position = 0, ValueFromRemainingArguments)]
    [string[]]$Commands,

    [string]$Configuration = "Release",
    [string]$Platform      = "x64"
)

$ErrorActionPreference = "Stop"

# По умолчанию -- clean затем build
if (-not $Commands) { $Commands = @("clean", "build") }

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------
function Invoke-Clean {
    Write-Host "--- clean ---" -ForegroundColor Cyan

    # Конечные папки в корне репозитория
    $outputDirs = @("Release", "Debug", "Release_x64", "Debug_x64")
    foreach ($d in $outputDirs) {
        $path = Join-Path $PSScriptRoot $d
        if (Test-Path $path) {
            Remove-Item $path -Recurse -Force
            Write-Host "  removed $d\"
        }
    }

    # Промежуточные объектные файлы внутри fileinfo\
    $projDir = Join-Path $PSScriptRoot "fileinfo"
    $intDirs = @("Release", "Debug", "Release_x64", "Debug_x64")
    foreach ($d in $intDirs) {
        $path = Join-Path $projDir $d
        if (Test-Path $path) {
            Remove-Item $path -Recurse -Force
            Write-Host "  removed fileinfo\$d\"
        }
    }

    # Бинарный кеш редактора ресурсов
    $aps = Join-Path $projDir "fileinfo.aps"
    if (Test-Path $aps) {
        Remove-Item $aps -Force
        Write-Host "  removed fileinfo\fileinfo.aps"
    }

    Write-Host "Clean done." -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
function Invoke-Build {
    Write-Host "--- build ---" -ForegroundColor Cyan

    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        Write-Error "vswhere.exe not found. Run check_build_env.ps1 first."
        exit 1
    }

    $msbuild = & $vsWhere -all -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null | Select-Object -Last 1
    if (-not $msbuild) {
        Write-Error "MSBuild not found. Run check_build_env.ps1 first."
        exit 1
    }

    $toolset = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Recurse -Directory -Filter "PlatformToolsets" -ErrorAction SilentlyContinue |
        Get-ChildItem -Directory |
        Where-Object { $_.Name -match "^v\d+$" } |
        Sort-Object Name |
        Select-Object -Last 1 -ExpandProperty Name

    if (-not $toolset) {
        Write-Error "No MSVC platform toolset found. Run check_build_env.ps1 first."
        exit 1
    }

    $sdk = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Include" -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name |
        Select-Object -Last 1 -ExpandProperty Name

    if (-not $sdk) {
        Write-Error "Windows SDK not found. Run check_build_env.ps1 first."
        exit 1
    }

    $sln = Join-Path $PSScriptRoot "fileinfo.sln"

    Write-Host "  Configuration   : $Configuration|$Platform"
    Write-Host "  PlatformToolset : $toolset"
    Write-Host "  WindowsSDK      : $sdk"
    Write-Host ""

    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    & $msbuild $sln `
        /p:Configuration=$Configuration `
        /p:Platform=$Platform `
        /p:PlatformToolset=$toolset `
        /p:WindowsTargetPlatformVersion=$sdk `
        /m /v:m /nologo

    $code = $LASTEXITCODE
    $sw.Stop()
    $elapsed = $sw.Elapsed.ToString("mm\:ss")

    Write-Host ""
    if ($code -eq 0) {
        $outDir = if ($Platform -eq "x64") { "${Configuration}_x64" } else { $Configuration }
        $exe    = Join-Path $PSScriptRoot "$outDir\fileinfo.exe"
        $size   = if (Test-Path $exe) { "{0:N0} bytes" -f (Get-Item $exe).Length } else { "?" }
        Write-Host "Build succeeded in $elapsed -- $outDir\fileinfo.exe ($size)" -ForegroundColor Green

        # Copy runtime DLLs next to the executable
        $dllSrc = if ($Platform -eq "x64") { "3rdParty\exiv2\bin64" } else { "3rdParty\exiv2\bin" }
        $dllSrc = Join-Path $PSScriptRoot $dllSrc
        $dllDst = Join-Path $PSScriptRoot $outDir
        if (Test-Path $dllSrc) {
            $dlls = Get-ChildItem $dllSrc -Filter "*.dll"
            if ($dlls) {
                $dlls | ForEach-Object {
                    Copy-Item $_.FullName $dllDst -Force
                    Write-Host "  copied $($_.Name) -> $outDir\" -ForegroundColor DarkGray
                }
            }
        }
    } else {
        Write-Host "Build FAILED (exit $code) after $elapsed" -ForegroundColor Red
        exit $code
    }
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
foreach ($cmd in $Commands) {
    switch ($cmd.ToLower()) {
        "clean" { Invoke-Clean }
        "build" { Invoke-Build }
        default {
            Write-Error "Unknown command '$cmd'. Valid commands: clean, build"
            exit 1
        }
    }
}
