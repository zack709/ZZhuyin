; ZZhuyin (Z注音) Inno Setup Installer Script
; Supports building separate x64, arm64, and x86 installers via /DArch=x64, /DArch=arm64, or /DArch=x86
;
; Example:
;   iscc /DArch=x64 /DAppVersion=0.1.3 /DBuildDir=..\build-release /DZDictFile=..\dist\zzhuyin.zdict /O..\dist installer\zzhuyin.iss
;   iscc /DArch=arm64 /DAppVersion=0.1.3 /DBuildDir=..\build-release-arm64 /DZDictFile=..\dist\zzhuyin.zdict /O..\dist installer\zzhuyin.iss
;   iscc /DArch=x86 /DAppVersion=0.1.3 /DBuildDir=..\build-release-x86 /DZDictFile=..\dist\zzhuyin.zdict /O..\dist installer\zzhuyin.iss

#ifndef Arch
  #define Arch "x64"
#endif

#ifndef AppVersion
  #define AppVersion "0.1.3"
#endif

#ifndef BuildDir
  #if Arch == "x64"
    #define BuildDir "..\build-release"
  #elif Arch == "arm64"
    #define BuildDir "..\build-release-arm64"
  #else
    #define BuildDir "..\build-release-x86"
  #endif
#endif

#ifndef ZDictFile
  #define ZDictFile "..\dist\zzhuyin.zdict"
#endif

#define AppName "Z注音"
#define AppPublisher "Buglife Corporation"
#define AppURL "https://github.com/zack709/ZZhuyin"
#define TipId "0404:{{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A01}{{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A02}"

[Setup]
#if Arch == "x64"
AppId={{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A10}
AppName={#AppName} (x64)
AppVerName={#AppName} {#AppVersion} (x64)
DefaultDirName={autopf}\ZZhuyin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=ZZhuyin-Setup-{#AppVersion}-x64
#elif Arch == "arm64"
AppId={{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A12}
AppName={#AppName} (ARM64)
AppVerName={#AppName} {#AppVersion} (ARM64)
DefaultDirName={autopf}\ZZhuyin
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
OutputBaseFilename=ZZhuyin-Setup-{#AppVersion}-arm64
#else
AppId={{8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A11}
AppName={#AppName} (x86)
AppVerName={#AppName} {#AppVersion} (x86)
DefaultDirName={commonpf32}\ZZhuyin
ArchitecturesAllowed=x86compatible
OutputBaseFilename=ZZhuyin-Setup-{#AppVersion}-x86
#endif

AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}/releases
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
PrivilegesRequired=admin
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName} ({#Arch})
CloseApplications=no

[Languages]
Name: "default"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#BuildDir}\zzhuyin_tsf.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#ZDictFile}"; DestDir: "{app}"; DestName: "zzhuyin.zdict"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; Register TSF COM server (Inno Setup automatically routes {sys} to System32 for x64 or SysWOW64 for x86)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\zzhuyin_tsf.dll"""; StatusMsg: "正在註冊 Z注音 輸入法元件..."; Flags: runhidden waituntilterminated
; Add ZZhuyin TIP to the logged-in user's Windows 10/11 keyboard list
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""$t='{#TipId}'; $l=Get-WinUserLanguageList; $z=$l | Where-Object {{$_.LanguageTag -like 'zh*'}} | Select-Object -First 1; if ($z -and -not ($z.InputMethodTips -contains $t)) {{ $z.InputMethodTips.Add($t); Set-WinUserLanguageList $l -Force -WarningAction SilentlyContinue }}"""; StatusMsg: "正在將 Z注音 加入系統鍵盤清單..."; Flags: runhidden waituntilterminated runasoriginaluser

[UninstallRun]
; Remove ZZhuyin TIP from the user's keyboard list
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""$t='{#TipId}'; $l=Get-WinUserLanguageList; $z=$l | Where-Object {{$_.LanguageTag -like 'zh*'}} | Select-Object -First 1; if ($z -and ($z.InputMethodTips -contains $t)) {{ [void]$z.InputMethodTips.Remove($t); Set-WinUserLanguageList $l -Force -WarningAction SilentlyContinue }}"""; RunOnceId: "RemoveZZhuyinTip"; Flags: runhidden waituntilterminated
; Unregister TSF COM server
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\zzhuyin_tsf.dll"""; RunOnceId: "UnregZZhuyinDll"; Flags: runhidden waituntilterminated
