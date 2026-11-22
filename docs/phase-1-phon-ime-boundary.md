# Phase 1：`phon.ime` 邊界與 COM 呼叫契約

## 目標二進位檔資訊
- **檔案**：`E:\Workspace\ZZhuyin\targets\phon.ime`
- **SHA-256**：`bfefed78130c394539a30283e4544214a6c75568a3318777db548069e4069b1e`
- **架構／格式**：PE32+ (`x86_64`), Image Base `0x7ff2e280000`
- **PDB 參考**：`phon.pdb`（GUID `ea796e9f-9abc-4308-8a3d-d8a0a6589244`, age 1）
- **分析規模**：796 procedures, 215 defined strings（Evidence: `ev_b54695297421a14a504e9c4589492104b00a7806cb6a24bf65215344c3dcb4aa`）

---

## 結論摘要（回答 Phase 1 三個問題）

### 1. `phon.ime` 如何實例化 `IMTCCORE.DLL`（COM 契約與 IID）
- `phon.ime` 本身是 **IMM32 介面外殼**，內部建立一個本地 COM 橋接物件（`0xe8` bytes，建構函式 `0x7ff2e289a30`，介面 IID `{f1caa554-85ec-4310-b333-254f8e65f864}`，Evidence: `ev_1a1bca0f37c261c91b963bdf9b605df4c49a2b04f510d0398d314907ba0a8414`、`ev_5e21c6977bc27c980b5b7d939cab338ed57503b4353d94a7eec785b1f15191e0`）。
- 真正連線到 `IMTCCORE.DLL` 的入口在 **`FUN_7ff2e28bab4`**（`0x7ff2e28bab4`，Evidence: `ev_0b076beab8dd6000823226c1a78c60c08503a3328f2dfedd52d64179e7374fcd`）呼叫 **`FUN_7ff2e29641c`**（`0x7ff2e29641c`，Evidence: `ev_783ecc51edfd9b5ba673ec33ca1694f3c40f5a2b739b67710110081c7f4934da`）：
  1. 以 `LANGID = 0x0404`（十進位 `1028`）依序嘗試解析 ProgID：
     - `ImeCommonAPIClassFactory%04d.%d`（`0x7ff2e282210`）
     - `ImeCommonAPIClassFactory_%04d.%d`（`0x7ff2e282250`）
     - `ImeCommonAPIClassFactory%04d`（`0x7ff2e282298`）
     - `ImeCommonAPIClassFactory_%04d`（`0x7ff2e2822d8`）
     （Evidence: `ev_46ce8b7fa1bf60398b08db95f7083fa80804f2055bb6933aa2177a4281026306`）
  2. 以 IID **`{f40663bc-452e-468e-82d0-ba7d9f35b845}`**（`DAT_7ff2e281510`，Evidence: `ev_e7ba98690372109dd2ba0852a00e420e869e16328335044370bdb709e89d3e03`）建立工廠介面（推測為 `IImeCommonAPIClassFactory`）。
  3. 透過該工廠的 vtable `+0x18` 方法，傳入 **Mode ID = `3`** 建立兩個核心物件：
     - `factory->vtable[+0x18](factory, 0, 3, &obj0)`（類別 `0`）
     - `factory->vtable[+0x18](factory, 1, 3, &obj1)`（類別 `1`，存入 `this + 0x48`）

---

### 2. 傳統注音 vs 新注音的模式切換表（關鍵發現！）
`phon.ime` 內部硬編碼自己的 HKL 為 **`0xE0010404`**（見 `FUN_7ff2e28deb8` @ `0x7ff2e28deb8` 與 `FUN_7ff2e28bab4` @ `0x7ff2e28bab4`），並以三個長度為 8 的靜態對照表對應各輸入法（Evidence: `ev_0729a2da2e873c1b9453004a4abaa028f9a69f2be91efa245ddd916bf93ac224`、`ev_bde948b7d85761d58bc45474f918fe9521c09f4a4d074abadf69bdfa7cde035d`）：

| Index | HKL (`0x7ff2e2815e0`) | LANGID (`0x7ff2e281500`) | **Mode ID (`0x7ff2e2814e0`)** | 對應輸入法（推論／觀察） |
|---:|---|---|---:|---|
| 0 | `0xE00E0804` | `0x0804` (2052) | `1` | 簡體中文拼音（`PINTLGNT70W`） |
| 1 | `0xE0080404` | `0x0404` (1028) | `4` | 繁體中文 新注音 |
| **2** | **`0xE0010404`** | **`0x0404` (1028)** | **`3`** | **繁體中文 傳統注音（`PHON70W`，`phon.ime` 使用此項）** |
| 3 | `0xE0090404` | `0x0404` (1028) | `6` | 繁體中文 新倉頡 |
| 4 | `0xE0020404` | `0x0404` (1028) | `5` | 繁體中文 倉頡 |
| 5 | `0xE00A0404` | `0x0404` (1028) | `8` | 繁體中文 新速成 |
| 6 | `0xE0030404` | `0x0404` (1028) | `7` | 繁體中文 速成 |
| 7 | `0xE00B0404` | `0x0404` (1028) | `9` | 繁體中文 新大易／其他 |

> **給 Phase 2～3 的關鍵線索**：在 `IMTCCORE.DLL` 中，只要追蹤 `ImeCommonAPIClassFactory` 收到 **`Mode ID == 3`**（而非新注音的 `4`）的分支，就能精準鎖定「傳統注音」專屬的詞庫載入與按鍵組字邏輯！

---

### 3. IMM32 入口與按鍵封包格式
（Evidence: `ev_3c91f27129c9ee931e961db4e8e890b0707b0623def84afe38901c9b3eb3e912`、`ev_783ecc51edfd9b5ba673ec33ca1694f3c40f5a2b739b67710110081c7f4934da`）

- **`ImeInquire` (`0x7ff2e29226c`)**：
  - UI 視窗類別名回傳 `"PHON70W"`（`0x7ff2e281bd0`）。
  - 填寫 `IMEINFO`：
    - `dwPrivateDataSize = 0x28`
    - `fdwProperty = 0x1d002a`
    - `fdwConversionCaps = 0x789`
    - `fdwSentenceCaps = 1`
    - `fdwUICaps = 3`
    - `fdwSCSCaps = 7`
    - `fdwSelectCaps = 0`
- **`ImeSelect` (`0x7ff2e2928a8`)** → 建立／取得本地橋接物件，呼叫其 vtable `+0x18`。
- **`ImeProcessKey` (`0x7ff2e292750`)** → 封裝按鍵結構後呼叫橋接物件 vtable `+0x28`。
- **`ImeToAsciiEx` (`0x7ff2e292b14`)** → 封裝按鍵結構後呼叫橋接物件 vtable `+0x20`。
- **`NotifyIME` (`0x7ff2e292c94`)** → 透過 `FUN_7ff2e296dc8` 取得子介面，呼叫其 vtable `+0x30(dwAction, dwIndex, dwValue)`。
- **按鍵事件封包結構**（由 `FUN_7ff2e293e48` @ `0x7ff2e293e48` 填寫，16 bytes）：
  - `+0x00` (`WCHAR`)：`ToUnicode(uVirtKey, scanCode, lpbKeyState)` 轉出的字元（若 `uVirtKey == 0x12` (`VK_MENU`) 則跳過；轉換結果非 1 字元時設為 `L'\0'`）
  - `+0x02` (`WORD`)：修飾鍵位元旗標：
    - `0x0001`：`VK_SHIFT` (`0x10`) 按下（`& 0x80`）
    - `0x0002`：`VK_CONTROL` (`0x11`) 按下（`& 0x80`）
    - `0x0004`：`VK_MENU` / Alt (`0x12`) 按下（`& 0x80`）
    - `0x0008`：`VK_CAPITAL` / CapsLock (`0x14`) 啟用（`& 0x01`）
    - `0x0010`：`VK_KANA` (`0x15`) 啟用（`& 0x01`）
  - `+0x04` (`UINT`)：`uVirtKey`（`ImeToAsciiEx` 中為 `uVirtKey & 0xffff`）
  - `+0x08` (`LPARAM`)：原始 `lParam`（`ImeToAsciiEx` 中為 `(longlong)(uScanCode << 16)`）

---

## 遺留問題（Unknowns，留待 Phase 2～3 解決）
1. `phon.ime` 橋接物件的 vtable `+0x18` (`0x7ff2e28cbd4`)、`+0x20` (`0x7ff2e289ea8`)、`+0x28` (`0x7ff2e28a2bc`) 內部還初始化了多個子元件（`DAT_7ff2e281750`、`DAT_7ff2e281780`），目前已知核心運算委派給 `IMTCCORE.DLL`（Mode ID = `3`），詳細按鍵分派到 `IMTCCORE.DLL` 哪個介面方法將於 Phase 3 追入 `IMTCCORE.DLL` 確認。
2. `IMTCCORE.DLL` 在 Mode ID = `3` 時實際開啟哪些外部詞庫檔（例如 `Windows\IME\IMETC10\DICTS\*`），留待 **Phase 2** 確認。
