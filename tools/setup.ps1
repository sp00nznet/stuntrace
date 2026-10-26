<#
Stunt Race FX recomp setup: from a plain download to a launcher.

    Setup.cmd                                          double-click; asks before installing anything
    powershell -File tools\setup.ps1 -Yes -Game D:\roms   no questions (recomp-launcher uses this)

It runs the README's "Step by step" commands, skips what is already done (so a rerun after a
failure resumes), and logs everything to logs\setup.log. Your ROM stays on this machine: it is
copied into the repo folder (gitignored). Nothing from the game is downloaded.
#>
param(
    [switch]$Yes,          # install missing prerequisites without asking
    [string]$Game = ""     # folder holding the ROM (default: the repo folder)
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
New-Item -ItemType Directory -Force logs, deps | Out-Null
$Log = Join-Path $Root "logs\setup.log"
"Stunt Race FX recomp setup, $(Get-Date)" | Out-File $Log -Encoding utf8

$RomName = "Stunt Race FX (USA).sfc"
$RomMd5 = "128B316A74CAF17FDC216B3AB46D4A9A"
# Used only for a plain download (no .git): must match the ext/snesrecomp submodule pin.
$SnesrecompRepo = "https://github.com/sp00nznet/snesrecomp.git"
$SnesrecompRef = "2583dd0b30d149e44315c4bf4915979b53389e42"
$Steps = 6

function Step($n, $what) { $line = "[$n/$Steps] $what"; Write-Host $line; $line | Out-File $Log -Append -Encoding utf8 }
function Stop-Setup($why) {
    Write-Host ""
    Write-Host "Setup stopped: $why" -ForegroundColor Red
    Write-Host "Details are in $Log"
    "Setup stopped: $why" | Out-File $Log -Append -Encoding utf8
    exit 1
}
# Runs a native command with its output in the log; stops with $why if it fails.
function Run($why, $exe, [string[]]$arguments) {
    "> $exe $($arguments -join ' ')" | Out-File $Log -Append -Encoding utf8
    # Windows PowerShell turns a native program's stderr into errors; only the exit code counts.
    $old = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $global:LASTEXITCODE = 0
    & $exe @arguments 2>&1 | ForEach-Object { "$_" } | Out-File $Log -Append -Encoding utf8
    $code = $LASTEXITCODE
    $ErrorActionPreference = $old
    if ($code -ne 0) { Stop-Setup $why }
}
function Ask($question) {
    if ($Yes) { return $true }
    return (Read-Host "$question [Y/n]") -notmatch '^[nN]'
}
function Refresh-Path {
    $env:PATH = [Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" + [Environment]::GetEnvironmentVariable("PATH", "User")
}
function Find-VS {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    return & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
function Md5($bytes) {
    $md5 = [Security.Cryptography.MD5]::Create()
    return ([BitConverter]::ToString($md5.ComputeHash($bytes)) -replace "-", "")
}
# A ROM image as bytes, minus a 512-byte copier header if it has one.
function Rom-Bytes([byte[]]$b) {
    if ($b.Length % 1024 -eq 512) { return $b[512..($b.Length - 1)] }
    return $b
}

# ---------------------------------------------------------------------------------------------
Step 1 "Checking the tools it needs"
$need = @()
if (-not (Get-Command git -ErrorAction SilentlyContinue))   { $need += @{ Name = "Git"; Id = "Git.Git"; Size = "about 300 MB" } }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $need += @{ Name = "CMake"; Id = "Kitware.CMake"; Size = "about 100 MB" } }
if (-not (Find-VS)) { $need += @{ Name = "Visual Studio 2022 Build Tools (C++)"; Id = "Microsoft.VisualStudio.2022.BuildTools"; Size = "about 3 GB";
    Extra = @("--override", "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --wait") } }
foreach ($t in $need) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Stop-Setup "$($t.Name) is missing and winget isn't available to install it. Install $($t.Name) yourself, then run Setup again."
    }
    if (-not (Ask "$($t.Name) is needed to build the game and isn't installed. Install it with winget ($($t.Size))?")) {
        Stop-Setup "$($t.Name) is needed. Install it, then run Setup again."
    }
    $args_ = @("install", "--id", $t.Id, "-e", "--accept-source-agreements", "--accept-package-agreements") + $t.Extra
    Run "installing $($t.Name) failed" "winget" $args_
    Refresh-Path
}
if (-not (Find-VS)) { Stop-Setup "Visual Studio's C++ build tools didn't install. Install the 'Desktop development with C++' workload, then run Setup again." }

# ---------------------------------------------------------------------------------------------
Step 2 "Finding your ROM"
$target = Join-Path $Root $RomName
if (-not ((Test-Path $target) -and (Md5 (Rom-Bytes ([IO.File]::ReadAllBytes($target)))) -eq $RomMd5)) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $dirs = @($Root) + $(if ($Game) { @($Game) } else { @() })
    $found = $null
    foreach ($dir in $dirs) {
        foreach ($f in Get-ChildItem -Path $dir -File -Include *.sfc, *.smc, *.zip -Recurse -Depth 1 -ErrorAction SilentlyContinue) {
            $images = @()
            if ($f.Extension -eq ".zip") {
                $zip = [IO.Compression.ZipFile]::OpenRead($f.FullName)
                foreach ($e in $zip.Entries | Where-Object { $_.Name -match '\.(sfc|smc)$' }) {
                    $ms = New-Object IO.MemoryStream
                    $s = $e.Open(); $s.CopyTo($ms); $s.Close()
                    $images += , $ms.ToArray()
                }
                $zip.Dispose()
            } else {
                $images += , [IO.File]::ReadAllBytes($f.FullName)
            }
            foreach ($img in $images) {
                $rom = Rom-Bytes $img
                if ((Md5 $rom) -eq $RomMd5) { [IO.File]::WriteAllBytes($target, $rom); $found = $f.FullName; break }
            }
            if ($found) { break }
        }
        if ($found) { break }
    }
    if (-not $found) {
        Stop-Setup "No Stunt Race FX (USA) ROM found. Put your .sfc, .smc or .zip in this folder (or pass -Game <folder>), then run Setup again."
    }
    Write-Host "  found $found"
}

# ---------------------------------------------------------------------------------------------
Step 3 "Getting snesrecomp (the SNES hardware library)"
if (-not (Test-Path "ext\snesrecomp\ext\LakeSnes\snes\gsu.c")) {
    if (Test-Path ".git") {
        Run "fetching the submodules failed (no internet, or see the log)" "git" @("submodule", "update", "--init", "--recursive")
    } else {
        if (Test-Path "ext\snesrecomp") { Remove-Item -Recurse -Force "ext\snesrecomp" }
        Run "downloading snesrecomp failed (no internet, or see the log)" "git" @("-c", "core.longpaths=true", "clone", $SnesrecompRepo, "ext\snesrecomp")
        Run "checking out snesrecomp $SnesrecompRef failed" "git" @("-C", "ext\snesrecomp", "checkout", $SnesrecompRef)
        Run "fetching snesrecomp's submodules failed" "git" @("-C", "ext\snesrecomp", "submodule", "update", "--init", "--recursive")
    }
}

# ---------------------------------------------------------------------------------------------
Step 4 "Getting SDL2 through vcpkg (first time: a few minutes)"
$vcpkg = Join-Path $Root "deps\vcpkg"
if (-not (Test-Path "$vcpkg\vcpkg.exe")) {
    if (-not (Test-Path "$vcpkg\bootstrap-vcpkg.bat")) {
        # a half-finished clone from an earlier run: start it again
        if (Test-Path $vcpkg) { Remove-Item -Recurse -Force $vcpkg }
        # core.longpaths: vcpkg's test ports are deeper than Windows' 260-character limit
        Run "downloading vcpkg failed (no internet, or see the log)" "git" @("-c", "core.longpaths=true", "clone", "https://github.com/microsoft/vcpkg", $vcpkg)
    }
    Run "setting up vcpkg failed" "$vcpkg\bootstrap-vcpkg.bat" @("-disableMetrics")
}
Run "installing SDL2 failed" "$vcpkg\vcpkg.exe" @("install", "sdl2:x64-windows")

# ---------------------------------------------------------------------------------------------
Step 5 "Building (about two minutes)"
Run "configuring the build failed" "cmake" @("-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64", "-DCMAKE_TOOLCHAIN_FILE=$vcpkg\scripts\buildsystems\vcpkg.cmake")
Run "building failed" "cmake" @("--build", "build", "--config", "Release", "--parallel")

# ---------------------------------------------------------------------------------------------
Step 6 "Making the launcher"
$launcher = Join-Path $Root "Stunt Race FX (recomp).cmd"
"@echo off`r`ncd /d `"%~dp0`"`r`nstart `"`" `"build\Release\srf_launcher.exe`" `"$RomName`" %*`r`n" | Out-File $launcher -Encoding ascii
Write-Host ""
Write-Host "Done. Double-click 'Stunt Race FX (recomp).cmd' to play."
