# Changelog

本專案的所有重要變更都會記錄於此檔案。

---

## [0.1.3] - 2026-11-24

### 新增與改進
- **新增 ARM64（Windows on ARM）建置與安裝檔打包支援**：
  - 更新 [installer/zzhuyin.iss](installer/zzhuyin.iss) 新增 `/DArch=arm64` 分支，可獨立產出 `ZZhuyin-Setup-<版本>-arm64.exe`。
  - 更新 [.github/workflows/release.yml](.github/workflows/release.yml) 新增 `Build Release arm64`（`vcvarsall.bat amd64_arm64` 交叉編譯）與 `Package arm64 Installer`，於 Release 同步發佈 ARM64 安裝檔。
  - 更新 [scripts/install.ps1](scripts/install.ps1) 在 ARM64 環境下自動偵測 `build-release-arm64`。
- **相關字詞預設不自動選取第一項**：
  - 彈出相關字詞（聯想詞）視窗時預設不反白／不預選第 1 個字詞；若未先以 `↑` / `↓` 選取，直接按 `Enter` 或 `Backspace` 會關閉聯想視窗並放行按鍵，不會自動上字第 1 個相關字詞（仍可使用 `Shift + 1`～`9` 或滑鼠直接點選）。

---

## [0.1.2] - 2026-11-22

### 新增與改進
- **密碼欄位自動切換英文模式**：偵測 TSF 停用／空上下文（`GUID_COMPARTMENT_KEYBOARD_DISABLED` / `GUID_COMPARTMENT_EMPTYCONTEXT`）、`InputScope`（`IS_PASSWORD` / `IS_NUMERIC_PASSWORD` / `IS_ALPHANUMERIC_PIN` / `IS_NUMERIC_PIN`）以及 Win32 `Edit` / `RichEdit` 密碼控制項（`ES_PASSWORD` / `EM_GETPASSWORDCHAR`）；進入密碼欄位時自動切換為英文模式並放行按鍵，離開密碼欄位後自動恢復原本的輸入模式。
- **更新安裝程式發行者名稱**：將 Inno Setup 安裝程式 `AppPublisher` 更新為 `Buglife Corporation`。

---

## [0.1.1] - 2026-11-22

### 新增功能
- **內嵌三頁籤「Z注音 設定」視窗**（直接內建於 `zzhuyin_tsf.dll`，不需額外執行檔，x64 / x86 皆支援）：
  - **一般**：預設輸入模式、候選字窗字型大小（100%～200%）、候選字窗方向（直排／橫排）、外觀主題、提示相關字詞、輸出字種與字元集設定。
  - **進階**：左右 Shift 鍵切換輸入模式、標點符號輸入、警告嗶聲提示與還原全部設定為預設值。
  - **鍵盤**：支援標準大千／倚天／IBM／精業鍵盤切換，並內建即時 5 排注音鍵盤預覽圖。
- **輸出字種切換（繁體字 / 简体字）**：選取「简体字 (S)」時，透過 Windows 原生 `LCMapStringEx` 即時轉換候選字、預覽字與上字結果。
- **Dark / Light Theme（深色／淺色主題）**：可於設定畫面即時切換深色與淺色外觀，同步套用於設定視窗（含 Windows 11 深色標題列）、字元集子視窗與浮動候選字視窗。
- **改版「字元集設定」子視窗**：採 `BIG5 字元 (I)` / `Unicode 字元 (U)` 單選切換，自動連動啟用／停用 CJK 擴充 A／B 區選項，並提供 HKSCS 與 CNS11643 選項。
- **`Ctrl + Alt + ,` 全形標點符號螢幕小鍵盤**：完整支援 Windows 7 經典 47 鍵全形標點與製表符號，可用實體鍵盤對應按鍵或滑鼠直接點選輸入（選完符號後自動關閉視窗），支援視窗拖曳、高 DPI 與 Dark / Light Theme 同步切換，亦可按 `Esc`／`Ctrl + Alt + ,`／`✕` 關閉。
- **系統設定整合**：實作 `ITfFnConfigure`，可從 Windows 設定 → 語言選項 → Z注音 → 鍵盤選項直接開啟設定畫面。

### 外觀與互動調整
- **全新配色經典方框「ㄅ」圖示**：保留傳統方框 `ㄅ` 造型，改採深青外框＋冰白底＋深青綠粗體 `ㄅ`（英文模式為岩灰 `英`），並同步套用於設定視窗標題列。
- **精簡右鍵選單與定位修正**：對系統匣圖示按右鍵改為彈出單一「設定值」選項，並修正對齊於工作列上緣、不再遮擋圖示。
- **修正設定畫面控制項底色**：統一所有文字標籤、核取方塊與單選按鈕的背景色與分頁底色，消除灰色方塊。

---

## [0.1.0] - 2026-11-22

### 初始版本
- 實作純 C++20 核心引擎（`zzhuyin_core`）與 Windows TSF COM 外殼（`zzhuyin_tsf.dll`，支援 x64 / x86）。
- 完整復刻 Windows 7 傳統注音（「ㄅ半」）行為：固定候選字排序（無動態學習）、4-Slot 亂序拼字與同類音覆蓋、唯一候選字自動上字、未選字繼續打字自動上 1 號字、不合法組合提示音並僅撤銷聲調。
- 支援四種鍵盤配置（標準大千、倚天、IBM、精業）、9 字分頁、相關字詞（`Shift+1..9`）、`Ctrl+Space` 與 `Left Shift` 短按切換中／英文、13 組標點快捷鍵。
- 支援雙字表後端（獨立 `.zdict` 格式與 Windows 7 `IMTCPH.IMD` / `lcptr.tbl` / `lcphrase.tbl`）與 Registry 設定持久化。
- 提供 `tools/build_zdict.py` 字表打包工具與 `scripts/install.ps1` / `uninstall.ps1` 一鍵安裝腳本。
