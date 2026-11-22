# Phase 2：`IMTCCORE.DLL` 詞庫／字表路徑與格式盤點

## 目標二進位檔資訊
1. **`IMTCCORE.DLL`**（`E:\Workspace\ZZhuyin\targets\IMETC10\IMTCCORE.DLL`）
   - **SHA-256**：`701ba64d65d059fb9a9b8301e4a92b90e0860ac8cf1bc79f3948c0bdb065fccd`
   - **架構／格式**：PE32+ (`x86_64`)，Image Base `0x7ff40800000`，2026 procedures / 379 strings
   - **PDB 參考**：`ImTcCore.pdb`（GUID `1e56ffff-602e-488e-984b-3f265a834f5b`, age 1）
   - **Evidence**：`ev_70c5838bf05e6b651c5c69bf5107586d4f1ba3303d186638388c46d3c2bc5482`
2. **`IMTCCFG.DLL`**（`E:\Workspace\ZZhuyin\targets\IMETC10\IMTCCFG.DLL`）
   - **SHA-256**：`97cfe87704cefd7ff3c1d5b8f790433bfc1009b64eba4d00f11e339d652a2b3c`
   - **PDB 參考**：`ImTcCfg.pdb`（GUID `40382d50-5394-44bb-a43d-dfb9443de7be`, age 1）
   - **用途**：`IMTCCORE.DLL` 不硬編碼詞庫檔名，而是透過 `IMTCCFG.DLL` 的設定物件（vtable `+0xe8`）依 **File ID** 查詢完整路徑。

---

## 結論摘要（回答 Phase 2 三個問題）

### 1. 傳統注音（Mode ID = 3）載入的外部字表與詞庫清單

#### 模式旗標轉換（承接 Phase 1）
在 `IMTCCORE.DLL` 的 `ImeCommonAPIClassFactory` vtable `+0x18`（`FUN_7ff4080b238` @ `0x7ff4080b238`，Evidence: `ev_d1372f6df5c9fba221bfbd44084cd78cd9bc4cc2cb05a98b9dbbfe83e8f2fb03`）呼叫 `FUN_7ff4080af98`（`0x7ff4080af98`，Evidence: `ev_d0a20046b54ca8f4b117e54111c09fb15aa75c2c904c5435667ee0b60b74070b`）：
- **`Mode ID == 3`（傳統注音）** → 轉換為引擎屬性 `0x142` 的位元旗標 **`0x01`**（對照：新注音 `4 → 0x02`、倉頡 `5 → 0x04`、新倉頡 `6 → 0x08`、速成 `7 → 0x10`、新速成 `8 → 0x20`、其他 `9 → 0x40`）。
- 詞庫管理器 `FUN_7ff4084c860`（`0x7ff4084c860`，Evidence: `ev_c4af98e249677a8fe9782fa43cc7769dd9b6ef60eaba18305c99898ccb3c82d1`）依引擎類別 `*param_1`（`0 = 注音`、`1 = 倉頡/速成`、`2 = 廣東話`）向 `IMTCCFG.DLL` 查詢檔案路徑並做記憶體映射。

#### File ID → 檔名完整對照（出自 `IMTCCFG.DLL` `FUN_7ff31b4cbd8` @ `0x7ff31b4cbd8` 與靜態表 `DAT_7ff31b42be0`，Evidence: `ev_a26021cc3236fdecef315dc8ec330890cf227c95d9c271b7901c6a1c95ab1061`、`ev_3d2959ce9456cbe5fd0ee1442125e7a2019acaaf6ffa231e48cffb20592c5a54`）

| File ID | 檔名 | 預期位置 | 格式 Magic | 傳統注音必備？ | 用途說明 |
|---:|---|---|---|---|---|
| **`0x2a` (42)** | **`IMTCPH.IMD`** | `Windows\IME\IMETC10\DICTS\` | `IMECHLEX` v2 | **必備（核心）** | **注音單字音表（BMP）**：注音碼 → 候選字清單、字元 ↔ 讀音雙向查詢 |
| **`0x2b` (43)** | **`IMTCPHCS.IMD`** | `Windows\IME\IMETC10\DICTS\` | `IMECHLEX` v2 | 選配（擴展字） | 注音代理對／擴展字單字表（Bank ID = `2`） |
| **`0x01` (1)** | **`IMTCL.IMD`** | `Windows\IME\IMETC10\DICTS\` | `IMEWDLEX` v2 | 引擎啟動必備 | 繁體中文主詞庫（`FUN_7ff4084c860` 載入失敗會中止初始化） |
| `0x29` (41) | `IMTCLS.IMD` | `Windows\IME\IMETC10\DICTS\` | `IMSUWDLX` v2 | 選配 | 代理對擴展詞庫 |
| `0x02` (2) | `IMTCV.IMD` | `Windows\IME\IMETC10\DICTS\` | `IMEVRLEX` v2 | 選配 | 異體字／變體詞庫 |
| `0x03` (3) | `IMTCS.IMD` | `Windows\IME\IMETC10\DICTS\` | `IMESTLEX` v2 | 選配 | 符號／統計表 |
| `0x04` (4) | `IMTCT.IMD` | `Windows\IME\IMETC10\DICTS\` | `IMETCLEX` v2 | 選配 | 繁體語言模型／轉移表 |
| `0x64` (100) | `TCEUDCPH.TBL` | 使用者 EUDC 路徑 | — | 選配 | 注音使用者造字表（由 `FUN_7ff4085e888` 載入，缺檔時以空表運作） |
| `0x24` (36) | `LCPTR.TBL` | `System32\`（`GetSystemDirectoryW`） | — | 選配 | 聯想詞（Associate Phrase）指標表 |
| `0x25` (37) | `LCPHRASE.TBL` | `System32\`（`GetSystemDirectoryW`） | — | 選配 | 聯想詞詞條表 |

> **對 `ZZhuyin` 的實務意義**：若目標是復刻 Windows 7 傳統注音（「ㄅ半」單字輸入），**最核心且不可替代的資料檔是 `IMTCPH.IMD`**（加上可選的 `IMTCPHCS.IMD` 支援 Surrogate 擴展字、以及 `LCPTR.TBL` / `LCPHRASE.TBL` 支援聯想詞）。

---

### 2. `CreateAbcDicProxy`、`CreateUDicProxy` 與核心詞庫載入流程

- **`CreateAbcDicProxy` (`0x7ff40816c6c`)**：在繁體中文版 `IMTCCORE.DLL` 中為空殼，直接回傳 `0x80004005`（`E_FAIL`）（Evidence: `ev_fbaaf3a6479d25b8c1940c5aa135fcfbd38a5433a93772a2160607ab1e82f6ba`）。
- **`CreateUDicProxy` (`0x7ff4080af4c`)**：檢查固定 GUID 後建立 `0x60` bytes 代理物件（vtable `PTR_FUN_7ff40806b20`），呼叫 `FUN_7ff40846828`（`0x7ff40846828`，Evidence: `ev_1d833c379e4e7d435b21ed05551d385da8e594692178f0fa205f892b2247e418`），透過 `FUN_7ff4084ce0c`（主詞庫載入器）載入上述系統詞庫與使用者詞庫（Evidence: `ev_8a7acb14e12eb938bfac611caa39ff9c1812e109bce925b313b4b0d7018b9a01`、`ev_7ce653a898839aaaeb8ba9fb679ff25f7d8feaae0c041031fb5286fefe67011c`）。
- **檔案開啟與映射**：`FUN_7ff40862f20`（`0x7ff40862f20`，Evidence: `ev_d7ad03d93589de598360db7c0276cd7fea3a613897517d6f3fa37377e1581cc2`）向 `IMTCCFG` 取得路徑後，呼叫 `FUN_7ff40862f9c` 以 `CreateFileW` + `CreateFileMappingW` + `MapViewOfFile` 唯讀映射整個 `.IMD` 檔案，並把基底指標存於物件 `+0x28`、檔案大小存於 `+0x30`。

---

### 3. 傳統注音字表二進位格式規格

#### A. `IMECHLEX` v2 格式（`IMTCPH.IMD` / `IMTCPHCS.IMD`）
由 `FUN_7ff40861c64`（`0x7ff40861c64`，Evidence: `ev_696727203e5fdaec846bc37f8ffb1617413535bf4eff6017c39ddcb18673e133`）解析，並由 vtable `0x7ff408073b0`（Evidence: `ev_1c428c319044060061bb28d8df8295bfa5fe9a15afb91227bb2248333ed6133f`）提供查詢介面：

##### Header（56 bytes，little-endian）
| Offset | Type | 欄位名稱（推論） | 說明／驗證條件 |
|---:|---|---|---|
| `0x00` | `char[8]` | `magic` | 必須為 `"IMECHLEX"` |
| `0x08` | `uint64` | `reserved` | 未使用／保留 |
| `0x10` | `uint32` | `minorVersion` | 必須為 `0` |
| `0x14` | `uint32` | `majorVersion` | 必須為 `2` |
| `0x18` | `uint32` | `sec1_attrOffset` | Section 1 偏移（字元屬性與字集旗標：Big5/CNS/HKSCS 等） |
| `0x1C` | `uint32` | `sec2_unicodeOffset` | Section 2 偏移（`uint16_t[]` 排序字元表） |
| `0x20` | `uint32` | `sec3_readingOffset` | Section 3 偏移（`uint16_t[]` CID → 主讀音表） |
| `0x24` | `uint32` | `sec4_indexOffset` | Section 4 偏移（讀音 → 候選 CID 倒排索引） |
| `0x28` | `uint32` | `totalSize` | 檔案總大小（必須 `<= fileSize`） |
| `0x2C` | `uint32` | `maxCID` | CID 上界（不含），字元總數 `N = maxCID - minCID` |
| `0x30` | `uint32` | `minCID` | CID 下界（含） |
| `0x34` | `uint32` | `flags` | 存入 `this + 0x50` |

##### 4 個 Section 與查詢演算法
1. **Section 1（`sec1_attrOffset`）**：
   - 開頭為 `N` bytes（8-byte 對齊）的字元屬性陣列（`this + 0x88`），其後緊接 3 個子區塊（每個子區塊含 8-byte 標頭與長度），供字集過濾（vtable `+0xa8`/`+0xb0`/`+0xb8`/`+0xc8`）使用。
2. **Section 2（`sec2_unicodeOffset`，`this + 0x10`）— `WCHAR unicodeTable[N]`**：
   - **CID → Unicode**（`0x7ff408643ec`，Evidence: `ev_04e528bd9c95529d195f376cbe90367c24a9ddc14254a641ae0aa72e72eccc5d`）：
     - `cid` 的 bit `20..23` 為 Bank ID（`0` = `IMTCPH.IMD`，`2` = `IMTCPHCS.IMD`），低 20 bits 為 CID。
     - 直接回傳 `unicodeTable[(cid & 0xFFFFF) - minCID]`。
   - **Unicode → 所有 CID（含破音字）**（`FUN_7ff40862284` @ `0x7ff40862284`，Evidence: `ev_15adcc5ad18d4e61cb92ada7e02d9289033105b9e39abc75bd74028690610ed4`）：
     - 因 `unicodeTable` 依 UTF-16 遞增排序，以二分搜尋找到所有等於目標 `WCHAR` 的連續範圍，對應出多個 CID（每個破音讀法各有一個 CID）。
3. **Section 3（`sec3_readingOffset`，`this + 0x18`）— `uint16_t readingTable[N]`**：
   - **CID → 16-bit 注音讀音碼**（`FUN_7ff4086457c` @ `0x7ff4086457c`，Evidence: `ev_1d604449606bcceece8bcedac9c2a54f0ada4fbdf5228ff823bda326c87b901c`）：
     - 讀取 `readingTable[(cid & 0xFFFFF) - minCID]`；若為 `0xFFFF` 表示無讀音。
4. **Section 4（`sec4_indexOffset`，`this + 0x20`）— 注音讀音 → 候選字 CID 倒排索引**：
   - **查詢函式**：`FUN_7ff408603f4`（`0x7ff408603f4`，Evidence: `ev_f36416b6d6347c7e17d5dc904476460c7f12e68202210b23aa15b4b5e3ea92c0`）
   - 結構分為兩段：
     - `+0x0000 .. +0x8001`（`0x8002` bytes = 16,385 個 `uint16_t`）：**`uint16_t bucketOffset[16385]`**
     - `+0x8002 ..`：**`uint16_t candidateCIDs[]`**（直接按預設候選順序排列！）
   - 查詢流程：
     1. 呼叫 `FUN_7ff4085f848`（`0x7ff4085f848`，Evidence: `ev_2284fd2b43ee471c9b11fd3eacc96b42ca601aec68e098c8d4bba71931e84014`）將 16-bit 注音碼轉成 `(startBucket, bucketCount)`（指定聲調時 `bucketCount = 1`；未指定聲調時涵蓋該音節所有聲調）。
     2. 取出 `startPos = bucketOffset[startBucket]`、`endPos = bucketOffset[startBucket + bucketCount]`。
     3. 依序走訪 `candidateCIDs[startPos .. endPos - 1]`，套用字集過濾後即為該注音的候選字清單！
   - 同音字排序位置查詢：`FUN_7ff40861734`（`0x7ff40861734`，Evidence: `ev_7a17a92813c302928e18379ec3b2e86927eb613ed97e1a5a359ba7e0636d358e`）也是直接掃描該 bucket 內的 `candidateCIDs` 並回傳 `index - startPos`。

#### B. `IMEWDLEX` / `IMSUWDLX` / `IMETCLEX` / `IMEDMLEX` / `IMEVRLEX` v2 格式（`IMTCL.IMD` 等）
由 `FUN_7ff4085df58`（`0x7ff4085df58`，Evidence: `ev_1620c76072d03a7477d90061a08f6518f6363c79c5406b3313856ed3a3038fe8`）與 `FUN_7ff4085de48`（`0x7ff4085de48`，Evidence: `ev_5e9144e5042d49fb3705f5ca685e11966ef18934ce15527aa26797e30886d7b2`）解析：
- `+0x00..0x07`：8-byte Magic（`IMEWDLEX` / `IMSUWDLX` / `IMETCLEX` / `IMEDMLEX` / `IMEVRLEX`）
- `+0x10`：`0`；`+0x14`：`2`
- `+0x18`、`+0x1C`、`+0x20`、`+0x24`：3 個區塊的起迄偏移
- `+0x28..0x47`：詞庫統計與索引邊界參數（共 8 個 `uint32`）

---

## 實檔驗證結果（已完成）

已於 `targets\DICTS\` 與 `targets\` 驗證 Windows 7 實檔：

| 檔案 | 大小 (bytes) | Header Magic | Version | 說明 |
|---|---:|---|---|---|
| `IMTCPH.IMD` | 893,472 | `IMECHLEX` | `2.0` | **注音 BMP 單字音表**（CID `[6400, 44602)` 共 38,202 筆；Section 4 含 1,484 個非空 bucket、36,639 筆候選 CID） |
| `IMTCPHCS.IMD` | 377,928 | `IMSUCHLX` | `2.0` | 注音 Surrogate 擴展字表 |
| `IMTCL.IMD` | 1,321,968 | `IMEWDLEX` | `2.0` | 繁體中文主詞庫 |
| `IMTCLS.IMD` | 404,864 | `IMSUWDLX` | `2.0` | Surrogate 擴展詞庫 |
| `IMTCS.IMD` | 15,815,555 | `IMESTLEX` | `2.0` | 符號／統計表 |
| `IMTCV.IMD` | 429,488 | `IMEVRLEX` | `2.0` | 異體字／變體詞庫 |
| `IMTCCJ.IMD` | 1,618,106 | `IMECHLEX` | `2.0` | 倉頡單字表（參考） |
| `IMTCCJCS.IMD` | 912,766 | `IMSUCHLX` | `2.0` | 倉頡擴展字表（參考） |
| `lcptr.tbl` / `lcphrase.tbl` | 24,114 / 211,938 | — | — | 聯想詞指標表與詞條表 |

### 實測確認之 16-bit 注音編碼與 O(1) 查表規則（驗證腳本：`tools/parse_dict.py`）
從 `IMTCPH.IMD` 直接解碼 `U+3105..U+3129`（ㄅ～ㄩ）與聲調字元，確認 16-bit 注音碼位元配置為：
- **bits 15..11（5 bits）**：聲母（Initial）：`0` = 無、`1..21` = `ㄅㄆㄇㄈㄉㄊㄋㄌㄍㄎㄏㄐㄑㄒㄓㄔㄕㄖㄗㄘㄙ`
- **bits 10..9（2 bits）**：介音（Medial）：`0` = 無、`1` = `ㄧ`、`2` = `ㄨ`、`3` = `ㄩ`
- **bits 8..5（4 bits）**：韻母（Final）：`0` = 無、`1..13` = `ㄚㄛㄜㄝㄞㄟㄠㄡㄢㄣㄤㄥㄦ`
- **bits 4..2（3 bits）**：聲調（Tone）：`0` = 未指定、`1` (`0x04`) = 一聲、`2` (`0x08`) = 二聲 `ˊ`、`3` (`0x0C`) = 三聲 `ˇ`、`4` (`0x10`) = 四聲 `ˋ`、`5` (`0x14`) = 輕聲 `˙`
- **bits 1..0（2 bits）**：保留（`0`）

Section 4 倒排索引的 bucket 編號直接等於 **`bucket = reading_code >> 2`**（`0..16383`，對應 `bucketOffset[16385]`），查詢時間複雜度為 **O(1)**。實測範例（未套用 Section 1 字集過濾）：
- `ㄓㄨˋ` (`0x7C10`, bucket `7940`) → `住著助注祝駐註柱蛀鑄炷...`
- `ㄧㄣ` (`0x0344`, bucket `209`) → `因音陰姻殷茵...`
- `ㄓㄨㄥ` (`0x7D84`, bucket `8033`) → `中鐘終忠鍾衷...`
- `ㄨㄛˇ` (`0x044C`, bucket `275`) → `我婐捰...`
- `ㄉㄜ˙` (`0x2874`, bucket `2589`) → `的得䙷地...`

---

## 遺留問題（Unknowns，留待 Phase 3～4）
1. **Section 1 字集過濾位元（Big5 / 常用字 / 擴展字 / 簡體過濾）**：實測 `ㄉㄜ˙` 出現 Ext-A 字 `䙷`，代表預設傳統注音在顯示候選字前會透過 `IMECHLEX` vtable `+0xa8/+0xb0/+0xb8/+0xc8` 讀取 Section 1 旗標過濾非 Big5／擴展字；留待 **Phase 4** 補完精確位元定義。
2. **鍵盤按鍵 → 16-bit 注音碼與組字狀態機**：留待 **Phase 3** 完整逆向。
