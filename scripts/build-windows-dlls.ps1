#requires -Version 5.1
<#
.SYNOPSIS
Builds pxlib.dll for 64-bit and 32-bit Windows.

.DESCRIPTION
This script validates and, when allowed, installs the Windows native build
dependencies needed by pxlib. It can build with either MSVC or an admin-free
portable MSYS2/MinGW toolchain, then configures, builds, tests, and verifies
both x64 and x86 DLLs.

The build intentionally disables GSF and iconv/recode, matching the small native
DLL expected by pypxlib's ctypes loader.

.EXAMPLE
powershell.exe -ExecutionPolicy Bypass -File .\scripts\build-windows-dlls.ps1

.EXAMPLE
powershell.exe -ExecutionPolicy Bypass -File .\scripts\build-windows-dlls.ps1 -SkipDependencyInstall

.EXAMPLE
powershell.exe -ExecutionPolicy Bypass -File .\scripts\build-windows-dlls.ps1 -Clean -CopyToPypxlib
#>

[CmdletBinding()]
param(
    [ValidateSet("auto", "msvc", "msys2")]
    [string]$Toolchain = "auto",

    [ValidateSet("both", "x64", "x86")]
    [string]$Architecture = "both",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [string]$BuildRoot,

    [string]$ArtifactsRoot,

    [switch]$Clean,

    [switch]$SkipTests,

    [switch]$SkipDependencyInstall,

    [switch]$CopyToPypxlib,

    [string]$VsBootstrapperUrl = "https://aka.ms/vs/18/release/vs_buildtools.exe",

    [string]$Msys2Root,

    [string]$Msys2ArchiveUrl = "https://repo.msys2.org/distrib/msys2-x86_64-latest.sfx.exe"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $ScriptDir "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $RepoRoot "build\windows"
}
if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {
    $ArtifactsRoot = Join-Path $RepoRoot "artifacts\windows"
}
if ([string]::IsNullOrWhiteSpace($Msys2Root)) {
    $Msys2Root = Join-Path $RepoRoot ".deps\msys64"
}

$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$ArtifactsRoot = [System.IO.Path]::GetFullPath($ArtifactsRoot)
$Msys2Root = [System.IO.Path]::GetFullPath($Msys2Root)
$RequiredExports = @("PX_retrieve_records", "PX_free_record", "PX_free_records")

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "[pxlib] $Message" -ForegroundColor Cyan
}

function Quote-CmdArg {
    param([Parameter(Mandatory = $true)][string]$Value)
    return '"' + $Value.Replace('"', '\"') + '"'
}

function Quote-BashArg {
    param([Parameter(Mandatory = $true)][string]$Value)
    return "'" + $Value.Replace("'", "'\''") + "'"
}

function ConvertTo-MsysPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath -notmatch "^([A-Za-z]):\\(.*)$") {
        throw "Cannot convert '$fullPath' to an MSYS2 path."
    }

    $drive = $Matches[1].ToLowerInvariant()
    $tail = $Matches[2].Replace("\", "/")
    return "/$drive/$tail"
}

function Assert-LastExitCode {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [int[]]$AllowedExitCodes = @(0)
    )
    if ($AllowedExitCodes -notcontains $LASTEXITCODE) {
        throw "$Message failed with exit code $LASTEXITCODE."
    }
}

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-PathUnder {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\')
    $comparison = [System.StringComparison]::OrdinalIgnoreCase

    if (-not ($fullPath.Equals($fullParent, $comparison) -or $fullPath.StartsWith($fullParent + "\", $comparison))) {
        throw "Refusing to operate on '$fullPath' because it is outside '$fullParent'."
    }
}

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [int[]]$AllowedExitCodes = @(0)
    )

    Write-Host ("> " + (Quote-CmdArg $FilePath) + " " + ($Arguments -join " "))
    $output = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }
    if ($AllowedExitCodes -notcontains $exitCode) {
        throw "'$FilePath' failed with exit code $exitCode."
    }
}

function Get-VsWherePath {
    $candidates = @()
    if (${env:ProgramFiles(x86)}) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe")
    }

    $fromPath = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        $candidates += $fromPath.Path
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Get-VisualStudioInstance {
    param([string[]]$Requires = @())

    $vswhere = Get-VsWherePath
    if (-not $vswhere) {
        return $null
    }

    $args = @("-latest", "-products", "*", "-format", "json")
    foreach ($requirement in $Requires) {
        $args += @("-requires", $requirement)
    }

    $json = & $vswhere @args
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($json -join "`n"))) {
        return $null
    }

    $instances = ($json -join "`n") | ConvertFrom-Json
    if ($null -eq $instances) {
        return $null
    }
    if ($instances -is [array]) {
        if ($instances.Count -eq 0) {
            return $null
        }
        return $instances[0]
    }

    return $instances
}

function Get-PreferredVisualStudioInstance {
    $instance = Get-VisualStudioInstance -Requires @("Microsoft.VisualStudio.Component.VC.Tools.x86.x64")
    if ($null -ne $instance) {
        return $instance
    }

    return Get-VisualStudioInstance -Requires @("Microsoft.VisualStudio.Workload.VCTools")
}

function Get-CMakePath {
    param($VisualStudioInstance)

    $fromPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Path
    }

    if ($null -ne $VisualStudioInstance) {
        $candidate = Join-Path $VisualStudioInstance.installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Test-CMakeVersion {
    param([Parameter(Mandatory = $true)][string]$CMakePath)

    $versionText = & $CMakePath --version
    if ($LASTEXITCODE -ne 0 -or -not $versionText) {
        return $false
    }

    $firstLine = $versionText[0]
    if ($firstLine -notmatch "cmake version ([0-9]+\.[0-9]+(\.[0-9]+)?)") {
        return $false
    }

    return ([version]$Matches[1] -ge [version]"3.12.0")
}

function Get-WindowsSdkState {
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"
    $state = [ordered]@{
        Root = $sdkRoot
        HasWindowsHeader = $false
        HasKernel32X64 = $false
        HasKernel32X86 = $false
        HasRcX64 = $false
        HasRcX86 = $false
    }

    if (-not (Test-Path -LiteralPath $sdkRoot)) {
        return [pscustomobject]$state
    }

    $includeRoot = Join-Path $sdkRoot "Include"
    if (Test-Path -LiteralPath $includeRoot) {
        $state.HasWindowsHeader = [bool](Get-ChildItem -LiteralPath $includeRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "um\windows.h") } |
            Select-Object -First 1)
    }

    $libRoot = Join-Path $sdkRoot "Lib"
    if (Test-Path -LiteralPath $libRoot) {
        $state.HasKernel32X64 = [bool](Get-ChildItem -LiteralPath $libRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "um\x64\kernel32.lib") } |
            Select-Object -First 1)
        $state.HasKernel32X86 = [bool](Get-ChildItem -LiteralPath $libRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "um\x86\kernel32.lib") } |
            Select-Object -First 1)
    }

    $binRoot = Join-Path $sdkRoot "bin"
    if (Test-Path -LiteralPath $binRoot) {
        $state.HasRcX64 = [bool](Get-ChildItem -LiteralPath $binRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "x64\rc.exe") } |
            Select-Object -First 1)
        $state.HasRcX86 = [bool](Get-ChildItem -LiteralPath $binRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "x86\rc.exe") } |
            Select-Object -First 1)
    }

    return [pscustomobject]$state
}

function Invoke-VsDevCommand {
    param(
        [Parameter(Mandatory = $true)]$VisualStudioInstance,
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture,
        [Parameter(Mandatory = $true)][string]$Command,
        [switch]$CaptureOutput
    )

    $vcvarsall = Join-Path $VisualStudioInstance.installationPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path -LiteralPath $vcvarsall)) {
        throw "vcvarsall.bat was not found at '$vcvarsall'."
    }

    $cmdLine = (Quote-CmdArg $vcvarsall) + " $TargetArchitecture >nul && $Command"
    Write-Host "> vcvarsall $TargetArchitecture && $Command"

    if ($CaptureOutput) {
        $output = & $env:ComSpec /d /s /c $cmdLine 2>&1
        if ($LASTEXITCODE -ne 0) {
            $output | Write-Host
            throw "Command failed in the $TargetArchitecture developer environment with exit code $LASTEXITCODE."
        }
        return $output
    }

    $output = & $env:ComSpec /d /s /c $cmdLine 2>&1
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "Command failed in the $TargetArchitecture developer environment with exit code $exitCode."
    }
}

function Test-VsDevTools {
    param(
        [Parameter(Mandatory = $true)]$VisualStudioInstance,
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture
    )

    try {
        Invoke-VsDevCommand -VisualStudioInstance $VisualStudioInstance -TargetArchitecture $TargetArchitecture -Command "where cl >nul && where link >nul && where nmake >nul && where rc >nul && where dumpbin >nul" -CaptureOutput | Out-Null
        return $true
    } catch {
        return $false
    }
}

function Get-MissingDependencies {
    $missing = New-Object System.Collections.Generic.List[string]
    $vs = Get-PreferredVisualStudioInstance
    $cmake = Get-CMakePath -VisualStudioInstance $vs
    $sdk = Get-WindowsSdkState

    if ($null -eq $vs) {
        $missing.Add("Visual Studio Build Tools with the C++ x64/x86 toolchain")
    } else {
        $vcvarsall = Join-Path $vs.installationPath "VC\Auxiliary\Build\vcvarsall.bat"
        if (-not (Test-Path -LiteralPath $vcvarsall)) {
            $missing.Add("Visual Studio vcvarsall.bat")
        } else {
            if (-not (Test-VsDevTools -VisualStudioInstance $vs -TargetArchitecture "x64")) {
                $missing.Add("x64 MSVC developer tools: cl, link, nmake, rc, dumpbin")
            }
            if (-not (Test-VsDevTools -VisualStudioInstance $vs -TargetArchitecture "x86")) {
                $missing.Add("x86 MSVC developer tools: cl, link, nmake, rc, dumpbin")
            }
        }
    }

    if (-not $cmake) {
        $missing.Add("CMake 3.12 or newer")
    } elseif (-not (Test-CMakeVersion -CMakePath $cmake)) {
        $missing.Add("CMake 3.12 or newer")
    }

    if (-not ($sdk.HasWindowsHeader -and $sdk.HasKernel32X64 -and $sdk.HasKernel32X86 -and $sdk.HasRcX64 -and $sdk.HasRcX86)) {
        $missing.Add("Windows SDK headers, x64/x86 import libraries, and resource compiler")
    }

    return [pscustomobject]@{
        VisualStudio = $vs
        CMake = $cmake
        WindowsSdk = $sdk
        Missing = $missing
    }
}

function Install-VisualStudioDependencies {
    param($CurrentVisualStudioInstance)

    if (-not (Test-Administrator)) {
        throw "Dependency installation needs an elevated PowerShell. Re-run as Administrator, or install the VS Build Tools dependencies yourself and use -SkipDependencyInstall."
    }

    $components = @(
        "Microsoft.VisualStudio.Workload.VCTools",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "Microsoft.VisualStudio.Component.VC.CMake.Project",
        "Microsoft.VisualStudio.Component.Windows11SDK.26100",
        "Microsoft.Component.VC.Runtime.UCRTSDK"
    )

    $commonArgs = @("--quiet", "--wait", "--norestart")
    foreach ($component in $components) {
        $commonArgs += @("--add", $component)
    }

    if ($null -ne $CurrentVisualStudioInstance) {
        $setup = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\setup.exe"
        if (-not (Test-Path -LiteralPath $setup)) {
            throw "Visual Studio Installer was not found at '$setup'."
        }

        Write-Step "Repairing Visual Studio Build Tools dependencies"
        $args = @("modify", "--installPath", $CurrentVisualStudioInstance.installationPath) + $commonArgs
        Invoke-Native -FilePath $setup -Arguments $args -AllowedExitCodes @(0, 3010)
        return
    }

    $dependencyDir = Join-Path $RepoRoot ".deps"
    New-Item -ItemType Directory -Force -Path $dependencyDir | Out-Null

    $bootstrapper = Join-Path $dependencyDir "vs_buildtools.exe"
    Write-Step "Downloading Visual Studio Build Tools bootstrapper"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $VsBootstrapperUrl -OutFile $bootstrapper

    Write-Step "Installing Visual Studio Build Tools dependencies"
    Invoke-Native -FilePath $bootstrapper -Arguments $commonArgs -AllowedExitCodes @(0, 3010)
}

function Ensure-MsvcDependencies {
    Write-Step "Validating build dependencies"
    $state = Get-MissingDependencies
    if ($state.Missing.Count -eq 0) {
        Write-Step "All build dependencies are present"
        return $state
    }

    Write-Warning ("Missing dependencies: " + ($state.Missing -join "; "))
    if ($SkipDependencyInstall) {
        throw "Dependency validation failed and -SkipDependencyInstall was set."
    }

    Install-VisualStudioDependencies -CurrentVisualStudioInstance $state.VisualStudio

    Write-Step "Re-validating build dependencies"
    $state = Get-MissingDependencies
    if ($state.Missing.Count -ne 0) {
        throw "Dependencies are still missing after installation: $($state.Missing -join '; ')"
    }

    return $state
}

function Assert-SourceTree {
    foreach ($path in @("CMakeLists.txt", "include\paradox.h.in", "src\paradox.c", "tests\fast_read.c")) {
        $fullPath = Join-Path $RepoRoot $path
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "Required source file '$path' was not found."
        }
    }
}

function Get-ArchitecturesToBuild {
    if ($Architecture -eq "both") {
        return @("x64", "x86")
    }

    return @($Architecture)
}

function Copy-BuildArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$TargetArchitecture,
        [Parameter(Mandatory = $true)][string]$BuildDir
    )

    $dll = Join-Path $BuildDir "pxlib.dll"
    if (-not (Test-Path -LiteralPath $dll)) {
        throw "Expected DLL was not produced: '$dll'."
    }

    $artifactDir = Join-Path $ArtifactsRoot $TargetArchitecture
    New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
    Copy-Item -LiteralPath $dll -Destination (Join-Path $artifactDir "pxlib.dll") -Force

    foreach ($extension in @("lib", "exp", "pdb")) {
        $candidate = Join-Path $BuildDir ("pxlib.$extension")
        if (Test-Path -LiteralPath $candidate) {
            Copy-Item -LiteralPath $candidate -Destination (Join-Path $artifactDir ("pxlib.$extension")) -Force
        }
    }

    foreach ($candidateName in @("pxlib.dll.a", "libpxlib.dll.a")) {
        $candidate = Join-Path $BuildDir $candidateName
        if (Test-Path -LiteralPath $candidate) {
            Copy-Item -LiteralPath $candidate -Destination (Join-Path $artifactDir $candidateName) -Force
        }
    }

    return (Join-Path $artifactDir "pxlib.dll")
}

function Test-DllMachineType {
    param(
        [Parameter(Mandatory = $true)]$VisualStudioInstance,
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture,
        [Parameter(Mandatory = $true)][string]$DllPath
    )

    $headers = Invoke-VsDevCommand -VisualStudioInstance $VisualStudioInstance -TargetArchitecture $TargetArchitecture -Command ("dumpbin /headers " + (Quote-CmdArg $DllPath)) -CaptureOutput
    $headersText = $headers -join "`n"

    if ($TargetArchitecture -eq "x64") {
        if ($headersText -notmatch "8664 machine \(x64\)") {
            throw "DLL '$DllPath' is not an x64 PE image."
        }
    } else {
        if ($headersText -notmatch "14C machine \(x86\)") {
            throw "DLL '$DllPath' is not an x86 PE image."
        }
    }
}

function Test-DllExports {
    param(
        [Parameter(Mandatory = $true)]$VisualStudioInstance,
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture,
        [Parameter(Mandatory = $true)][string]$DllPath
    )

    $exports = Invoke-VsDevCommand -VisualStudioInstance $VisualStudioInstance -TargetArchitecture $TargetArchitecture -Command ("dumpbin /exports " + (Quote-CmdArg $DllPath)) -CaptureOutput
    $exportsText = $exports -join "`n"

    foreach ($export in $RequiredExports) {
        if ($exportsText -notmatch ("(?m)\b" + [regex]::Escape($export) + "\b")) {
            throw "DLL '$DllPath' does not export '$export'."
        }
    }
}

function Invoke-PxlibBuild {
    param(
        [Parameter(Mandatory = $true)]$DependencyState,
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture
    )

    $buildDir = Join-Path $BuildRoot $TargetArchitecture
    Assert-PathUnder -Path $buildDir -Parent $RepoRoot

    if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
        Write-Step "Removing previous $TargetArchitecture build directory"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    $cmake = $DependencyState.CMake
    $configureCommand = @(
        (Quote-CmdArg $cmake),
        "-S", (Quote-CmdArg $RepoRoot),
        "-B", (Quote-CmdArg $buildDir),
        "-G", (Quote-CmdArg "NMake Makefiles"),
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DBUILD_TESTING=ON",
        "-DENABLE_GSF=OFF",
        "-DCMAKE_DISABLE_FIND_PACKAGE_Iconv=ON"
    ) -join " "

    Write-Step "Configuring pxlib for $TargetArchitecture"
    Invoke-VsDevCommand -VisualStudioInstance $DependencyState.VisualStudio -TargetArchitecture $TargetArchitecture -Command $configureCommand

    $buildCommand = (Quote-CmdArg $cmake) + " --build " + (Quote-CmdArg $buildDir)
    Write-Step "Building pxlib for $TargetArchitecture"
    Invoke-VsDevCommand -VisualStudioInstance $DependencyState.VisualStudio -TargetArchitecture $TargetArchitecture -Command $buildCommand

    if (-not $SkipTests) {
        $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
        if (-not (Test-Path -LiteralPath $ctest)) {
            $ctest = "ctest"
        }

        $testCommand = (Quote-CmdArg $ctest) + " --test-dir " + (Quote-CmdArg $buildDir) + " --output-on-failure"
        Write-Step "Running CTest for $TargetArchitecture"
        Invoke-VsDevCommand -VisualStudioInstance $DependencyState.VisualStudio -TargetArchitecture $TargetArchitecture -Command $testCommand
    }

    Write-Step "Collecting $TargetArchitecture artifacts"
    $artifactDll = Copy-BuildArtifacts -TargetArchitecture $TargetArchitecture -BuildDir $buildDir

    Write-Step "Verifying $TargetArchitecture DLL"
    Test-DllMachineType -VisualStudioInstance $DependencyState.VisualStudio -TargetArchitecture $TargetArchitecture -DllPath $artifactDll
    Test-DllExports -VisualStudioInstance $DependencyState.VisualStudio -TargetArchitecture $TargetArchitecture -DllPath $artifactDll

    return $artifactDll
}

function Get-Msys2BashPath {
    return (Join-Path $Msys2Root "usr\bin\bash.exe")
}

function Invoke-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("MSYS", "MINGW64", "MINGW32")][string]$Msystem,
        [Parameter(Mandatory = $true)][string]$Command,
        [switch]$CaptureOutput,
        [int[]]$AllowedExitCodes = @(0)
    )

    $bash = Get-Msys2BashPath
    if (-not (Test-Path -LiteralPath $bash)) {
        throw "MSYS2 bash was not found at '$bash'."
    }

    Write-Host "> MSYSTEM=$Msystem bash -lc $Command"

    $oldMsystem = $env:MSYSTEM
    $oldChereInvoking = $env:CHERE_INVOKING
    $oldPathType = $env:MSYS2_PATH_TYPE

    try {
        $env:MSYSTEM = $Msystem
        $env:CHERE_INVOKING = "1"
        $env:MSYS2_PATH_TYPE = "minimal"

        if ($CaptureOutput) {
            $output = & $bash -lc $Command 2>&1
            if ($AllowedExitCodes -notcontains $LASTEXITCODE) {
                $output | Write-Host
                throw "MSYS2 command failed in $Msystem with exit code $LASTEXITCODE."
            }
            return $output
        }

        $output = & $bash -lc $Command 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        if ($AllowedExitCodes -notcontains $exitCode) {
            throw "MSYS2 command failed in $Msystem with exit code $exitCode."
        }
    } finally {
        if ($null -eq $oldMsystem) { Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue } else { $env:MSYSTEM = $oldMsystem }
        if ($null -eq $oldChereInvoking) { Remove-Item Env:CHERE_INVOKING -ErrorAction SilentlyContinue } else { $env:CHERE_INVOKING = $oldChereInvoking }
        if ($null -eq $oldPathType) { Remove-Item Env:MSYS2_PATH_TYPE -ErrorAction SilentlyContinue } else { $env:MSYS2_PATH_TYPE = $oldPathType }
    }
}

function Test-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("MSYS", "MINGW64", "MINGW32")][string]$Msystem,
        [Parameter(Mandatory = $true)][string]$Command
    )

    try {
        Invoke-Msys2Command -Msystem $Msystem -Command $Command -CaptureOutput | Out-Null
        return $true
    } catch {
        return $false
    }
}

function Install-Msys2Base {
    $bash = Get-Msys2BashPath
    if (Test-Path -LiteralPath $bash) {
        return
    }

    if ((Test-Path -LiteralPath $Msys2Root) -and -not $Clean) {
        throw "MSYS2 root '$Msys2Root' exists but bash.exe is missing. Re-run with -Clean to replace it."
    }

    if ($SkipDependencyInstall) {
        throw "MSYS2 is missing and -SkipDependencyInstall was set."
    }

    if (Test-Path -LiteralPath $Msys2Root) {
        Assert-PathUnder -Path $Msys2Root -Parent $RepoRoot
        Remove-Item -LiteralPath $Msys2Root -Recurse -Force
    }

    $dependencyDir = Split-Path -Parent $Msys2Root
    Assert-PathUnder -Path $dependencyDir -Parent $RepoRoot
    New-Item -ItemType Directory -Force -Path $dependencyDir | Out-Null

    $archive = Join-Path $dependencyDir "msys2-x86_64-latest.sfx.exe"
    Write-Step "Downloading portable MSYS2"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $Msys2ArchiveUrl -OutFile $archive

    Write-Step "Extracting portable MSYS2"
    Invoke-Native -FilePath $archive -Arguments @("-y", "-o$dependencyDir")

    if (-not (Test-Path -LiteralPath $bash)) {
        throw "MSYS2 extraction completed, but bash.exe was not found at '$bash'."
    }
}

function Get-Msys2MissingDependencies {
    $missing = New-Object System.Collections.Generic.List[string]
    $bash = Get-Msys2BashPath

    if (-not (Test-Path -LiteralPath $bash)) {
        $missing.Add("portable MSYS2 base")
        return $missing
    }

    Invoke-Msys2Command -Msystem "MINGW64" -Command "true" -AllowedExitCodes @(0, 1) | Out-Null
    Invoke-Msys2Command -Msystem "MINGW32" -Command "true" -AllowedExitCodes @(0, 1) | Out-Null

    $toolProbe = "command -v gcc >/dev/null && command -v cmake >/dev/null && command -v ninja >/dev/null && command -v objdump >/dev/null"
    if (-not (Test-Msys2Command -Msystem "MINGW64" -Command $toolProbe)) {
        $missing.Add("MSYS2 mingw64 packages: gcc, cmake, ninja, binutils")
    }
    if (-not (Test-Msys2Command -Msystem "MINGW32" -Command $toolProbe)) {
        $missing.Add("MSYS2 mingw32 packages: gcc, cmake, ninja, binutils")
    }

    return $missing
}

function Install-Msys2Packages {
    Install-Msys2Base

    if ($SkipDependencyInstall) {
        $missing = @(Get-Msys2MissingDependencies)
        if ($missing.Count -ne 0) {
            throw "MSYS2 packages are missing and -SkipDependencyInstall was set: $($missing -join '; ')"
        }
        return
    }

    Write-Step "Initializing MSYS2"
    Invoke-Msys2Command -Msystem "MSYS" -Command "true" -AllowedExitCodes @(0, 1)

    Write-Step "Updating MSYS2 package databases"
    Invoke-Msys2Command -Msystem "MSYS" -Command "pacman --noconfirm -Syuu"
    Invoke-Msys2Command -Msystem "MSYS" -Command "pacman --noconfirm -Syuu"

    $packages = @(
        "mingw-w64-x86_64-gcc",
        "mingw-w64-x86_64-cmake",
        "mingw-w64-x86_64-ninja",
        "mingw-w64-i686-gcc",
        "mingw-w64-i686-cmake",
        "mingw-w64-i686-ninja"
    )

    Write-Step "Installing MSYS2 mingw64/mingw32 build packages"
    Invoke-Msys2Command -Msystem "MSYS" -Command ("pacman --noconfirm --needed -S " + ($packages -join " "))

    Write-Step "Finalizing MSYS2 shell initialization"
    Invoke-Msys2Command -Msystem "MSYS" -Command "true" -AllowedExitCodes @(0, 1)
    Invoke-Msys2Command -Msystem "MINGW64" -Command "true" -AllowedExitCodes @(0, 1)
    Invoke-Msys2Command -Msystem "MINGW32" -Command "true" -AllowedExitCodes @(0, 1)
}

function Ensure-Msys2Dependencies {
    Write-Step "Validating portable MSYS2 dependencies"

    $missing = @(Get-Msys2MissingDependencies)
    if ($missing.Count -eq 0) {
        Write-Step "All portable MSYS2 dependencies are present"
        return [pscustomobject]@{ Toolchain = "msys2"; Root = $Msys2Root }
    }

    Write-Warning ("Missing MSYS2 dependencies: " + ($missing -join "; "))
    Install-Msys2Packages

    Write-Step "Re-validating portable MSYS2 dependencies"
    $missing = @(Get-Msys2MissingDependencies)
    if ($missing.Count -ne 0) {
        throw "MSYS2 dependencies are still missing after installation: $($missing -join '; ')"
    }

    return [pscustomobject]@{ Toolchain = "msys2"; Root = $Msys2Root }
}

function Get-Msys2BuildEnvironment {
    param([Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture)

    if ($TargetArchitecture -eq "x64") {
        return "MINGW64"
    }

    return "MINGW32"
}

function Test-DllWithMsys2 {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture,
        [Parameter(Mandatory = $true)][string]$DllPath
    )

    $msystem = Get-Msys2BuildEnvironment -TargetArchitecture $TargetArchitecture
    $dllMsysPath = ConvertTo-MsysPath $DllPath

    $headers = Invoke-Msys2Command -Msystem $msystem -Command ("objdump -f " + (Quote-BashArg $dllMsysPath)) -CaptureOutput
    $headersText = $headers -join "`n"

    if ($TargetArchitecture -eq "x64") {
        if ($headersText -notmatch "file format pei-x86-64") {
            throw "DLL '$DllPath' is not an x64 PE image."
        }
    } else {
        if ($headersText -notmatch "file format pei-i386") {
            throw "DLL '$DllPath' is not an x86 PE image."
        }
    }

    $exports = Invoke-Msys2Command -Msystem $msystem -Command ("objdump -p " + (Quote-BashArg $dllMsysPath)) -CaptureOutput
    $exportsText = $exports -join "`n"

    foreach ($export in $RequiredExports) {
        if ($exportsText -notmatch ("(?m)\b" + [regex]::Escape($export) + "\b")) {
            throw "DLL '$DllPath' does not export '$export'."
        }
    }
}

function Invoke-Msys2PxlibBuild {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("x64", "x86")][string]$TargetArchitecture
    )

    $buildDir = Join-Path $BuildRoot ("msys2-" + $TargetArchitecture)
    Assert-PathUnder -Path $buildDir -Parent $RepoRoot

    if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
        Write-Step "Removing previous MSYS2 $TargetArchitecture build directory"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    $sourceMsysPath = ConvertTo-MsysPath $RepoRoot
    $buildMsysPath = ConvertTo-MsysPath $buildDir
    $msystem = Get-Msys2BuildEnvironment -TargetArchitecture $TargetArchitecture

    $configureCommand = @(
        "cmake",
        "-S", (Quote-BashArg $sourceMsysPath),
        "-B", (Quote-BashArg $buildMsysPath),
        "-G", (Quote-BashArg "Ninja"),
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DBUILD_TESTING=ON",
        "-DENABLE_GSF=OFF",
        "-DCMAKE_DISABLE_FIND_PACKAGE_Iconv=ON",
        "-DCMAKE_SHARED_LINKER_FLAGS=-static-libgcc",
        "-DCMAKE_EXE_LINKER_FLAGS=-static-libgcc"
    ) -join " "

    Write-Step "Configuring pxlib with MSYS2 for $TargetArchitecture"
    Invoke-Msys2Command -Msystem $msystem -Command $configureCommand

    Write-Step "Building pxlib with MSYS2 for $TargetArchitecture"
    Invoke-Msys2Command -Msystem $msystem -Command ("cmake --build " + (Quote-BashArg $buildMsysPath))

    if (-not $SkipTests) {
        Write-Step "Running CTest with MSYS2 for $TargetArchitecture"
        Invoke-Msys2Command -Msystem $msystem -Command ("ctest --test-dir " + (Quote-BashArg $buildMsysPath) + " --output-on-failure")
    }

    Write-Step "Collecting MSYS2 $TargetArchitecture artifacts"
    $artifactDll = Copy-BuildArtifacts -TargetArchitecture $TargetArchitecture -BuildDir $buildDir

    Write-Step "Verifying MSYS2 $TargetArchitecture DLL"
    Test-DllWithMsys2 -TargetArchitecture $TargetArchitecture -DllPath $artifactDll

    return $artifactDll
}

function Copy-ToPypxlib {
    param([hashtable]$BuiltDlls)

    $targetDir = Join-Path $RepoRoot "..\pypxlib\pypxlib\pxlib_ctypes"
    $targetDir = [System.IO.Path]::GetFullPath($targetDir)
    if (-not (Test-Path -LiteralPath $targetDir)) {
        throw "pypxlib ctypes directory was not found at '$targetDir'."
    }

    if ($BuiltDlls.ContainsKey("x86")) {
        Copy-Item -LiteralPath $BuiltDlls["x86"] -Destination (Join-Path $targetDir "pxlib.dll") -Force
    }
    if ($BuiltDlls.ContainsKey("x64")) {
        Copy-Item -LiteralPath $BuiltDlls["x64"] -Destination (Join-Path $targetDir "pxlib_x64.dll") -Force
    }

    Write-Step "Copied DLLs into $targetDir"
}

Assert-SourceTree

Assert-PathUnder -Path $BuildRoot -Parent $RepoRoot
Assert-PathUnder -Path $ArtifactsRoot -Parent $RepoRoot
Assert-PathUnder -Path $Msys2Root -Parent $RepoRoot

$selectedToolchain = $Toolchain
$dependencyState = $null

if ($Toolchain -eq "auto") {
    Write-Step "Checking whether MSVC is ready"
    $msvcState = Get-MissingDependencies
    if ($msvcState.Missing.Count -eq 0) {
        $selectedToolchain = "msvc"
        $dependencyState = $msvcState
        Write-Step "Using MSVC toolchain"
    } else {
        $selectedToolchain = "msys2"
        Write-Warning ("MSVC is not ready (" + ($msvcState.Missing -join "; ") + "); falling back to portable MSYS2.")
        $dependencyState = Ensure-Msys2Dependencies
    }
} elseif ($Toolchain -eq "msvc") {
    Write-Step "Using MSVC toolchain"
    $dependencyState = Ensure-MsvcDependencies
} else {
    Write-Step "Using portable MSYS2 toolchain"
    $dependencyState = Ensure-Msys2Dependencies
}

$builtDlls = @{}

foreach ($targetArchitecture in (Get-ArchitecturesToBuild)) {
    if ($selectedToolchain -eq "msvc") {
        $builtDlls[$targetArchitecture] = Invoke-PxlibBuild -DependencyState $dependencyState -TargetArchitecture $targetArchitecture
    } else {
        $builtDlls[$targetArchitecture] = Invoke-Msys2PxlibBuild -TargetArchitecture $targetArchitecture
    }
}

if ($CopyToPypxlib) {
    Copy-ToPypxlib -BuiltDlls $builtDlls
}

Write-Step "Done"
foreach ($key in ($builtDlls.Keys | Sort-Object)) {
    Write-Host "$key DLL: $($builtDlls[$key])"
}
