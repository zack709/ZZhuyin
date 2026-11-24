# Z注音（ZZhuyin）

在 **Windows 10 / 11（x64 / ARM64 / x86）** 上以 **TSF（Text Services Framework）** 復刻 **Windows 7 傳統注音（「ㄅ半」）** 手感與規則的繁體中文輸入法。

本專案採 **純 C++20 核心邏輯層**（零 Win32 / UI 相依）與 **薄型 TSF COM 外殼層** 分離設計，提供流暢穩定的傳統單字注音體驗。

---

## 核心特色

- **純靜態固定候選排序**：完全不啟用動態詞頻學習（MRU）或長句自動選字，選字順序永遠固定，保留最穩定的打字肌肉記憶。
- **4-Slot 亂序拼字與同類音覆蓋**：支援任意順序輸入聲母、介音、韻母與聲調，同槽位後鍵自動覆蓋前鍵；不合法注音組合時發出提示音並僅撤銷聲調，保留原聲／介／韻母。
- **流暢連續打字**：
  - 唯一候選字無條件自動上字。
  - 選字視窗開啟時，若未按數字鍵而直接繼續輸入下一個注音或標點，自動將目前反白字（預設 1 號字）上字並緊接著處理新按鍵。
- **四種內建鍵盤配置**：支援 **標準大千**、**倚天（ETen）**、**IBM**、**精業（Gin-Yieh）**，並於設定畫面提供即時 5 排注音鍵盤預覽圖。
- **經典 9 字分頁與相關字詞（聯想詞）**：
  - `1`～`9` 選字、`Space` / `PageDown` 循環翻頁、`PageUp` 不循環、`↑` / `↓` 首尾循環、`Esc` / `Backspace` 取消選字。
  - 啟用相關字詞時採 `Shift + 1`～`9` 或滑鼠點選，預設不自動預選第一項（未選取時按 `Enter` / `Backspace` 直接關閉聯想窗並放行按鍵），裸按 `1`～`9` 自動關閉聯想窗並開始新組字，不干擾大千鍵盤連續輸入。
- **完整快捷鍵與全形符號小鍵盤**：
  - 支援 **`Ctrl + Space`**、**`Left Shift` 短按（`< 401 ms`）** 或點擊系統匣 `ㄅ`／`英` 圖示切換中／英文（與 Windows `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 雙向同步）。
  - 支援 **`Ctrl + Alt + ,`** 開啟 47 鍵全形標點符號螢幕小鍵盤（支援滑鼠點選或實體鍵盤對應按鍵，選完自動關閉），以及 13 組 `Ctrl` / `Ctrl + Alt` 標點快捷鍵。
- **內嵌設定視窗與深淺主題**：
  - 對右下角系統匣 `ㄅ` 圖示按右鍵 → **「設定值」**（或於 Windows 設定 → 語言選項 → Z注音 → 鍵盤選項）即可開啟三頁籤設定畫面（一般／進階／鍵盤）。
  - 支援 **Dark / Light Theme** 即時切換（同步套用於設定視窗、字元集子視窗、候選字窗與符號小鍵盤）、**繁體字／简体字** 輸出字種切換、以及 **BIG5 / Unicode（Ext-A / Ext-B / HKSCS / CNS11643）** 字元集設定，設定自動儲存於 `HKCU\Software\ZZhuyin`。

---

## 安裝與移除

### 方式一：下載安裝檔（推薦）
前往 [GitHub Releases](https://github.com/zack709/ZZhuyin/releases) 依系統架構下載對應的安裝程式：
- 64 位元 Windows（x64）：`ZZhuyin-Setup-<版本>-x64.exe`
- Windows on ARM（ARM64）：`ZZhuyin-Setup-<版本>-arm64.exe`
- 32 位元 Windows（或 64 位元系統上需支援 32 位元程式）：`ZZhuyin-Setup-<版本>-x86.exe`

執行安裝程式後即可透過 `Win + Space` 切換至 **「Z注音」**。
需要移除時，請直接至 **Windows 設定 → 應用程式 → 已安裝的應用程式** 尋找 **「Z注音」** 並點選「解除安裝」。

### 方式二：本機手動建置安裝（開發者）
以 **系統管理員身分** 開啟 PowerShell：

```powershell
# 產生開源注音字表
python tools/build_zdict.py --fetch-chewing -o build-release/zzhuyin.zdict

# 安裝並註冊（x64 + x86）至 C:\Program Files\ZZhuyin
.\scripts\install.ps1 -ZDictPath .\build-release\zzhuyin.zdict

# 移除並解除註冊
.\scripts\uninstall.ps1
```

---

## 專案結構

```text
ZZhuyin/
├── CMakeLists.txt              # CMake 建置設定（Core、Tests、TSF DLL）
├── .github/workflows/
│   └── release.yml             # GitHub Actions 自動編譯 x64/arm64/x86 安裝檔與發佈 Release
├── installer/
│   └── zzhuyin.iss             # Inno Setup 安裝腳本（支援獨立產出 x64、arm64 與 x86）
├── src/
│   ├── core/                   # zzhuyin_core（純標準 C++20，零 Windows 相依）
│   │   ├── types.h             # 16-bit 注音編碼、事件與設定結構
│   │   ├── key_mapper.*        # 4 種鍵盤配置、標點快捷鍵與 47 鍵符號表
│   │   ├── composer.*          # 4-Slot 亂序組字與覆蓋
│   │   ├── candidate.*         # 9 字分頁與邊界循環邏輯
│   │   ├── zdict.*             # .zdict (ZZDICT01) 字典讀取器
│   │   └── engine.*            # 核心輸入狀態機
│   └── tsf/                    # zzhuyin_tsf.dll（Windows TSF COM 外殼）
│       ├── dll_main.cpp        # COM 工廠與 RegSvr32 註冊
│       ├── text_service.*      # ITfTextInputProcessorEx / KeyEventSink / CompartmentSink / FnConfigure
│       ├── candidate_wnd.*     # 高 DPI 雙緩衝浮動候選視窗（直／橫排、深淺主題）
│       ├── symbol_wnd.*        # Ctrl+Alt+, 全形標點符號螢幕小鍵盤（深淺主題）
│       ├── settings_dlg.*      # 內嵌三頁籤設定視窗（一般／進階／鍵盤、深淺主題）
│       ├── lang_bar.*          # 系統匣ㄅ／英圖示與右鍵「設定值」選單
│       ├── display_attr.*      # 組字虛線底線屬性
│       └── config_reg.*        # Registry 設定持久化
├── tests/                      # 單元測試與 TSF COM 煙霧測試
├── tools/
│   └── build_zdict.py          # .zdict 字表編譯打包工具（支援 libchewing-data 與 TSV）
└── scripts/
    ├── install.ps1             # 本機開發用安裝腳本
    └── uninstall.ps1           # 本機開發用移除腳本
```

---

## 建置與測試

### 環境需求
- Windows 10 / 11
- Visual Studio 2022（或 Visual Studio Build Tools 2022，含 MSVC C++20、CMake、Ninja、Windows SDK）
- Python 3.10+（打包字表用）
- Inno Setup 6（選配，於本機打包 `.exe` 安裝檔用）

### 編譯 Release x64、ARM64 與 x86

```bat
:: 建置 x64 Release 並執行測試
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

:: 建置 ARM64 Release（需於 Visual Studio Installer 勾選 MSVC C++ ARM64 建置工具）
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64_arm64
cmake -B build-release-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=ARM64
cmake --build build-release-arm64

:: 建置 x86 Release 並執行測試
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
cmake -B build-release-x86 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release-x86
ctest --test-dir build-release-x86 --output-on-failure
```

### 產生字表（`.zdict`）

```powershell
# 自動下載開源 libchewing-data 並打包為 zzhuyin.zdict
python tools/build_zdict.py --fetch-chewing -o build-release/zzhuyin.zdict

# 或從自訂 UTF-8 TSV 純文字字表編譯
# TSV 格式：<注音音節>\t<單一漢字>[\t<旗標hex，預設0x02 Big5>]
python tools/build_zdict.py --from-text words.tsv --assoc-text assoc.tsv -o build-release/zzhuyin.zdict
```

---

## 授權條款

- 本專案程式碼採用 **Apache License 2.0 + Commons Clause License Condition v1.0** 授權，詳見 [LICENSE](LICENSE)。
  你可以自由使用、修改與散佈本軟體，但不得將本軟體（或其衍生產品／服務）用於商業販售（Sell）。
- Release 安裝檔內建之開源字詞庫資料來自 [libchewing-data](https://codeberg.org/chewing/libchewing-data)（LGPL-2.1-or-later）。
