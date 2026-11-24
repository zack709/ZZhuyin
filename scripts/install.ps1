#Requires -RunAsAdministrator
param(
    [string]$BuildDir = "",
    [string]$BuildDirX86 = "$PSScriptRoot\..\build-release-x86",
    [string]$InstallDir = "$env:ProgramFiles\ZZhuyin",
    [string]$ZDictPath = ""
)

$ErrorActionPreference = "Stop"

if (-not $BuildDir) {
    if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64" -and (Test-Path "$PSScriptRoot\..\build-release-arm64\zzhuyin_tsf.dll")) {
        $BuildDir = "$PSScriptRoot\..\build-release-arm64"
    } else {
        $BuildDir = "$PSScriptRoot\..\build-release"
    }
}

function Install-DllWithHotSwap {
    param(
        [string]$SrcPath,
        [string]$DstPath,
        [string]$RegSvrPath
    )

    $dstDir = Split-Path $DstPath -Parent
    New-Item -ItemType Directory -Path $dstDir -Force | Out-Null

    Get-ChildItem -Path $dstDir -Filter "zzhuyin_tsf.dll.old.*" -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue

    if (Test-Path $DstPath) {
        Start-Process -FilePath $RegSvrPath -ArgumentList "/u", "/s", "`"$DstPath`"" -Wait | Out-Null
        Start-Sleep -Milliseconds 300

        try {
            Copy-Item $SrcPath $DstPath -Force -ErrorAction Stop
        } catch {
            $backup = "$DstPath.old.$(Get-Date -Format 'yyyyMMddHHmmss')"
            Write-Host "Existing DLL is in use; renaming to $(Split-Path $backup -Leaf) ..."
            Move-Item $DstPath $backup -Force
            Copy-Item $SrcPath $DstPath -Force
        }
    } else {
        Copy-Item $SrcPath $DstPath -Force
    }
}

$dllSrcX64 = Join-Path $BuildDir "zzhuyin_tsf.dll"
if (-not (Test-Path $dllSrcX64)) {
    throw "Cannot find $dllSrcX64. Please build Release x64 first."
}

$regsvr64 = Join-Path $env:WINDIR "System32\regsvr32.exe"
$regsvr32 = Join-Path $env:WINDIR "SysWOW64\regsvr32.exe"

New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

# 1. Copy dictionary
if ($ZDictPath -and (Test-Path $ZDictPath)) {
    Copy-Item $ZDictPath (Join-Path $InstallDir "zzhuyin.zdict") -Force
    Write-Host "Copied dictionary: $ZDictPath -> $InstallDir\zzhuyin.zdict"
} else {
    $localImd = Join-Path $PSScriptRoot "..\targets\DICTS\IMTCPH.IMD"
    if (Test-Path $localImd) {
        $dictsDir = Join-Path $InstallDir "DICTS"
        New-Item -ItemType Directory -Path $dictsDir -Force | Out-Null
        Copy-Item $localImd (Join-Path $dictsDir "IMTCPH.IMD") -Force
        foreach ($tbl in @("lcptr.tbl", "lcphrase.tbl")) {
            $tblSrc = Join-Path $PSScriptRoot "..\targets\$tbl"
            if (Test-Path $tblSrc) {
                Copy-Item $tblSrc (Join-Path $InstallDir $tbl) -Force
            }
        }
        Write-Host "Copied local compatibility dictionaries to $InstallDir"
    }
}

# 2. Install & register x64 DLL
$dllDstX64 = Join-Path $InstallDir "zzhuyin_tsf.dll"
Install-DllWithHotSwap -SrcPath $dllSrcX64 -DstPath $dllDstX64 -RegSvrPath $regsvr64

Write-Host "Registering x64: $dllDstX64 ..."
$proc64 = Start-Process -FilePath $regsvr64 -ArgumentList "/s", "`"$dllDstX64`"" -Wait -PassThru
if ($proc64.ExitCode -ne 0) {
    throw "x64 regsvr32 failed with exit code $($proc64.ExitCode)"
}

# 3. Install & register x86 DLL (for 32-bit applications on 64-bit Windows)
$dllSrcX86 = Join-Path $BuildDirX86 "zzhuyin_tsf.dll"
if ((Test-Path $dllSrcX86) -and (Test-Path $regsvr32)) {
    $dllDstX86 = Join-Path $InstallDir "x86\zzhuyin_tsf.dll"
    Install-DllWithHotSwap -SrcPath $dllSrcX86 -DstPath $dllDstX86 -RegSvrPath $regsvr32

    Write-Host "Registering x86: $dllDstX86 ..."
    $proc32 = Start-Process -FilePath $regsvr32 -ArgumentList "/s", "`"$dllDstX86`"" -Wait -PassThru
    if ($proc32.ExitCode -ne 0) {
        throw "x86 regsvr32 failed with exit code $($proc32.ExitCode)"
    }
}

# 4. Add ZZhuyin TIP to Windows 10/11 User Language List ("已安裝的鍵盤")
$tipId = "0404:{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A01}{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A02}"
$langList = Get-WinUserLanguageList
$zhLang = $langList | Where-Object { $_.LanguageTag -like "zh*" } | Select-Object -First 1
if ($zhLang -and -not ($zhLang.InputMethodTips -contains $tipId)) {
    $zhLang.InputMethodTips.Add($tipId)
    Set-WinUserLanguageList $langList -Force -WarningAction SilentlyContinue
}

Write-Host "Z注音 installed and registered successfully (x64 + x86)!"
