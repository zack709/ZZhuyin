# Z注音（ZZhuyin）

在 **Windows 10 / 11（x64 / x86）** 上以 **TSF（Text Services Framework）** 復刻 **Windows 7 傳統注音（「ㄅ半」）** 手感與規則的繁體中文輸入法。

本專案採 **純 C++20 核心邏輯層**（零 Win32 / UI 相依）與 **薄型 TSF COM 外殼層** 分離設計，完整還原由逆向分析確認的按鍵狀態機、注音編碼、候選字排序與操作細節。

---

## 核心特色

- **純靜態固定候選排序**：完全不啟用動態詞頻學習（MRU）或長句自動選字，選字順序永遠固定，保留最穩定的打字肌肉記憶。
- **4-Slot 亂序拼字與同類音覆蓋**：支援任意順序輸入聲母、介音、韻母與聲調，同槽位後鍵自動覆蓋前鍵；不合法注音組合時發出提示音並僅撤銷聲調，保留原聲／介／韻母。
- **流暢連續打字**：
  - 唯一候選字無條件自動上字。
  - 選字視窗開啟時，若未按數字鍵而直接繼續輸入下一個注音或標點，自動將目前反白字（預設 1 號字）上字並緊接著處理新按鍵。
- **四種內建鍵盤配置**：支援 **標準大千**、**倚天（ETen）**、**IBM**、**精業（Gin-Yieh）**。
- **經典 9 字分頁與相關字詞（聯想詞）**：
  - `1`～`9` 選字、`Space` / `PageDown` 循環翻頁、`PageUp` 不循環、`↑` / `↓` 首尾循環、`Esc` / `Backspace` 取消選字。
  - 啟用相關字詞時採 `Shift + 1`～`9` 選詞，裸按 `1`～`9` 自動關閉聯想窗並開始新組字，不干擾大千鍵盤連續輸入。
- **完整快捷鍵與中英切換**：
  - 支援 **`Ctrl + Space`**、**`Left Shift` 短按（`< 401 ms`）** 或點擊工作列系統匣圖示切換中／英文（與 Windows `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 雙向同步）。
  - 支援全／半形切換與 13 組 `Ctrl` / `Ctrl + Alt` 全形標點快捷鍵（如 `Ctrl+,` → `，`、`Ctrl+.` → `。`、`Ctrl+Alt+/` → `？` 等）。
- **高 DPI 候選窗與系統匣選單**：
  - 支援直排／橫排切換、滑鼠懸停與點擊選字、螢幕邊界防裁切與雙緩衝無閃爍繪製。
  - 右鍵點擊系統匣 `中`／`英` 圖示即可即時切換鍵盤配置、排列方向、相關字詞與字集過濾（Big5 / CJK Ext-A / CNS），設定自動儲存於 `HKCU\Software\ZZhuyin`。
- **雙字表後端**：支援獨立封裝的 `.zdict`（`ZZDICT01`）格式，亦相容直接讀取 Windows 7 原廠 `IMTCPH.IMD` + `lcptr.tbl` / `lcphrase.tbl`。

---

## 專案結構

```text
ZZhuyin/
├── CMakeLists.txt              # CMake 建置設定（Core、Tests、TSF DLL）
├── docs/                       # 逆向分析文件與完整實作規格書
│   ├── zzhuyin-spec.md         # ZZhuyin 核心規格與 TSF 實作藍圖
│   └── phase-1..4-*.md         # 各階段逆向報告
├── src/
│   ├── core/                   # zzhuyin_core（純標準 C++20，零 Windows 相依）
│   │   ├── types.h             # 16-bit 注音編碼、事件與設定結構
│   │   ├── key_mapper.*        # 4 種鍵盤配置與標點對照表
│   │   ├── composer.*          # 4-Slot 亂序組字與覆蓋
│   │   ├── candidate.*         # 9 字分頁與邊界循環邏輯
│   │   ├── imechlex_dict.*     # IMTCPH.IMD / LCPTR.TBL / LCPHRASE.TBL 讀取器
│   │   ├── zdict.*             # 獨立 .zdict (ZZDICT01) 讀取器
│   │   └── engine.*            # 核心輸入狀態機
│   └── tsf/                    # zzhuyin_tsf.dll（Windows TSF COM 外殼）
│       ├── dll_main.cpp        # COM 工廠與 RegSvr32 註冊（含現代 Windows Category）
│       ├── text_service.*      # ITfTextInputProcessorEx / KeyEventSink / CompartmentSink
│       ├── candidate_wnd.*     # 高 DPI 雙緩衝浮動候選視窗（直／橫排）
│       ├── lang_bar.*          # 系統匣中／英圖示與右鍵設定選單
│       ├── display_attr.*      # 組字虛線底線屬性
│       └── config_reg.*        # Registry 設定持久化
├── tests/                      # 單元測試與 TSF COM 煙霧測試
├── tools/
│   ├── parse_dict.py           # IMTCPH.IMD 查詢驗證腳本
│   └── build_zdict.py          # .zdict 字表編譯打包工具（支援 TSV 與 IMD 來源）
└── scripts/
    ├── install.ps1             # 一鍵安裝、註冊（x64 + x86）與掛載鍵盤清單
    └── uninstall.ps1           # 解除註冊與移除
```

---

## 建置與測試

### 環境需求
- Windows 10 / 11
- Visual Studio 2022（或 Visual Studio Build Tools 2022，含 MSVC C++20、CMake、Ninja、Windows SDK）
- Python 3.10+（僅在使用 `tools/build_zdict.py` 打包字表時需要）

### 編譯 Release x64 與 x86

於 **Developer Command Prompt for VS 2022**（或先執行 `vcvars64.bat` / `vcvars32.bat`）執行：

```bat
:: 建置 x64 Release 並執行測試
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

:: 建置 x86 Release（供 32 位元應用程式使用）並執行測試
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
cmake -B build-release-x86 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release-x86
ctest --test-dir build-release-x86 --output-on-failure
```

> 註：`tests/test_dictionary.cpp` 與 `tests/test_engine_e2e.cpp` 會讀取本機 `targets/` 內的 `IMTCPH.IMD`、`lcptr.tbl`、`lcphrase.tbl` 進行驗證；`test_zdict.cpp` 與 `test_candidate.cpp` 則不依賴外部字表即可執行。

---

## 產生字表（`.zdict`）

`tools/build_zdict.py` 支援兩種來源格式：

```powershell
# 方式 A：從 UTF-8 TSV 純文字字表建立獨立字表
# TSV 格式：<注音音節>\t<單一漢字>[\t<旗標hex，預設0x02 Big5>]
python tools/build_zdict.py --from-text words.tsv --assoc-text assoc.tsv -o build-release/zzhuyin.zdict

# 方式 B：從本機 Windows 7 字表打包為單一 .zdict
python tools/build_zdict.py `
  --from-imd targets/DICTS/IMTCPH.IMD `
  --lcptr targets/lcptr.tbl `
  --lcphrase targets/lcphrase.tbl `
  -o build-release/zzhuyin.zdict
```

---

## 安裝與移除

以 **系統管理員身分** 開啟 PowerShell：

```powershell
# 安裝並同時註冊 x64 與 x86 版本至 C:\Program Files\ZZhuyin
.\scripts\install.ps1 -ZDictPath .\build-release\zzhuyin.zdict

# 移除並解除註冊
.\scripts\uninstall.ps1
```

安裝完成後即可透過 `Win + Space` 切換至 **「Z注音」**。若要覆蓋更新正在被系統載入的 DLL，再次執行 `install.ps1` 即會自動熱替換（新開啟的應用程式會立即載入新版）。

---

## 規格與逆向文件

完整的狀態機規則、16-bit 注音編碼與二進位格式說明請參閱：
- [規格書與實作藍圖（zzhuyin-spec.md）](docs/zzhuyin-spec.md)
- [執行計畫（plan.md）](plan.md)

---

## 授權條款

本專案採用 **Apache License 2.0 + Commons Clause License Condition v1.0** 授權，詳見 [LICENSE](LICENSE)。
你可以自由使用、修改與散佈本軟體，但不得將本軟體（或其衍生產品／服務）用於商業販售（Sell）。
