# EDIFIER M16 Pro HID Report Descriptor 分析

本報告只處理 Interface 0 的 HID Report Descriptor，不重做 USB Audio 鑑識。結論先行：M16 Pro 並非只是「Interface class = HID」；317-byte descriptor 內有明確的 **Generic Desktop / Keyboard Application Collection**，並包含標準鍵盤 modifier、6-key array 與鍵盤 LED Output。這足以構成 descriptor 層級的 keyboard-like 證據。

但目前仍無法證明 PS5 是「因為先把 Interface 0 分類成鍵盤，所以停止建立 Interface 1/2 的 USB Audio」。新觀察讓這條因果鏈變得更可信，仍需 PS5 host-side enumeration trace 才能確認。

## Evidence and method

- 以標準、唯讀的 `GET_DESCRIPTOR(HID_REPORT)` 對 Interface 0 要求 317 bytes；沒有 claim interface、detach driver、OUT transfer 或 vendor command。結果見 [fetch log](../descriptors/m16/m16-hid-report-fetch.log)。
- 完整 raw binary 為 [m16-hid-report.bin](../descriptors/m16/m16-hid-report.bin)，長度 **317 bytes**，SHA-256：`e746d77ff0299b06cf43c21a95526c768bc6240b1e4d438109256627ac95503e`。可讀 hex 見 [m16-hid-report-hex.txt](../descriptors/m16/m16-hid-report-hex.txt)。
- 新取得的 317 bytes 與 macOS IOKit `ReportDescriptor` 逐 byte 比對完全相同，排除短讀或錯抓其他介面的可能。
- [完整 parser 輸出](../descriptors/m16/m16-hid-report-parsed.txt) 依 USB HID 1.11 的 short-item state、Global/Local/Main item、Collection stack 與 Report bit layout 解析全部 317 bytes；驗證結果為 `PASS`（全部 bytes consumed、Collection 平衡、Report layout 有效）。另以 Apple IOHID 的 `DeviceUsagePairs`、report size 與 element 結果交叉驗證。HID Report Descriptor 的 item 語義依 [USB-IF HID 1.11](https://www.usb.org/sites/default/files/hid1_11.pdf)，Usage 名稱依 [USB-IF HID Usage Tables 1.7](https://www.usb.org/sites/default/files/hut1_7.pdf)。
- 可重跑的工具原始碼：[HID fetcher](../tools/hid-report-fetch/m16-hid-report-fetch.c) 與 [HID parser](../tools/hid-report-parser/m16-hid-parse.py)。

## HID Topology

Interface 0 是 `03/00/00`（HID / no boot subclass / no boot protocol），HID descriptor 宣告 Report Descriptor 長度 `0x013D = 317`。因此它不是 Boot Keyboard protocol 裝置；host 必須解析 Report Descriptor 才知道功能。完整 USB descriptor 見 [m16-usb-descriptor.txt](../descriptors/m16/m16-usb-descriptor.txt)。

descriptor 有 7 個 Report ID、7 個 top-level Collection，另在 Mouse Application Collection 內有一個 Pointer Physical Collection。出現的 Usage Page 如下：

| Usage Page | 名稱 | 用途 |
|---|---|---|
| `0x01` | Generic Desktop | System Control、Mouse、Pointer、Keyboard、X/Y/Wheel |
| `0x07` | Keyboard/Keypad | modifier 與 6-key keycode array；另有 Feature |
| `0x08` | LED | keyboard lock LED 與 telephony/status LED Output |
| `0x09` | Button | Mouse button 1–8 |
| `0x0B` | Telephony Device | Phone、Hook Switch、Flash、Phone Mute |
| `0x0C` | Consumer | Consumer Control 與 media/volume controls |
| `0xFF01` | Vendor Defined | 63-byte Input / 63-byte Output payload |

所有 Input、Output、Feature 的完整 layout：

| Report ID | Collection | Input payload | Output payload | Feature payload | wire size（含 Report ID） |
|---:|---|---:|---:|---:|---|
| 1 | System Control | 1 byte | — | — | Input 2 bytes |
| 2 | Consumer Control | 2 bytes | — | — | Input 3 bytes |
| 3 | Mouse / Pointer | 4 bytes | — | — | Input 5 bytes |
| 4 | Keyboard | 7 bytes | 1 byte | 20 bytes | Input 8、Output 2、Feature 21 bytes |
| 5 | Vendor Defined `0xFF01` | 63 bytes | 63 bytes | — | Input/Output 64 bytes |
| 6 | Consumer Control | 1 byte | — | — | Input 2 bytes |
| 7 | Telephony / Phone | 1 byte | 2 bytes | — | Input 2、Output 3 bytes |

Apple IOHID 回報 `MaxInputReportSize=64`、`MaxOutputReportSize=64`、`MaxFeatureReportSize=21`，與上述 parser 結果完全一致。[IOKit evidence](../descriptors/m16/m16-ioreg-host-devices.txt)

## Top-level Collections

| # | descriptor offset | Collection type | Usage Page : Usage | Report ID |
|---:|---:|---|---|---:|
| 1 | `0x0004` | Application | `0x01:0x80` Generic Desktop / System Control | 1 |
| 2 | `0x001C` | Application | `0x0C:0x01` Consumer / Consumer Control | 2 |
| 3 | `0x0035` | Application | `0x01:0x02` Generic Desktop / Mouse | 3 |
| 4 | `0x006F` | Application | `0x01:0x06` Generic Desktop / **Keyboard** | 4 |
| 5 | `0x00B5` | Vendor-defined (`0x80`) | `0xFF01:0x00` | 5 |
| 6 | `0x00CF` | Application | `0x0C:0x01` Consumer / Consumer Control | 6 |
| 7 | `0x00F2` | Application | `0x0B:0x01` Telephony Device / Phone | 7 |

Mouse Collection 內另有 `0x01:0x01` Pointer 的 Physical Collection；其 Input 是 8 個 buttons、相對 X/Y 與相對 wheel。以上不是只看 top-level Usage 的概括，而是 parser 對所有 Collection/Main item 的結果；逐 item offset 與 flags 可在 [完整解析](../descriptors/m16/m16-hid-report-parsed.txt) 查核。

## Keyboard-related Usages

descriptor 的關鍵原始序列從 offset `0x006B` 開始：

```text
05 01       Usage Page (Generic Desktop)
09 06       Usage (Keyboard)
A1 01       Collection (Application)
85 04       Report ID (4)
05 07       Usage Page (Keyboard/Keypad)
```

Report ID 4 不是模糊的 vendor data，而是完整、標準形狀的 keyboard report：

- **Input bits 0–7**：Usage Page `0x07`，Usage `0xE0–0xE7`，即 LeftControl 到 Right GUI；8 個 1-bit modifier。
- **Input bits 8–55**：Usage Page `0x07`，Usage range `0x00–0xFF`；6 個 8-bit keycode array entries。
- **Output bits 0–2**：Usage Page `0x08`，Num Lock、Caps Lock、Scroll Lock；3 個 1-bit LED controls，另有 5 bits padding。
- **Feature bits 0–159**：Usage Page `0x07`、Usage `0x00`，20-byte data array；descriptor 沒有提供足以解釋其產品語義的 Usage。

因此，以下三件事是 descriptor 層級的確定事實：

1. 存在 `Usage Page 0x01 / Usage 0x06 / Application Collection`。
2. Collection 內切到 `Usage Page 0x07 Keyboard/Keypad`。
3. 其 Input/Output 結構實作 modifier、6-key key array 與 keyboard LED feedback。

它雖不是 HID Boot Keyboard（subclass/protocol 都是 0），但這不會消除 keyboard Application Collection 的語義；只代表 host 不能僅套用 boot protocol，必須解析 descriptor 與 Report ID 4。

## Consumer-control Usages

有兩個 Consumer Control Application Collection：

- **Report ID 2**：16-bit Data Array，Usage Page `0x0C`，Usage range `0x0000–0x0514`。它可表達廣泛 Consumer usages，但 descriptor 沒有指出裝置實際會送出其中哪些值。
- **Report ID 6**：8 個明確的 1-bit Input usages：Volume Increment (`0xE9`)、Volume Decrement (`0xEA`)、Mute (`0xE2`)、Play/Pause (`0xCD`)、Scan Next Track (`0xB5`)、Scan Previous Track (`0xB6`)、Fast Forward (`0xB3`)、Stop (`0xB7`)。

另有 Report ID 1 的 Generic Desktop / System Control Application Collection，1-byte array 可表示 Usage `0x01–0xB7`。Apple 將此列為裝置的 `PrimaryUsage = 0x80`，所以 macOS `hidutil` 的 device/service 摘要主要顯示 **System Control**，不是主要顯示 Keyboard。[hidutil device view](../descriptors/m16/m16-hidutil-device.txt)

## Vendor-defined Usages

唯一的 Vendor Defined Usage Page 是 `0xFF01`：

- top-level Collection type 為 `0x80`（Vendor-defined），Usage `0x0000`，Report ID 5。
- Input：63 × 8-bit Data Variable，payload 63 bytes，加 Report ID 後 64 bytes。
- Output：63 × 8-bit Data Variable，payload 63 bytes，加 Report ID 後 64 bytes。
- descriptor 沒有定義更具體的 vendor usages，因此不能從 raw descriptor 判斷 payload 內容。

本次沒有讀寫 Report ID 5、沒有發 vendor command，也沒有用其 Output channel。

## Why macOS classifies it as keyboard-like

### Confirmed evidence

- descriptor 明確宣告 `Generic Desktop / Keyboard` Application Collection，不是僅有 `bInterfaceClass=0x03`。
- 同一 Collection 內含 Keyboard/Keypad modifier、6-key array 與 keyboard lock LED Output，符合 host 建立鍵盤 element 的充分 descriptor 資料。
- Apple IOHID parser 的 `DeviceUsagePairs` 包含 `{"DeviceUsagePage"=1,"DeviceUsage"=6}`，並明確設置 `HIDKeyboardKeysDefined = Yes`。[IOKit evidence](../descriptors/m16/m16-ioreg-host-devices.txt)

### Strong inference

macOS 的 IOHID parser 確實解析了同一 HID interface 的多個 top-level Collection，並從 Keyboard Collection 建立 keyboard elements；這足以讓鍵盤相關處理路徑看見它，因此出現 Keyboard Setup 或 keyboard-like peripheral 行為是合理且有直接 descriptor 依據的解釋。

### Important limit

M16 Pro 的 **primary** Usage 是 System Control (`0x01:0x80`)；`hidutil list --matching keyboard` 沒有把整台 M16 列為 primary keyboard。換言之，最精確的說法是：它是帶有真實 Keyboard Application Collection 的 multi-function HID，會被 keyboard stack 看見；不是「整個 Interface 0 只有鍵盤功能」，也不是 boot keyboard。至於 Keyboard Setup Assistant 的精確觸發條件，現有證據沒有揭露 Apple 內部 heuristic，屬 unknown。

## Implication for PS5

問題是：「PS5 是否可能因 Interface 0 是 keyboard-like HID，而未繼續把 Interface 1/2 建立成 USB Audio device？」

### Confirmed evidence

- 實機觀察：PS5 插入 M16 Pro 時提示／辨識為「鍵盤」，但沒有建立 USB Audio Output。（這是本次新增的使用者實機觀察，非本機工具直接量測。）
- 同一裝置在 macOS 上會同時建立 HID 與 USB Audio，證明 M16 的 composite descriptor 並非在所有 host 上都只能選一種功能。
- USB configuration 中 Interface 0 是 HID，Interface 1 是 Audio Control，Interface 2 是 Audio Streaming；HID 排在 Audio interfaces 前。
- Interface 0 的 descriptor 確實含有完整 keyboard Application Collection。因此 PS5 的「鍵盤」提示有 descriptor 層級的直接解釋，不需要只靠 HID interface class 猜測。

### Strong inference

- **PS5 的 keyboard classification：高可信度。** PS5 很可能在解析 Interface 0 / Report ID 4 後，依 `0x01:0x06` Keyboard Collection 將這個 composite peripheral 歸入 keyboard-like 類別。新 descriptor 證據大幅強化了 HID-first / peripheral-classification 假說。
- **keyboard-first classification 與 Audio 未建立之間的因果：中等可信度。** 一個合理機制是 PS5 對此 VID:PID 或 configuration 先套用 peripheral category/filter，之後沒有為 Interface 1/2 建立 audio function；但現有資料只能證明兩件事同時發生，不能證明前者造成後者。

### Unknown

- PS5 是否在提示「鍵盤」後仍繼續讀完 Interface 1/2 的 Audio class-specific descriptors。
- PS5 是停止 composite enumeration、拒絕 Audio 1.0 topology、driver bind 失敗，還是套用了未公開的相容性白名單／黑名單。
- PS5 host log 中是否有 AudioControl/AudioStreaming probe、`SET_INTERFACE`、class request 或 failure code。
- 把 HID interface 移到 Audio 之後、移除 Keyboard Collection，或改成不同 VID:PID 是否就會讓 PS5 建立 Audio；未做這些侵入性／韌體變更測試，也不應把它們當成已證實解法。

### Reassessment

**可能，而且現在是有 descriptor 證據支持的強假說；但尚未 confirmed。** 可以確認的是「M16 的 Interface 0 真的 keyboard-like，且足以解釋 PS5 為何提示鍵盤」。尚不能確認的是「PS5 因此停止或拒絕 Interface 1/2」。若要把後半段升級為 confirmed evidence，最低限度需要 PS5 端或 USB bus analyzer 的 enumeration trace，證明它在 HID 分類後沒有 probe/bind Audio interfaces，或在該節點明確中止。
