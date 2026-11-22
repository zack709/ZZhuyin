#Requires -RunAsAdministrator
param(
    [string]$InstallDir = "$env:ProgramFiles\ZZhuyin"
)

$ErrorActionPreference = "Stop"

# Remove ZZhuyin TIP from Windows 10/11 User Language List
$tipId = "0404:{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A01}{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A02}"
$langList = Get-WinUserLanguageList
$zhLang = $langList | Where-Object { $_.LanguageTag -like "zh*" } | Select-Object -First 1
if ($zhLang -and ($zhLang.InputMethodTips -contains $tipId)) {
    [void]$zhLang.InputMethodTips.Remove($tipId)
    Set-WinUserLanguageList $langList -Force -WarningAction SilentlyContinue
}

$regsvr64 = Join-Path $env:WINDIR "System32\regsvr32.exe"
$regsvr32 = Join-Path $env:WINDIR "SysWOW64\regsvr32.exe"

$dllDstX86 = Join-Path $InstallDir "x86\zzhuyin_tsf.dll"
if ((Test-Path $dllDstX86) -and (Test-Path $regsvr32)) {
    Write-Host "Unregistering x86: $dllDstX86 ..."
    $proc32 = Start-Process -FilePath $regsvr32 -ArgumentList "/u", "/s", "`"$dllDstX86`"" -Wait -PassThru
    if ($proc32.ExitCode -ne 0) {
        Write-Warning "x86 regsvr32 /u exited with code $($proc32.ExitCode)"
    }
}

$dllDstX64 = Join-Path $InstallDir "zzhuyin_tsf.dll"
if (Test-Path $dllDstX64) {
    Write-Host "Unregistering x64: $dllDstX64 ..."
    $proc64 = Start-Process -FilePath $regsvr64 -ArgumentList "/u", "/s", "`"$dllDstX64`"" -Wait -PassThru
    if ($proc64.ExitCode -ne 0) {
        Write-Warning "x64 regsvr32 /u exited with code $($proc64.ExitCode)"
    }
}

if (Test-Path $InstallDir) {
    Remove-Item $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Z注音 unregistered and removed."
