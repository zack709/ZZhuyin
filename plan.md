# Windows 7 傳統注音（`phon.ime` + `IMTCCORE.DLL`）分階段 REA 逆向與 TSF 復刻計畫

本計畫將逆向工程拆分為多個**可獨立執行、可中斷續接**的階段，每個階段只聚焦少數關鍵函式與問題，並將結論落地到 `docs/` 下的檔案，避免單次對話消耗過多 token。

---

## 0. 已知前置資訊（已完成，後續階段直接沿用）

- **目標檔案位置**：`e:\Workspace\ZZhuyin\targets\`
  - `phon.ime`（175 KB, x64 PE32+）：Windows 7 傳統注音前端，**IMM32 架構**（匯出 `ImeInquire`、`ImeProcessKey`、`ImeToAsciiEx`、`ImeConversionList`、`NotifyIME`、`GetReadingString` 等），透過 COM（`ImeCommonAPIClassFactory`、`PHON70W`）呼叫共用核心。
  - `IMETC10\IMTCCORE.DLL`（698 KB, x64 PE32+）：繁體中文輸入法共用 COM 核心（匯出 `DllGetClassObject`、`CreateAbcDicProxy`、`CreateUDicProxy`，含 `ImeCommonAPIClassFactory`）。
  - `IMETC10\` 其他模組：`IMTCCFG.DLL`、`imtcui.DLL`、`IMTCTIP.dll`（新注音 TSF 前端，僅供比對參考）、`IMTCPROP.exe`、`applets\*`。
  - `shared\`：`IMEAPIS.DLL`、`MSCAND20.DLL`、`IMETIP.DLL` 等共用元件。
- **分析工具**：REA MCP + Ghidra 12.1.4（`E:\Programs\ghidra_12.1.4_PUBLIC`）+ JDK 21。
- **最終目標**：抽離 Windows 7 傳統注音的**按鍵狀態機、注音拼字規則、候選字排序與詞庫格式**，作為獨立核心模組，外層以 **TSF（Text Services Framework）** 實作 `ZZhuyin`。

---

## Token 節省守則（每個階段必守）

1. **一階段一對話（或階段結束後執行 `/compact`）**：下一個階段開新對話時，只需讀 `plan.md` 與前一階段產出的 `docs/phase-*.md`，不重複反編譯已確認的函式。
2. **Summary-first，不做全檔反編譯**：先用 `binary_overview`、`search_procedures`、`search_strings`、`xrefs` 鎖定少數關鍵地址，再對目標函式呼叫 `procedure_pseudo_code` / `analyze_function`。
3. **成果強制落地**：每階段結束前，將「函式位址、結構體定義、狀態機規則、Evidence ID、待解問題（Unknowns）」寫入 `docs/phase-N-*.md`，並更新本檔案的進度勾選框。
4. **切換目標前關閉會話**：分析完單一二進位檔後呼叫 `close_binary` 釋放狀態。

---

## 分階段執行清單

### Phase 1：`phon.ime` 邊界與 COM 呼叫契約（預估最省 token）
- **狀態**：[x] 已完成（見 `docs/phase-1-phon-ime-boundary.md`）
- **目標檔案**：`targets\phon.ime`
- **要回答的問題**：
  1. `phon.ime` 啟動與初始化時（`DllMain` / `ImeInquire` / `ImeSelect`），透過哪幾個 COM CLSID / IID 實例化 `IMTCCORE.DLL`？
  2. `ImeProcessKey` 與 `ImeToAsciiEx` 是把原始按鍵直接轉交給 COM 核心，還是在 `phon.ime` 內先做部分鍵位／模式（全形、半形、標點、組字狀態）過濾？
  3. 傳統注音（「ㄅ半」）與新注音共用 `IMTCCORE.DLL` 時，`phon.ime` 傳入了什麼模式旗標／設定（例如 `PHON70W` 相關設定）來切換成「傳統注音」行為？
- **建議 REA 操作**：
  - `open_binary` (`phon.ime`) → `binary_overview`
  - 查 `ImeInquire`、`ImeSelect`、`ImeProcessKey`、`ImeToAsciiEx`、`NotifyIME` 的 `procedure_pseudo_code` 與 `xrefs`（重點追蹤 `CoCreateInstance` 與 `ImeCommonAPIClassFactory`）。
  - `close_binary`
- **產出檔案**：`docs/phase-1-phon-ime-boundary.md`（IMM32 → COM 呼叫對照表、模式旗標、CLSID/IID 清單）

---

### Phase 2：`IMTCCORE.DLL` 詞庫／字表路徑與格式盤點
- **狀態**：[x] 已完成（見 `docs/phase-2-dictionary-format.md` 與 `tools/parse_dict.py`，已完成 `IMTCPH.IMD` 實檔解析與 O(1) 查字驗證）
- **目標檔案**：`targets\IMETC10\IMTCCORE.DLL`（以及視發現從 `install.wim` 補抽 `Windows\IME\IMETC10\DICTS\*`）
- **要回答的問題**：
  1. `IMTCCORE.DLL` 載入哪些外部字表／詞庫檔案（預期在 `Windows\IME\IMETC10\DICTS\`，例如 `.imd` / `.lex` / `.dic`）？哪些是傳統注音必備的？
  2. `CreateAbcDicProxy`、`CreateUDicProxy` 以及核心詞庫載入函式如何讀取字表標頭與索引？
  3. 傳統注音單字／多字查詢對應的檔案偏移與資料結構是什麼？
- **建議 REA 操作**：
  - `open_binary` (`IMTCCORE.DLL`) → `search_strings`（搜尋 `.imd`、`.dic`、`.lex`、`DICTS`、`Phon`、`PHON`、登錄檔路徑）
  - 透過 `xrefs` 追蹤詞庫開啟（`CreateFileW` / `MapViewOfFile`）與解析函式。
  - 若發現缺少字典檔，**在此階段暫停**，先從 `install.wim` 抽出對應的 `Windows\IME\IMETC10\DICTS\` 檔案到 `targets\DICTS\`。
  - `close_binary`
- **產出檔案**：`docs/phase-2-dictionary-format.md`（字表檔案清單、二進位格式規格、可選：Python 驗證解析腳本 `tools/parse_dict.py`）

---

### Phase 3：注音鍵位映射與拼字狀態機（核心邏輯）
- **狀態**：[x] 已完成（見 `docs/phase-3-bopomofo-state-machine.md`）
- **目標檔案**：`targets\IMETC10\IMTCCORE.DLL`（必要時輔以 `phon.ime`）
- **要回答的問題**：
  1. 標準大千鍵盤（以及若需支援的倚天／許氏等鍵盤配置）如何映射成內部注音編碼（聲母、介音、韻母、聲調 bitfield 或 code）？
  2. 組字狀態機規則：
     - 同類音覆蓋（例如已打「ㄅ」再打「ㄆ」的覆蓋規則）與不合法組合處理
     - 四聲（一聲空白、二三四聲、輕聲）觸發查字與自動上字／出候選窗的條件
     - Backspace / Esc / Enter / 方向鍵在組字中與選字中的行為
     - 標點符號（`Ctrl+,`、全形符號、前導鍵 `` ` `` 等）與中英切換行為
- **建議 REA 操作**：
  - 從 Phase 1 找出的 COM 按鍵處理入口（對應 `ImeProcessKey` / `ImeToAsciiEx` 的 COM 方法）往下追 2～3 層。
  - 只反編譯注音按鍵分類表與狀態轉換函式。
  - `close_binary`
- **產出檔案**：`docs/phase-3-bopomofo-state-machine.md`（鍵位對照表、注音編碼規則、完整狀態轉換表）

---

### Phase 4：候選字排序、選字視窗與相容設定
- **狀態**：[x] 已完成（見 `docs/phase-4-candidate-and-config.md`）
- **目標檔案**：`targets\IMETC10\IMTCCORE.DLL`、`targets\IMETC10\IMTCCFG.DLL`（視需要）
- **要回答的問題**：
  1. 傳統注音查出多個同音字時，候選字的排序依據是什麼（固定字頻表、常用字動態調整、或使用者造詞／詞頻檔）？
  2. 分頁規則（每頁字數、翻頁鍵 `Space` / `PageUp` / `PageDown` / `<` `>`、選字鍵 `1-9` / `0`）與「單一候選字是否直接上字」的設定。
  3. 傳統注音在 `IMTCCFG.DLL` 或登錄檔中有哪些可調選項值得在 `ZZhuyin` 中保留？
- **建議 REA 操作**：
  - 鎖定候選字建立／分頁／選字提交函式與設定讀取函式進行定向反編譯。
  - `close_binary`
- **產出檔案**：`docs/phase-4-candidate-and-config.md`（候選字排序規格、操作按鍵規格、設定項目清單）

---

### Phase 5：規格彙整與 `ZZhuyin`（TSF）實作藍圖
- **狀態**：[x] 已完成（見 `docs/zzhuyin-spec.md`）
- **目標**：不需再跑 Ghidra，純整理 Phase 1～4 的文件。
- **工作內容**：
  1. 彙整成 `docs/zzhuyin-spec.md`（核心引擎介面、資料檔格式、測試案例集）。
  2. 決定 `ZZhuyin` 的實作架構：
     - **核心邏輯層（純 C++ / Rust，零 Windows UI 相依）**：可先寫單元測試驗證每一條注音拼字與選字規則。
     - **TSF 外殼層（Windows C++ COM DLL）**：實作 `ITfTextInputProcessor` / `ITfKeyEventSink` / 候選字視窗。
  3. 驗證是否需要補充任何遺留的 Unknowns（必要時才針對單一函式補查）。

---

## 建議啟動方式

- **現在開始 Phase 1**：直接跟我說「執行 Phase 1」，我會先用 REA 開啟 `targets\phon.ime`，完成後輸出 `docs/phase-1-phon-ime-boundary.md` 並回報摘要。
- **階段之間**：每完成一個 Phase，建議使用 `/compact` 或開新對話，輸入「請讀 `plan.md` 並執行 Phase N」即可無縫接續。
