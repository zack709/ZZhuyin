# Phase 4：候選字排序、選字視窗與相容設定

## 目標二進位檔與資料檔資訊
1. **`IMTCCORE.DLL`**（`E:\Workspace\ZZhuyin\targets\IMETC10\IMTCCORE.DLL`，SHA-256 `701ba64d65d059fb9a9b8301e4a92b90e0860ac8cf1bc79f3948c0bdb065fccd`，Image Base `0x7ff40800000`）
2. **`IMTCCFG.DLL`**（`E:\Workspace\ZZhuyin\targets\IMETC10\IMTCCFG.DLL`，SHA-256 `97cfe87704cefd7ff3c1d5b8f790433bfc1009b64eba4d00f11e339d652a2b3c`，Image Base `0x7ff31b40000`）
3. **實檔驗證資料**：`targets\DICTS\IMTCPH.IMD`、`targets\lcptr.tbl`、`targets\lcphrase.tbl`

---

## 結論摘要（回答 Phase 4 三個問題）

### 1. 候選字排序依據、Section 1 字集過濾與聯想詞格式

#### 1.1 候選字排序規則（純固定詞頻順序，無動態 MRU 調整）
- 傳統注音（`config->vtable[+0x50]() == 0`，對應 `ProfileID = 0x01`）在組字完成進入 `FUN_7ff4082d31c` → `FUN_7ff4082d4e4`（Evidence: `ev_434381d0c0f9c68c98e0efb7230dfba9aacd890438a03ff849d70ed21d265450`、`ev_4d95f6733bfd64a5b2e5fb4cca6fd0dc41d3d6e8af7d1689fab0c4de88661888`）時：
  1. 呼叫 `FUN_7ff40845c60` → `FUN_7ff4084c144`（Evidence: `ev_9ce0afe00acea392799b66c3e91546fe7a1826cdde6d473aefb8f5cda925fd54`、`ev_62134f935742472461e25027f99192ad5c8ea9344a51ccfc7f412784d95e973a`），依序由 **`IMTCPH.IMD`（Bank 0）→ `IMTCPHCS.IMD`（Bank 2，若啟用 Ext-B）→ `TCEUDCPH.TBL`（使用者造字）** 查出所有匹配 CID。
  2. 對每個 CID 呼叫 `FUN_7ff4084c784`（Evidence: `ev_4866b6933dc2aad569d6e1ea1d8df244015f6c1049c2ad1e6542b7ccb9479224`）→ `IMECHLEX::vtable[+0x38]`（`FUN_7ff40861734`），直接取回該 CID 在 **`IMTCPH.IMD` Section 4 倒排 bucket 內的 0-based 順位（`index - startPos`）** 作為排序權重，並選取順位最小者（即 bucket 第 0 筆）為預設候選字。
  3. 接著 `FUN_7ff4082d31c` 呼叫 `candidate->vtable[+0x100](6)` → `FUN_7ff40832c64(6)`（Evidence: `ev_2dacc8651002c9fd7f0d02ad1892a038b08e4b6a03d79e2f6a4c3134f0baf1a8`），套用固定順序排序模式 `6`（非新注音的 N-gram 語意選字或動態學習模式）。
- **結論**：Windows 7 傳統注音**不做動態常用字調整或個人詞頻學習**（`IMTCCFG.DLL` 中 `0x0101 Enable New Phrase Learning` 與 `0x011d Enable Personal Regulating` 對 `mask & 0x01` 皆標記為 `(NoReg)`）；候選字完全依 **`IMTCPH.IMD` Section 4 的靜態預排順序**輸出。

#### 1.2 `IMTCPH.IMD` Section 1 字集過濾位元（完整解開 Phase 2 Unknown #1）
由 `IMECHLEX` vtable `+0xa8/+0xb0/+0xb8/+0xc8`（`0x7ff40862188`～`0x7ff40862230`，Evidence: `ev_33a9558182e6cb3d94d4be7a4870d4c63e1ca8e2aef47aa755dabd31911cf3cb`）與 `FUN_7ff408603f4`（Evidence: `ev_f36416b6d6347c7e17d5dc904476460c7f12e68202210b23aa15b4b5e3ea92c0`）交叉比對 `IMTCPH.IMD` 全表 38,202 筆 CID 實測驗證，Section 1 每字 1 byte 的位元定義如下：

| Bit Mask | `IMECHLEX` vtable | 對應 `IMTCCFG` 屬性 ID 與登錄檔名稱 | 實測意義（驗證於 `IMTCPH.IMD` 全 38,202 CID） |
|---:|---|---|---|
| **`0x01`** | `+0xa8` (`0x7ff40862188`) | `0x0129` `Allow CNS Input Sequence` | **CNS 11643 擴展字與罕用破音讀音**（6,056 筆，例如 `我(ㄜˇ)`、`不(ㄅㄨ˙/ㄈㄨ)`、`三(ㄙㄢˋ/ㄙㄚ)`、`會(ㄍㄨㄟˋ)`、`那(ㄋㄜˋ/ㄋㄟˇ)`） |
| **`0x02`** | `+0xb0` (`0x7ff408621c0`) | `0x0126` `Output Big5 Only` | **Big5 (CP950) 標準字與常用讀音**（20,039 筆，20,038/20,039 可無損編碼為 CP950） |
| **`0x04`** | `+0xb8` (`0x7ff40862230`) | `0x0127` `Include Extension A Characters` | **Unicode CJK 擴展 A 區 `U+3400..U+4DBF`**（7,405 筆，100% 落在 Ext-A，例如 `䙷`） |
| **`0x08`** | `+0xc8` (`0x7ff408621f8`) | `0x012c` `Include HKSCS Characters` | **香港增補字符集 HKSCS**（672 筆，含部首與 Ext-A 香港字） |
| *(Bank 2)* | `IMTCPHCS.IMD` | `0x0128` `Include Extension B Characters` | Unicode CJK 擴展 B 區代理對字表（由外層決定是否納入查詢） |

**精確過濾演算法**（出自 `FUN_7ff408603f4`）：
```c
uint8_t fl = sec1[cid - minCID];
// 1. 若啟用「只輸出 Big5 字元」(Output Big5 Only)，必須帶有 0x02 位元
if (output_big5_only && (fl & 0x02) == 0) {
    skip;
}
// 2. 若此筆 CID 帶有任一擴展／特殊旗標 (0x01 | 0x04 | 0x08 = 0x0D)，
//    則至少須符合一項已啟用的擴展選項，否則過濾掉
if ((fl & 0x0D) != 0) {
    bool allowed = (include_ext_a && (fl & 0x04))
                || (allow_cns     && (fl & 0x01))
                || (include_hkscs && (fl & 0x08));
    if (!allowed) skip;
}
```
- **實測驗證**：預設關閉 Ext-A / CNS / HKSCS 時，`ㄉㄜ˙` 從未過濾的 `的得䙷地坔埊嶳底襶` 精準過濾為 **`的得地`**，`ㄨㄛˇ` 從 `我婐捰䂺䰀婑` 過濾為 **`我婐捰`**，與 Windows 7 傳統注音實測完全吻合！

#### 1.3 聯想詞（相關字詞）機制與 `LCPTR.TBL` / `LCPHRASE.TBL` 格式
- 由 `FUN_7ff40857c50`（Evidence: `ev_1b4ebb661c884a4f102e59d55cc28de18178de501f4d0672a7453c04c11bd2c3`）載入 File ID `0x24`（`LCPTR.TBL`）與 `0x25`（`LCPHRASE.TBL`），受設定 `0x0113`（`Prompt Associate Phrase.Phonetic`）控制。
- **觸發時機**：
  1. 在正常同音候選清單（`type == 2`）選定字元上字後（`FUN_7ff4081f588`，Evidence: `ev_ec66e0a0db5e1ca1795888fb5cbffe8abebc154ff6c447738659ff0c7f0a95c4`）；或
  2. 單一候選字自動上字後（`FUN_7ff4082c0d8`，Evidence: `ev_464893b93281643477452009bd0a73b4a984bf1e23323675d5eaecbe7825cbc5`）。
  若 `prop 0x0113 != 0` 且剛才的候選清單不是聯想詞（`type != 3`），立即開啟 **`type == 3` 聯想詞候選清單**。
- **二進位檔案格式（實檔驗證）**：
  - **`lcptr.tbl`**（24,114 bytes）：6-byte 零標頭 + **4,018 筆 6-byte 索引紀錄** `{ uint16_t lead_wchar; uint32_t wchar_offset; }`（little-endian，依 `lead_wchar` 嚴格遞增，支援二分搜尋）。
  - **`lcphrase.tbl`**（211,938 bytes）：UTF-16LE `NUL`（`0x0000`）分隔詞尾池。第 `k` 個字的所有聯想詞位於位元組區間 `[ptr[k]*2, ptr[k+1]*2)`，以 `\0` 切分。
  - **實測範例**：
    - `一` (`U+4E00`, `ptr=0..157`) → `般、直、些、時、定、月、點、一、旦、切...`
    - `三` (`U+4E09`, `ptr=252..389`) → `八、重、商銀、溫暖、峽、重市、星、國、軍、總...`

---

### 2. 分頁規則、操作按鍵與「單一候選字自動上字」

#### 2.1 單一候選字自動上字（無條件啟用）
- 出自 `FUN_7ff4082c0d8`（`candidate->vtable[+0xc8]`，Evidence: `ev_464893b93281643477452009bd0a73b4a984bf1e23323675d5eaecbe7825cbc5`）：
  - 在傳統模式（`mode ∈ {0: 傳統注音, 2: 倉頡, 4: 速成}`）下開啟同音候選清單（`param_2 == 2`）時，若候選字總數 **`count == 1`**，引擎**無條件直接關閉候選窗並立即提交上字**（不檢查額外開關）；若同時開啟了「查詢組字字根」（`0x0148`）則發送字根提示，若開啟了「相關字詞」（`0x0113`）則接著開啟 `type == 3` 聯想詞清單。

#### 2.2 候選窗分頁與按鍵完整規格
（出自 `FUN_7ff4081effc`、`FUN_7ff4081f20c`、`FUN_7ff4081f2e4`、`FUN_7ff4081f588`、`FUN_7ff4081f67c`，Evidence: `ev_195663ae40b720fbf3987f7e723716a999bf6da95ff76b2f09d0e99077e8ae2d`、`ev_80bd1120659f3e4189771a62414f00df107913a7491a422dbad330bb911c8ab8`、`ev_9b30846ec03eb5fd89d179430ac47db9c6b433dff2f4ee82ed611223a49ef7f3`、`ev_ec66e0a0db5e1ca1795888fb5cbffe8abebc154ff6c447738659ff0c7f0a95c4`、`ev_d14ab11dc0b9022a19217ec12a52905fe2e20ac644b35e11fb1c3236d4f12dab`）

- **每頁字數**：固定 **9 字**（硬編碼常數 `9`，選字鍵為 `1`～`9`，**不使用 `0`**）。
- **選字鍵差異（同音字 vs 聯想詞）**：
  - **同音候選窗（`type == 2`）**：按 **`1`～`9`（不可按 Shift）** 直接選定本頁第 `0..8` 筆並上字。
  - **聯想詞候選窗（`type == 3`）**：必須按 **`Shift + 1`～`9`** 才選定聯想詞！若直接按未加 Shift 的 `1`～`9`（例如大千鍵盤的 `1=ㄅ`、`2=ㄉ`、`5=ㄓ`），候選窗會自動關閉並將該鍵透傳給注音組字器，避免干擾連續打字！
- **翻頁與移動規則**（設總數為 `N`，目前游標為 `idx`）：
  - **`Space` (`0x20`) / `PageDown` (`0x22`)**：
    - 若 `idx + 9 < N` → `idx = idx + 9`
    - 否則若 `N <= 9` → 不動作
    - 否則若 `idx < N - (N % 9)`（位於倒數第二頁但同欄在末頁無字）→ `idx = N - 1`（跳至末頁最後一字）
    - 否則（已在末頁）→ **`idx = idx % 9`**（循環回首頁對應位置）
  - **`PageUp` (`0x21`)**：若 `idx - 9 >= 0` → `idx = idx - 9`；否則不動作（**不循環**）。
  - **`↑` (`0x26`)**：若 `idx > 0` → `idx - 1`；否則循環至 `N - 1`。
  - **`↓` (`0x28`)**：若 `idx + 1 < N` → `idx + 1`；否則循環至 `0`。
  - **`←` (`0x25`) / `→` (`0x27`)**：在傳統模式（`param_1 + 0x28 != 0`）下直接攔截無動作（橫／直排設定 `0x0145 & 1` 會先將 `←/↑/→/↓` 映射為 `↑/←/↓/→` 再執行上述邏輯）。
  - **`Enter` (`0x0D`)**：選定目前反白項 `idx` 並上字。
  - **`Esc` (`0x1B`)**：關閉候選窗並取消目前未提交組字（`comp->vtable[+0xa8]()`）。
  - **`Tab` (`0x09`)**：傳統模式下無動作（僅新注音用於切換 9 字／展開面板）。
  - **其他按鍵**：在傳統模式下若未命中上述按鍵，設 `*handled = 0`、關閉候選窗（`vtable +0xd8`），並交由後續注音組字器處理。

---

### 3. `IMTCCFG.DLL` 傳統注音相容設定清單

從 `IMTCCFG.DLL` `.rdata` 偏移 `0x3000`（123 筆、每筆 120 bytes 的靜態屬性表，Evidence: `ev_6f5a07600a2724a2db01e8a61d88a7e4cf439cb0824c8e5669ee1030c5698a1a`）完整擷取所有對傳統注音（`mode_mask & 0x01 != 0`）有效之登錄檔設定（路徑位於 `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\ime\...` 與相容舊路徑 `Software\Microsoft\Windows\CurrentVersion\MSTCIPH`）：

| Property ID | 登錄檔鍵名（Registry Value Name） | 適用範圍 (`mask`) | 說明 | `ZZhuyin` 建議保留？ |
|---:|---|---:|---|---|
| **`0x0141`** | `Keyboard Layout Setting.Phonetic` | `0x01` (傳統注音專用) | 鍵盤配置：`1`=標準大千、`2`=倚天、`3`=IBM、`4`=精業、`5`=倚天26/許氏 | **必留（核心）** |
| **`0x0126`** | `Output Big5 Only` | `0x7f` | 僅輸出 Big5 字元（對應 Section 1 bit `0x02`） | **必留（核心）** |
| **`0x0127`** | `Include Extension A Characters` | `0x7f` | 包含 Unicode CJK Ext-A 字元（對應 Section 1 bit `0x04`） | **必留** |
| **`0x0128`** | `Include Extension B Characters` | `0x7f` | 包含 Unicode CJK Ext-B 代理對字表 `IMTCPHCS.IMD` | **建議保留** |
| **`0x0129`** | `Allow CNS Input Sequence` | `0x7f` | 允許 CNS 擴展字／罕用破音讀音（對應 Section 1 bit `0x01`） | **建議保留** |
| **`0x012c`** | `Include HKSCS Characters` | `0x7f` | 包含香港增補字符集（對應 Section 1 bit `0x08`） | **選配保留** |
| **`0x0113`** | `Prompt Associate Phrase.Phonetic` | `0x01` (傳統注音專用) | 啟用「相關字詞（聯想詞）」視窗（`Shift+1..9` 選詞） | **必留（經典功能）** |
| **`0x0114`** | `Default Input Mode` | `0x7f` | 啟動預設輸入模式（中文 / 英文） | **必留** |
| **`0x0115`** | `Enable Punctuation Input` | `0x7f` | 啟用 `Ctrl+,` 等標點組合鍵與全形標點輸入 | **必留** |
| **`0x011b`** | `Enable Warning Beep Feedback` | `0x7f` | 不合法注音組合時發出提示音（Beep） | **必留** |
| **`0x0120`** | `Left Shift Usage` | `0x7f` | 左 Shift 短按（`< 401ms`）切換中／英模式 | **必留** |
| **`0x0121`** | `Right Shift Usage` | `0x7f` | 右 Shift 短按切換行為 | **建議保留** |
| **`0x0119`** | `UI Font Setting` | `0x7f` | 候選視窗字型與大小設定 | **建議保留** |
| **`0x0159`** | `EUDC Filename.Phonetic` | `0x03` | 使用者造字檔路徑（預設 `TCEUDCPH.TBL`） | 選配 |
| `0x0148` | `Query Input Sequence Setting.Phonetic` | `0x01` | 查詢組字字根（反向提示他種輸入法字根） | 可略過 |
| `0x0117` | `Show Ballon UI` | `0x7f` | 狀態列氣球提示 | 可略過 |
| `0x0145` | `(NoReg)TextOrientation` | `0x7f` | 候選窗橫排／直排方向（影響方向鍵互換） | **建議保留** |
| `0x0136` / `0x0137` | `(NoReg)PH.CModeNative` / `PH.CModeFullShape` | `0x01` | 執行期狀態：中／英模式、全形／半形模式 | **必留（執行期狀態）** |

> **確認傳統注音「不包含」的設定**（在 `IMTCCFG.DLL` 屬性表中對 `mask & 0x01` 標為 `(NoReg)`）：新詞學習（`0x0101`）、個人化調整（`0x011d`）、使用者造詞（`0x0102`）、模糊音（`0x0104`）、智慧中英切換（`0x0105`）、前導符號鍵（`0x0125`）、簡體輸出（`0x012d`）、省略聲調（`0x0130`/`0x0131`）。

---

## 遺留問題（Unknowns）
- **無**：Phase 1～4 所有核心逆向問題與實檔驗證（`IMTCPH.IMD`、`LCPTR.TBL`、`LCPHRASE.TBL`、`IMTCCFG.DLL` 屬性表）均已解決，可直接進入 **Phase 5：規格彙整與 `ZZhuyin`（TSF）實作藍圖**。
