# PS5 USB Audio 相容性鑑識：EDIFIER M16 Pro 為什麼被當成「鍵盤」？

> 狀態：**研究進行中 / Working Hypothesis**  
> 測試日期：2026-08-31  
> 主要樣本：EDIFIER M16 Pro  
> 對照樣本：HECATE G1500 BAR（PS5 已知可用，Descriptor 尚未取得）、EDIFIER Melo Bar（待測）  
> 測試平台：PS5 Slim、macOS 27.0

---

## TL;DR

EDIFIER M16 Pro 在 macOS / Windows 可正常作為 USB Audio 裝置，但直接插入 PS5 Slim 時，**PS5 不建立 USB Audio Output，反而把裝置辨識成鍵盤類 HID 周邊**。

最初最直覺的猜測，例如：

- UAC2-only
- 沒有 16-bit / 48 kHz PCM
- 使用 asynchronous endpoint
- 需要 EDIFIER 私有音訊驅動
- 麥克風 / capture topology 造成衝突

經實際 USB Descriptor dump 後，**幾乎全部被排除**。

M16 Pro 的 Audio function 反而非常標準：

- **USB Audio Class 1.0**
- **2-channel**
- **16-bit**
- **48 kHz**
- **PCM**
- **Adaptive Isochronous OUT**
- macOS 直接由 Apple 內建 USB Audio stack 驅動

HID Report Descriptor 也已完成取回與解析：

- 完整長度 **317 bytes**，SHA-256：`e746d77ff0299b06cf43c21a95526c768bc6240b1e4d438109256627ac95503e`
- 明確存在 `Usage Page 0x01 / Usage 0x06 / Collection (Application)` 的 Keyboard top-level collection
- Report ID 4 包含標準 keyboard modifiers、6-key array 與 Num/Caps/Scroll Lock LED Output
- 另有 Consumer Control、Mouse、Telephony 與 `0xFF01` Vendor Defined collections
- macOS IOHID 設置 `HIDKeyboardKeysDefined = Yes`，但整台裝置的 primary usage 仍是 System Control，不是純鍵盤

目前最值得驗證的主假說變成：

> **M16 Pro 是 HID-first Composite Device：Interface 0 是 HID，AudioControl / AudioStreaming 排在 Interface 1 / 2。PS5 可能在 peripheral classification 階段先把整個裝置分類成鍵盤 / HID，之後沒有正確建立 Audio function。**

這目前是**強推論，不是已證實的 PS5 內部行為**。真正要把它證明，需要 PASS 樣本的完整 Descriptor，或 PS5 enumeration USB trace。

## Repository guide

- [完整 M16 USB Audio 鑑識](./docs/m16-analysis.md)
- [M16 HID Report Descriptor 專題分析](./docs/m16-hid-analysis.md)
- [M16 raw descriptors 與 macOS evidence](./descriptors/m16/)
- [PASS / FAIL comparison matrix](./findings/comparison.md)
- [唯讀 descriptor 工具原始碼](./tools/)

---

# 1. 問題背景

家用場景：

```text
PS5 Slim
   │
   ├── HDMI ──> AOC Q27G3XMN
   │
   └── USB ───> EDIFIER M16 Pro
```

預期：

```text
PS5
  ↓
USB Audio Device
  ↓
M16 Pro
  ↓
有聲音
```

實際：

```text
PS5
  ↓
M16 Pro
  ↓
被辨識為鍵盤 / HID 類周邊
  ↓
USB Audio Output 不出現
```

同一台 M16 Pro 接到 Mac / PC：

```text
Mac / PC
   ↓ USB
M16 Pro
   ↓
USB Audio 正常
```

因此問題不是：

- M16 Pro 的 USB 音效卡壞掉
- USB 線完全無法傳輸資料
- 喇叭本體無法播放 USB Audio

而是：

> **PS5 USB Host 與 M16 Pro 的 Composite USB presentation / runtime behavior 之間存在相容性問題。**

---

# 2. 最關鍵的 Runtime 現象

M16 Pro 插入 macOS 後，macOS 會跳出：

> **鍵盤設定輔助程式**

也就是 macOS 的 HID subsystem 確實把 M16 Pro 的某個 HID collection 視為 keyboard-like peripheral。

同時 macOS 還是可以正常建立 USB Audio Output。

這代表 macOS 的處理比較像：

```text
M16 Pro
├── HID function       → Apple HID stack
├── AudioControl       → Apple USB Audio
└── AudioStreaming     → usbaudiod
```

而 PS5 的實際現象則是：

```text
M16 Pro
├── HID function       → 被辨識成鍵盤
├── AudioControl       → 沒有建立可見 Audio Output
└── AudioStreaming     → 沒有建立可見 Audio Output
```

這是目前「HID-first / peripheral classification」假說最重要的 runtime 支持。

---

# 3. Device Identity

實測裝置：

| Field | Value |
|---|---|
| Product | EDIFIER M16 Pro |
| Manufacturer | EDIFIER |
| VID | `0x2D99` |
| PID | `0xA020` |
| bcdUSB | `0x0110` |
| Speed | Full Speed, 12 Mb/s |
| Device Class | `0x00`，class defined per interface |
| Configurations | 1 |
| Interfaces | 3 |
| Power | Bus-powered |
| bMaxPower | 100 mA |
| Composite | Yes |

關鍵點：

> Device Class = `0x00`，代表 Host 必須依各 Interface 判斷裝置功能，而不是只看 Device Descriptor 就知道它是 Audio Device。

---

# 4. M16 Pro USB Interface Map

實際 Descriptor：

```text
EDIFIER M16 Pro  2D99:A020

├── Interface 0, Alt 0: HID
│   ├── class/subclass/protocol: 03/00/00
│   ├── EP 0x81 IN:  Interrupt, 64 bytes, interval 1 ms
│   └── EP 0x01 OUT: Interrupt, 64 bytes, interval 1 ms
│
├── Interface 1, Alt 0: AudioControl
│   ├── class/subclass/protocol: 01/01/00
│   ├── UAC1 Header → AudioStreaming Interface 2
│   ├── Input Terminal 1: USB Streaming, 2ch L/R
│   ├── Feature Unit 9: master mute + volume
│   └── Output Terminal 3: Speaker
│
└── Interface 2: AudioStreaming OUT
    ├── Alt 0: zero-bandwidth
    └── Alt 1:
        ├── PCM Type I
        ├── 2 channels
        ├── 16-bit
        ├── 48 kHz
        └── EP 0x02 OUT: Isochronous Adaptive
```

最醒目的結構：

```text
IF0 = HID
IF1 = AudioControl
IF2 = AudioStreaming
```

也就是：

> **HID-first Composite Device**

---

# 5. Audio Descriptor 其實非常「正常」

## USB Audio Class

```text
bcdADC = 0x0100
```

即：

> **USB Audio Class 1.0**

不是 UAC2。

---

## Audio Format

M16 Pro 唯一有效播放模式：

| Parameter | Value |
|---|---|
| Format | PCM Type I |
| Channels | 2 |
| Bit depth | 16-bit |
| Sample rate | 48,000 Hz |
| Direction | OUT |
| Endpoint | `0x02` |

原始 Format Type bytes：

```text
0b 24 02 01 02 02 10 01 80 bb 00
```

解析：

```text
FORMAT_TYPE_I
2 channels
2-byte subframe
16-bit
1 discrete sample rate
0x00BB80 = 48000 Hz
```

因此：

> M16 Pro 不只是「有 16/48 fallback」，而是 **唯一播放模式就是 16-bit / 48 kHz stereo PCM**。

---

# 6. Endpoint

Audio endpoint：

```text
07 05 02 09 c0 00 01
```

解析：

| Field | Value |
|---|---|
| Address | `0x02` |
| Direction | OUT |
| Transfer | Isochronous |
| Synchronization | **Adaptive** |
| Max packet | 192 bytes |
| Interval | 1 ms |

192 bytes 恰好等於：

```text
48 samples/ms × 2 channels × 2 bytes
= 192 bytes/ms
```

因此沒有看到 packet size 異常。

它也**不是 asynchronous endpoint**。

---

# 7. macOS Driver Binding

實測 macOS binding：

```text
IOUSBHostDevice
├── AppleUSBHostCompositeDevice
├── Interface 0 → AppleUserUSBHostHIDDevice
├── Interface 1 → AppleUSBAudioControlNub + usbaudiod
└── Interface 2 → usbaudiod
```

CoreAudio：

- USB output
- 2-channel
- 48 kHz
- 正常播放

所以：

> **M16 Pro Audio function 是 class-compliant UAC1。**

macOS 不需要 EDIFIER 私有 Kernel Extension / DriverKit Audio Driver 才能讓它播放。

---

# 8. 已經被排除的常見解釋

| Hypothesis | Result |
|---|---|
| UAC2-only | ❌ 排除 |
| 沒有 16-bit / 48 kHz | ❌ 排除 |
| Async endpoint | ❌ 排除 |
| Audio interface 是 Vendor-specific | ❌ 排除 |
| 一定需要 EDIFIER 私有 Audio Driver | ❌ 排除 |
| Speaker + Microphone topology 衝突 | ❌ 排除 |
| Descriptor packet size 明顯錯誤 | ❌ 未發現 |

這點很重要。

因為直覺上會以為：

> 「PS5 只吃 UAC1，所以 M16 大概是 UAC2。」

實測完全不是。

M16 已經是 UAC1，而且格式甚至比很多 DAC 更單純。

---

# 9. 目前第一嫌疑：HID-first Composite Classification

目前最值得驗證的模型：

```text
M16 Pro
Device Class = 0x00
      ↓
Host 依 Interface 分類
      ↓
Interface 0 = HID
      ↓
keyboard-like HID collection
      ↓
PS5 peripheral classifier
      ↓
「Keyboard」
      ↓
Audio interfaces 未建立成可用 Output？
```

其中最後兩步目前仍是推論。

但有四個重要支持：

1. **Descriptor 證實 IF0 是 HID。**
2. **HID Report Descriptor 證實 IF0 內有真正的 Keyboard Application Collection、modifier、6-key array 與 keyboard LED Output。**
3. **macOS 插入 M16 時會觸發 Keyboard Setup Assistant。**
4. **PS5 插入 M16 時也會把它辨識成鍵盤。**

而 macOS 與 PS5 最大行為差異是：

```text
macOS:
HID ✅
Audio ✅

PS5:
HID / Keyboard ✅
Audio ❌
```

因此問題很可能不在 Audio format 本身，而在：

> **Host 如何分類與綁定 Composite Device 的多個 Interface。**

---

# 10. 這仍然不是「已證實的 PS5 Bug」

目前不能直接宣稱：

> PS5 只看 Interface 0。

也不能直接宣稱：

> PS5 不支援 HID-first USB Audio Composite Device。

還缺至少一項強證據：

- 已知 PASS 裝置的完整 Descriptor
- PS5 enumeration USB trace
- 可控 USB Gadget A/B test

因此目前措辭應該是：

> **HID-first composite topology 是目前最有力的工作假說。**

而不是：

> **已找到 PS5 USB Audio root cause。**

---

# 11. G1500 BAR：下一個關鍵 PASS 樣本

HECATE G1500 BAR 已知有 PS5 實際使用案例，因此可作為 PASS 樣本。

目前可信狀態：

| Field | G1500 BAR |
|---|---|
| PS5 Audio | **PASS** |
| VID:PID | 待本機 dump 確認 |
| UAC version | UNKNOWN |
| Interface order | UNKNOWN |
| HID position | UNKNOWN |
| Audio endpoint | UNKNOWN |
| HID report | UNKNOWN |

**不要在沒有 raw descriptor 前把 G1500 的 Interface layout 當成已知事實。**

下一步最高資訊量操作：

> 買 / 借一台 G1500 BAR，用與 M16 Pro 相同工具 dump 完整 Device + Configuration + HID descriptors。

---

# 12. Melo Bar：真正想預測的目標

Melo Bar 是待測樣本。

目前已知產品功能包含：

- USB Audio
- 麥克風
- 按鍵
- 燈效
- TempoHub 控制

因此高度可能也是 Composite Device，但：

> **其實際 Interface layout 目前 UNKNOWN。**

Melo 到貨後優先確認：

```text
Interface 0 = ?
```

## Case A：Audio-first

如果得到：

```text
IF0 AudioControl
IF1 AudioStreaming OUT
IF2 AudioStreaming IN
IF3 HID
```

並且 PS5：

```text
USB Audio ✅
```

這會強烈支持：

> M16 的 HID-first presentation 是失敗因素之一。

## Case B：HID-first，但 PS5 仍 PASS

如果：

```text
IF0 HID
IF1 AudioControl
IF2 AudioStreaming
```

但：

```text
PS5 Audio ✅
```

那「只因 Interface 0 是 HID」這個假說就會被明顯削弱。

接下來要比的是：

- HID Top-Level Usage
- Keyboard usage
- Interface Association
- Audio Terminal Type
- Feature Unit
- Enumeration control response
- HID initialization
- descriptor ordering
- power / timing behavior

## Case C：HID-first，而且 PS5 FAIL

如果 Melo：

```text
IF0 HID
IF1 AudioControl
IF2 AudioStreaming
```

PS5 又：

```text
Keyboard ✅
Audio ❌
```

那會形成：

```text
M16 Pro:
HID-first
PS5 FAIL

Melo Bar:
HID-first
PS5 FAIL

G1500:
待 dump
PS5 PASS
```

此時 G1500 的 Descriptor 會變成決定性樣本。

---

# 13. 三機 Comparison Matrix

| Field | M16 Pro | Melo Bar | G1500 BAR |
|---|---|---|---|
| VID:PID | `2D99:A020` | UNKNOWN | UNKNOWN |
| bcdUSB / speed | `0110` / Full Speed | UNKNOWN | UNKNOWN |
| UAC version | UAC1 | UNKNOWN | UNKNOWN |
| 16/48 stereo | YES | UNKNOWN | UNKNOWN |
| PCM | YES | UNKNOWN | UNKNOWN |
| Async endpoint | NO, Adaptive | UNKNOWN | UNKNOWN |
| Composite | YES | UNKNOWN | UNKNOWN |
| Interface order | **HID 0 / AC 1 / AS 2** | UNKNOWN | UNKNOWN |
| HID | YES | UNKNOWN | UNKNOWN |
| Keyboard-like behavior on macOS | **YES** | UNKNOWN | UNKNOWN |
| PS5 sees keyboard | **YES** | UNKNOWN | UNKNOWN |
| PS5 Audio | **FAIL** | UNKNOWN | **PASS** |

---

# 14. HID Report Descriptor 結果

已用標準、唯讀的 `GET_DESCRIPTOR(HID_REPORT)` 取得完整 **317-byte** descriptor；沒有 claim interface、detach driver、OUT transfer 或 vendor command。新取得的 raw bytes 與 macOS IOKit 留存值逐 byte 一致。

七個 top-level collections：

| Report ID | Top-level Usage |
|---:|---|
| 1 | Generic Desktop / System Control |
| 2 | Consumer / Consumer Control |
| 3 | Generic Desktop / Mouse |
| 4 | Generic Desktop / **Keyboard** |
| 5 | Vendor Defined `0xFF01` |
| 6 | Consumer / Consumer Control |
| 7 | Telephony / Phone |

鍵盤證據不是從 `bInterfaceClass=HID` 推測而來，而是 descriptor 明文：

```text
05 01       Usage Page (Generic Desktop)
09 06       Usage (Keyboard)
A1 01       Collection (Application)
85 04       Report ID (4)
05 07       Usage Page (Keyboard/Keypad)
```

Report ID 4 實作 8 個 modifier bits、6 個 8-bit keycodes、3 個 keyboard LED outputs 與 20-byte Feature payload。完整逐 item 解析與 confirmed / inference / unknown 邊界見 [HID 專題分析](./docs/m16-hid-analysis.md)。

這足以確認 M16 是 **keyboard-like multi-function HID**，也足以解釋 macOS／PS5 的鍵盤提示；但「PS5 因此停止建立 Audio interfaces」仍是 strong inference，不是已確認因果。

---

# 15. 最終驗證：USB Gadget A/B Test

如果要把問題從「高可信推論」變成真正的實驗證明，可以建立兩個功能完全相同的 USB Gadget。

## Gadget A：Audio-first

```text
IF0 AudioControl
IF1 AudioStreaming
IF2 HID Keyboard
```

## Gadget B：HID-first

```text
IF0 HID Keyboard
IF1 AudioControl
IF2 AudioStreaming
```

兩者保持：

- 相同 UAC version
- 相同 16/48 PCM
- 相同 endpoint
- 相同 Feature Unit
- 相同 HID report
- 相同 VID/PID strategy
- 唯一主要變數：**Interface ordering**

接入同一台 PS5。

如果結果：

```text
A → Audio PASS
B → Keyboard only / Audio FAIL
```

那就能相當有力地證明：

> **PS5 USB peripheral classification 對 Composite Device interface ordering 存在 undocumented behavior / limitation。**

---

# 16. 可以使用的實驗平台

可考慮：

- Raspberry Pi Zero / Zero 2 W USB Gadget
- Linux ConfigFS USB Gadget
- Facedancer
- RP2040 / TinyUSB
- STM32 USB Device

初期最方便的是 Linux ConfigFS，因為可以快速改：

```text
functions/
configs/
interface ordering
UAC
HID
```

而不用反覆刷 MCU 韌體。

---

# 17. 如果要做 PS5 Enumeration Trace

只有在：

- M16 FAIL
- G1500 PASS
- Descriptor 靜態差異仍無法解釋

時才值得進這一步。

目標是觀察 PS5 對裝置送出的：

```text
GET_DESCRIPTOR
SET_CONFIGURATION
SET_INTERFACE
GET_CUR
GET_MIN
GET_MAX
GET_RES
SET_CUR
HID GET_REPORT
HID SET_REPORT
```

以及：

- STALL
- timeout
- reset
- alternate setting selection
- Audio endpoint 是否曾被啟用

這才可能直接看到：

> PS5 在哪一個 request 後停止建立 Audio function。

---

# 18. 不值得先做的事情

目前**不建議**：

- 刷 M16 firmware
- 修改 M16 firmware
- 一開始就用 Ghidra 拆 TempoHub
- 先逆 EDIFIER DSP protocol
- 先假設 Sony 有 VID/PID whitelist
- 先做 UAC1 → UAC2 proxy

因為現在最需要的是：

> **更多 PASS / FAIL Descriptor 對照資料。**

不是把工程規模直接膨脹成 USB 音訊核武器。

---

# 19. Evidence 等級

為避免把腦補寫成結論，本研究建議使用三個標籤：

### CONFIRMED

由 raw descriptor、IOKit binding、CoreAudio 或實機直接觀察。

例如：

```text
M16 IF0 = HID
M16 UAC1
M16 16/48 PCM
PS5 sees M16 as keyboard
PS5 Audio FAIL
```

### STRONG INFERENCE

有多個 observation 支持，但尚未取得 Host trace。

例如：

```text
PS5 的 peripheral classification
可能被 HID-first presentation 影響。
```

### UNKNOWN

目前沒有直接資料。

例如：

```text
Sony 是否使用 VID/PID whitelist
G1500 的 Interface ordering
Melo 的 UAC version
```

---

# 20. Current Conclusion

截至目前，最重要的發現不是：

> 「M16 USB Audio 規格不符合 PS5。」

而恰恰相反：

> **M16 Pro 的 Audio function 本身非常標準，但整個 USB Composite Device 的 presentation 可能讓 PS5 在 Audio stack 建立之前就先把它分類成 keyboard-like HID peripheral。**

現在可以進一步確認：M16 的 keyboard-like 特徵存在於 descriptor 本身，而不是只由 HID interface class 或 UI 提示推定。不能確認的仍是 PS5 內部是否因這個分類而略過 Audio interfaces。

目前嫌疑排序：

1. **HID-first Composite / peripheral classification**
2. Enumeration-time control behavior
3. HID runtime initialization / report topology
4. 其他 undocumented PS5 host-side filter
5. VID/PID / licensed-device rule

以下原因目前已大致排除：

- UAC2-only
- 缺少 16/48 PCM
- asynchronous endpoint
- vendor-specific Audio interface
- microphone capture topology

---

# 21. Next Actions

```text
[x] Dump M16 full HID Report Descriptor
[x] Parse Keyboard / Consumer / Telephony / Vendor usages
[ ] Dump Melo Bar descriptors
[ ] Test Melo Bar on PS5
[ ] Dump G1500 BAR descriptors
[ ] Compare PASS vs FAIL topology
[ ] Build ConfigFS Audio-first / HID-first A/B gadget
[ ] Only if needed: capture PS5 enumeration traffic
```

---

# 22. Repository 結構

```text
ps5-usb-audio-research/
│
├── README.md
├── docs/
│   ├── m16-analysis.md
│   └── m16-hid-analysis.md
│
├── descriptors/
│   └── m16/
│       ├── m16-usb-descriptor.txt
│       ├── m16-hid-report.bin
│       ├── m16-hid-report-hex.txt
│       ├── m16-hid-report-parsed.txt
│       └── macOS evidence files
│
├── tools/
│   ├── usb-descriptor-dump/
│   ├── hid-report-fetch/
│   └── hid-report-parser/
│
└── findings/
    └── comparison.md
```

尚未取得 evidence 的裝置與 gadget tests 不先建立空目錄；有實際 dump 或實驗後再加入，避免用 placeholder 製造虛假的完成感。

---

# 23. Reproducibility

M16 Pro 第一輪鑑識：

- macOS 27.0
- 唯讀操作
- 未使用 `sudo`
- 未修改 firmware
- 未修改 macOS USB stack
- Audio descriptor 透過既有 `libusb` 取得
- M16 VID:PID：`2D99:A020`

建議後續所有樣本記錄：

```text
Device:
Firmware:
VID:PID:
Host:
PS5 system software:
USB port:
Cable:
Cold boot result:
Hot plug result:
Sleep/resume result:
Audio output visible:
Keyboard notification:
```

避免把不同 Firmware / Cable / Port / PS5 系統版本混成同一個結論。

---

# 24. Research Status

**目前：沒有宣稱已找到 PS5 root cause。**

但已有足夠證據把研究問題從：

> 「為什麼這顆 USB 喇叭不能在 PS5 用？」

縮小成：

> **「為什麼一個標準 UAC1 / 16-bit / 48 kHz 的 Composite USB Audio Device，在 HID-first presentation 下會被 PS5 當成鍵盤而沒有建立 Audio Output？」**

這已經是一個可以被明確測試、反駁、重現的工程問題。

而這也是下一階段真正值得研究的地方。

---

## Source

本 README 主要依據：

- `EDIFIER M16 Pro USB Audio / PS5 Compatibility Analysis`
- M16 Pro raw USB configuration descriptor
- M16 Pro 317-byte HID Report Descriptor 與完整 parser output
- macOS IOKit / CoreAudio binding
- PS5 實機 FAIL 行為
- macOS / PS5 對 M16 的 keyboard-like runtime observation

對 Melo Bar、G1500 BAR 尚未取得 raw descriptor 的欄位，一律維持 `UNKNOWN`，避免把推測寫成事實。
