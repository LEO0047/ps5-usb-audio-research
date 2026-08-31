# EDIFIER M16 Pro USB Audio / PS5 Compatibility Analysis

> 此文件是取得完整 HID Report Descriptor 前的第一輪 USB Audio 鑑識。HID-first 假說的最新 descriptor 證據與重評估，以 [M16 HID 專題分析](./m16-hid-analysis.md) 為準。

鑑識日期：2026-08-31（Asia/Taipei）  
Mac 測試環境：macOS 27.0，Build 26A5421a  
執行邊界：唯讀；未使用 `sudo`，未安裝套件，未修改韌體或 macOS 設定。

## Evidence index

- `[D]` [`m16-usb-descriptor.txt`](../descriptors/m16/m16-usb-descriptor.txt)：libusb 結構化解析與完整 140-byte configuration raw descriptor。
- `[I]` [`m16-ioreg-host-devices.txt`](../descriptors/m16/m16-ioreg-host-devices.txt)：IOKit device tree、composite、interface 與 driver owner。
- `[IF]` [`m16-ioreg-host-interfaces.txt`](../descriptors/m16/m16-ioreg-host-interfaces.txt)：依 `IOUSBHostInterface` 類別取得的介面樹。
- `[A]` [`m16-system-profiler-audio.txt`](../descriptors/m16/m16-system-profiler-audio.txt)：CoreAudio 裝置狀態。
- `[C]` [`m16-ioreg-apple-usb-audio-control.txt`](../descriptors/m16/m16-ioreg-apple-usb-audio-control.txt)：AppleUSBAudio control binding。
- `[E]` [`m16-ioreg-apple-usb-audio-engine.txt`](../descriptors/m16/m16-ioreg-apple-usb-audio-engine.txt)：`AppleUSBAudioEngine` 查詢結果；檔案為 0 bytes。
- `[P]` [`m16-usbaudiod-process.txt`](../descriptors/m16/m16-usbaudiod-process.txt)：Apple 系統 `usbaudiod` process。
- `[SP-USB]` [`m16-system-profiler-usb.txt`](../descriptors/m16/m16-system-profiler-usb.txt)：指定的 `system_profiler SPUSBDataType` 命令成功結束，但本機輸出為 0 bytes；不能據此推論裝置不存在。
- 未收錄 host-wide `ioreg -p IOUSB` 全樹輸出，因為它包含與本研究無關的本機 session／系統資料；本報告沒有關鍵判斷依賴該兩份未收錄檔案。

引用格式例如 `[D:90-94]` 表示上列 `[D]` 檔案第 90 至 94 行。所有關鍵判斷都區分為「確認」、「推論」或 `UNKNOWN`。

## 1. Executive Summary

### 結論

M16 Pro 的靜態 USB Audio descriptor **沒有出現常見的 PS5 不相容原因**：它不是 UAC2-only；它就是 UAC1，且唯一播放模式正是 2-channel / 16-bit / 48 kHz / PCM，端點也不是 asynchronous，而是 adaptive isochronous OUT。[D:90-94, 143-165]

因此，目前不能把失敗歸因於「UAC 版本太新」、「缺少 16/48 fallback」或「async clock」。Mac 證據所能支持的最可能排序是：

1. **PS5 host-side 接受／分類規則，超出公開的 UAC1 基線（H10）— Medium**  
   這是排除明顯 descriptor 問題後的最佳剩餘解釋，但沒有 Sony 文件或 USB trace 可證明白名單、VID/PID 特例或確切篩選條件，因此只能列為推論，不能宣稱「PS5 有白名單」。

2. **HID-first composite topology 觸發 PS5 的 peripheral classifier 或介面選擇問題（H5）— Medium-Low**  
   裝置是三介面的 composite；Interface 0 先出現一個功能很廣的雙向 HID，AudioControl 與 AudioStreaming 才在 Interface 1、2。[D:37-77, 79-168] 這是目前最突出的 descriptor 差異點，但 composite USB Audio 本身是合法且常見的，尚無 PS5 trace 證明它就是拒絕點。

3. **enumeration 時的標準 UAC control response 或 HID initialization 行為不合 PS5 預期（H4/H7）— Low**  
   靜態 descriptor 宣告 Feature Unit mute/volume 與 endpoint sampling-frequency control；Mac 會同時啟動 Apple HID 與 USB Audio stack。[D:113-122, 166-172; I:65-88, 247-315] Mac dump 無法觀察 PS5 發出的 control request、STALL、timeout 或初始化順序。

### 目前最重要的反證

| 常見猜測 | 實測結果 | 判定 |
|---|---|---|
| UAC2-only | `bcdADC = 0x0100` | 反駁 |
| 沒有 16-bit/48 kHz stereo | 唯一模式就是 2ch / 16-bit / 48 kHz PCM | 反駁 |
| asynchronous endpoint | `bmAttributes = 0x09`，解析為 Adaptive Isochronous | 反駁 |
| Audio 使用 vendor protocol | Audio interface class/subclass/protocol = `01/01/00` 與 `01/02/00` | 反駁 |
| speaker + microphone topology | 只有 playback OUT，沒有 capture AudioStreaming interface | 反駁 |

## 2. Device Identity

| Field | Value | Evidence |
|---|---:|---|
| USB Product Name | EDIFIER M16 Pro | D:18-20 |
| Manufacturer | EDIFIER | D:18 |
| Serial Number | EDI00000X06 | D:20 |
| VID | `0x2D99` (11673) | D:15 |
| PID | `0xA020` (40992) | D:16 |
| bcdDevice | `0x0000` | D:17 |
| bcdUSB | `0x0110` (USB 1.1 descriptor) | D:10 |
| Negotiated speed | Full Speed, 12 Mb/s | D:3-4; I:4-5 |
| Device Class | `0x00` — class defined per interface | D:11 |
| Device SubClass | `0x00` | D:12 |
| Device Protocol | `0x00` | D:13 |
| Configurations | 1 | D:21 |
| Interfaces | 3 | D:40-41 |
| Configuration | value 1 | D:42 |
| Power model | Bus-powered (`bmAttributes 0x80`) | D:44 |
| bMaxPower | raw `0x32` = 50 × 2 mA = **100 mA** | D:45-46 |
| Composite | Yes; `AppleUSBHostCompositeDevice` 綁定 | I:37-50 |

完整 Device Descriptor raw bytes：[D:23-25]。  
完整 Configuration Descriptor raw bytes（140 bytes）：[D:26-35]。

## 3. USB Interface Map

```text
EDIFIER M16 Pro  2D99:A020
├── Interface 0, Alt 0: HID
│   ├── class/subclass/protocol: 03/00/00
│   ├── EP 0x81 IN:  Interrupt, 64 bytes, interval 1 ms
│   └── EP 0x01 OUT: Interrupt, 64 bytes, interval 1 ms
├── Interface 1, Alt 0: AudioControl
│   ├── class/subclass/protocol: 01/01/00
│   ├── UAC1 Header → AudioStreaming Interface 2
│   ├── Input Terminal 1: USB Streaming, 2ch L/R
│   ├── Feature Unit 9: master mute + volume
│   └── Output Terminal 3: Speaker
└── Interface 2: AudioStreaming OUT
    ├── Alt 0: zero-bandwidth, no endpoint
    └── Alt 1: PCM Type I, 2ch, 16-bit, 48 kHz
        └── EP 0x02 OUT: Isochronous Adaptive, 192 bytes/frame, interval 1 ms
```

確認事項：

- **HID：有。** IOKit 綁定 `AppleUserUSBHostHIDDevice`。[I:52-88]
- **Vendor Specific interface：沒有。** 三個 interface class 分別是 HID `0x03`、Audio `0x01`、Audio `0x01`。[D:49-55, 79-85, 124-139]
- HID report 內部有 vendor-defined usage page，且 IOKit 還解析出 Generic Desktop、Consumer 與 Telephony 等多組 usage；這不等於存在 vendor-specific USB interface。[I:86, 200-205]
- **Capture / microphone AudioStreaming：沒有。** AC Header 的 collection 只有 Interface 2，而 Interface 2 只有 OUT endpoint。[D:90-94, 124-168]
- AudioControl 位於 Interface 1、AudioStreaming 位於 Interface 2；Interface 0 被 HID 佔用。

## 4. USB Audio Class

### UAC version

- `bcdADC = 0x0100`，即 **USB Audio Device Class 1.0**。[D:88-94]
- USB-IF UAC1 規格定義 `bcdADC` 為 Audio Device Class release number，並在規格範例中直接把 `0x0100` 標為 Release 1.0：[USB-IF Audio Device Class 1.0](https://www.usb.org/sites/default/files/audio10.pdf)。
- UAC2 Clock Source / Clock Selector descriptor：**不存在，且 UAC1 不要求使用 UAC2 clock entities。**

### AudioControl topology

```text
USB Streaming Input Terminal 1 (0x0101, 2ch, L/R)
    ↓
Feature Unit 9 (master Mute + Volume)
    ↓
Speaker Output Terminal 3 (0x0301)
```

| Entity | Key fields | Evidence |
|---|---|---|
| AC Header | `bcdADC=0100`, total 40, one AS interface = 2 | D:88-94 |
| Input Terminal | ID 1, type `0101`, 2 channels, channel config `0003` | D:95-104 |
| Feature Unit | ID 9, source 1, master controls `0x03`, per-channel controls `0x00` | D:113-122 |
| Output Terminal | ID 3, type `0301` Speaker, source 9 | D:105-112 |

`bmaControls[0] = 0x03` 對應 master Mute 與 Volume。USB-IF UAC1 規格的 Feature Unit selector 表列 Mute=`0x01`、Volume=`0x02`：[USB-IF Audio Device Class 1.0](https://www.usb.org/sites/default/files/audio10.pdf)。

### AudioStreaming modes

只有一個有頻寬的播放模式：

| IF / Alt | Direction | Format | Channels | Subframe / bit depth | Sample rates | Endpoint |
|---|---|---|---:|---|---|---|
| 2 / 0 | — | zero-bandwidth | — | — | — | none |
| 2 / 1 | OUT | PCM Type I (`wFormatTag 0x0001`) | 2 | 2 bytes / 16 bits | 48000 Hz only | `0x02` |

原始 Format Type bytes：

```text
0b 24 02 01 02 02 10 01 80 bb 00
```

解析：`FORMAT_TYPE_I`, 2 channels, 2-byte subframe, 16-bit, one discrete rate, `0x00BB80 = 48000`。[D:147-155] USB-IF UAC1 規格也定義 Type I descriptor 的相同欄位結構：[USB-IF Audio Device Class 1.0](https://www.usb.org/sites/default/files/audio10.pdf)。

**16-bit / 48 kHz stereo fallback：存在，而且它不是 fallback 之一，而是裝置唯一宣告的播放格式。**

## 5. Endpoint Analysis

| Interface / Alt | Address | Direction | Transfer | Sync | Usage | Max packet | Interval | Extra |
|---|---:|---|---|---|---|---:|---:|---|
| HID 0/0 | `0x81` | IN | Interrupt | n/a | HID data | 64 bytes | 1 ms | none |
| HID 0/0 | `0x01` | OUT | Interrupt | n/a | HID data | 64 bytes | 1 ms | none |
| AudioStreaming 2/1 | `0x02` | OUT | Isochronous | **Adaptive** | Data | 192 bytes | 1 ms | CS EP General |

Audio endpoint raw descriptor：

```text
07 05 02 09 c0 00 01
```

- `0x02`：host-to-device OUT。
- `0x09`：Isochronous + Adaptive + Data endpoint。[D:156-165]
- `0x00C0 = 192` bytes；剛好等於 `48 samples/ms × 2 channels × 2 bytes = 192 bytes/ms`。
- `bInterval = 1`：Full-Speed 每 frame 一次。
- 無 feedback endpoint；adaptive sink 由裝置調整到 host 提供的資料率。

Class-specific endpoint raw descriptor：

```text
07 25 01 01 01 01 00
```

解析為 Sampling Frequency Control enabled、lock delay unit=milliseconds、lock delay=1。[D:166-172]

USB-IF UAC1 規格明確把 Adaptive 定義為合法同步類型：endpoint 可在操作範圍內調整自身資料率以匹配介面強加的資料率；因此 `Adaptive` 本身不是 descriptor 違規：[USB-IF Audio Device Class 1.0](https://www.usb.org/sites/default/files/audio10.pdf)。

## 6. macOS Driver Binding

### 實測 binding

```text
IOUSBHostDevice
├── AppleUSBHostCompositeDevice
├── Interface 0 → AppleUserUSBHostHIDDevice
├── Interface 1 → AppleUSBAudioControlNub + usbaudiod
└── Interface 2 → usbaudiod
```

- `AppleUSBAudioControlNub` 的 bundle identifier 是 `com.apple.driver.AppleUSBAudio`，provider 是 `IOUSBHostInterface`，match class/subclass 是 AudioControl `1/1`。[C:1-13]
- Interface 1 與 2 的 `UsbExclusiveOwner` 都是 Apple 系統 `usbaudiod`。[I:234-315]
- `usbaudiod` 執行檔來自 `/System/Library/Audio/Plug-Ins/usbaudio.bundle/Contents/MacOS/usbaudiod`。[P:1]
- CoreAudio 把它列為 USB、2-channel、目前 48000 Hz 的預設輸出裝置。[A:13-21]
- `ioreg -r -c AppleUSBAudioEngine` 沒有找到 registry object，因此 `[E]` 是空檔。這只表示目前 macOS 版本沒有以該舊 class name 暴露 engine object；不能反過來說沒有 Apple USB Audio engine，因為 AppleUSBAudio control nub 與 `usbaudiod` ownership 已直接證明 Apple class stack 正在使用。

### 判定

M16 Pro 的 Audio 部分是 **class-compliant UAC1，直接由 Apple 內建 USB Audio stack 驅動**。沒有發現 EDIFIER kernel extension、DriverKit audio driver或 vendor-specific Audio interface。

Mac 能使用它的原因並不是額外 EDIFIER audio driver，而是 macOS 接受這組標準 UAC1 AudioControl / AudioStreaming descriptor，並由 AppleUSBAudio + `usbaudiod` 建立 CoreAudio 裝置。

## 7. PS5 Failure Hypotheses

### 可靠的 PS5 profile 邊界

Sony 公開文件沒有提供通用 USB DAC 的 UAC version、bit depth、sample rate、sync type 或 VID/PID acceptance matrix：

- Sony 只公開表示「多數 USB 與類比 headset」可用，並要求使用者向製造商確認相容性：[PS5: The Ultimate FAQ](https://blog.playstation.com/2020/11/09/ps5-the-ultimate-faq/comment-page-10/)。
- PS5 Slim 官方 USB port guide 列出 port speed 與官方／授權周邊用法，但沒有通用 USB Audio descriptor 規格：[PS5 USB ports guide](https://www.playstation.com/en-gb/support/hardware/ps5-usb-ports-guide/)。

可用的高品質廠商實測資料是 JDS Labs：

- JDS 表示 PS4/PS5 使用 UAC1 input，UAC2 DAC 需切換或 fallback 到 UAC1：[JDS Labs FAQ](https://jdslabs.com/support/faq/)。
- 其 PS5-tested firmware 紀錄顯示除了 UAC1/UAC2，OS volume control、standby、host error handling 與 power declaration 也會影響主機相容性；部分 UAC1 fallback 版本會強制 16-bit：[JDS Labs firmware matrix](https://blog.jdslabs.com/2020/08/custom-firmware-builds-for-element-ii-el-dac-ii-and-atom-dac/)。

這些是廠商實測，不是 Sony 規範。因此：

- **PS5 主要有通用 UAC1 support：有高品質廠商證據。**
- **通用 UAC2 support：Sony 未公開；JDS 的實測說法是 PS5 只提供 UAC1 driver。** 不把這擴張為所有 firmware／所有裝置的絕對定律。
- **通用 sample-rate / bit-depth matrix：UNKNOWN。** 已知成功廠商 profile 至少包含 UAC1 16/48 或 24/48；M16 的 16/48 不超出這個實測範圍。
- **是否限制 asynchronous endpoint：UNKNOWN。** 沒找到 Sony 技術文件；而 M16 實際是 adaptive，不是 async。

### H1–H10 evidence matrix

| Hypothesis | 證據支持 | 證據反對 | 判定 |
|---|---|---|---|
| **H1 UAC2-only，無 UAC1 fallback** | 無 | `bcdADC=0x0100`，明確為 UAC1 | **反駁 / High** |
| **H2 沒有 2ch/16-bit/48k PCM** | 無 | 唯一 active mode 就是 2ch/16-bit/48k PCM | **反駁 / High** |
| **H3 async endpoint / clocking 不被接受** | 無 | endpoint `bmAttributes=0x09` = adaptive；不是 async | **就 async 部分反駁 / High**；PS5 是否排斥此 adaptive 實作仍 UNKNOWN |
| **H4 AC/AS descriptor 非典型** | Interface order 是 HID 先於 Audio；`bcdDevice=0000` | descriptor length、entity source link、format、packet size互相一致，沒有看到靜態違規 | **靜態 descriptor 支持度低；runtime control 行為 UNKNOWN** |
| **H5 composite 太複雜，PS5 選錯 interface** | Composite；HID Interface 0 在前，且有雙向 endpoint 與廣泛 report usages | 只有 3 個 interface；AC Header 明確把 AS 指到 Interface 2；沒有 vendor interface | **可能 / Medium-Low** |
| **H6 Audio interface 用 vendor-specific protocol** | 無 | Audio interfaces 是 `01/01/00` 與 `01/02/00` | **反駁 / High** |
| **H7 要 vendor command/HID init 才啟用 Audio** | HID report 含 vendor-defined usage，Mac 同時啟動 HID stack | 沒有 vendor-specific interface；Apple Audio driver可直接 bind | **UNKNOWN / Low**；需 clean-boot USB trace |
| **H8 bus power / enumeration 行為不合** | 裝置宣告 bus-powered；實際電流未知 | 只宣告 100 mA，且在 Mac 以 Full Speed 成功 enumerate；使用者觀察 PS5 有供電 | **支持度低**；實際 draw、inrush、PS5 port trace UNKNOWN |
| **H9 speaker + mic/capture topology 讓 PS5 拒絕** | 無 | 只有一個 playback OUT stream，沒有 capture AS interface | **反駁 / High** |
| **H10 host whitelist / VID-PID / 其他限制** | 合法、簡單的 UAC1 16/48 descriptor 仍在 PS5 失敗；Sony 未公開通用 acceptance matrix | 沒有 PS5 USB trace、Sony whitelist 文件或對照 descriptor | **可能 / Medium**；白名單本身仍 UNKNOWN |

## 8. Most Likely Root Cause

### 可下的結論

**M16 Pro 並不是因為 UAC2-only、缺 16/48 PCM、asynchronous endpoint、vendor-specific Audio interface 或 speaker+mic topology而被 PS5 拒絕。** 這些說法都被 raw descriptor 反駁。[D:88-94, 124-172]

### 目前最佳推論

最可能是 **PS5 host stack 的裝置分類／接受條件與 M16 的 HID-first composite presentation 或 enumeration-time control 行為互動不良**。理由是：

1. Audio function 本身落在已知 PS5 廠商實測的 UAC1、48 kHz 範圍內。
2. Audio descriptor 的 length、topology、packet size 與 format 相互一致，未見可直接指出的 malformed byte。
3. 相較於最小 UAC1 DAC，M16 最顯著的額外部分是 Interface 0 的複合 HID：雙向 interrupt endpoint、317-byte HID report descriptor、Generic Desktop／Consumer／Telephony／vendor usage。[D:49-77; I:86, 200-205]
4. JDS 的 PS5 firmware 記錄顯示，UAC1 class identity 以外的 standby、volume/control 與 host handling 細節也能決定實際主機相容性；因此「descriptor 看似標準」仍不足以保證 PS5 接受。

信心只有 **Medium-Low 到 Medium**，因為這是排除法推論，不是 PS5 bus trace 的直接證明。不能把「可能的 host-side filter」寫成「已證實白名單」。

### 沒看到的 descriptor 異常

- Configuration `wTotalLength=140` 與 raw stream 長度一致。
- AC `wTotalLength=40` 與 Header + Input Terminal + Output Terminal + Feature Unit 長度一致。
- AS terminal link 1 能連回 USB Streaming Input Terminal 1。
- Feature Unit 9 的 source 1、Output Terminal 3 的 source 9 能形成完整播放鏈。
- 192-byte packet 與 48 kHz × stereo × 16-bit 精確吻合。
- 無多餘 AudioStreaming capture interface、無 vendor-specific USB interface。

## 9. What We Still Cannot Know

只靠這台 Mac 的靜態 descriptor 無法證明：

1. PS5 enumeration 的哪一個 request 失敗、STALL 或 timeout。
2. PS5 是否先把 Interface 0 當成 input peripheral，再拒絕或忽略 Audio function。
3. PS5 是否針對 Terminal Type、HID usage、VID/PID、licensed device、interface order 或 Feature Unit controls 做額外篩選。
4. M16 在 cold power-up 後是否必須收到某個 HID output／feature report 才完成 Audio 初始化。
5. M16 對 UAC `GET_CUR / GET_MIN / GET_MAX / GET_RES / SET_CUR`（volume、mute、sample frequency）的實際回應是否完全合規；本輪未 claim interface 或干擾 Apple driver。
6. PS5 port 的實際 VBUS、inrush、reset timing 與 M16 實際電流；100 mA 只是 descriptor 宣告值。
7. PS5 system software 版本差異是否改變 acceptance behavior。
8. Sony 是否使用白名單；目前沒有直接證據。

最小、最有資訊量的下一步不是刷 firmware，而是：

1. 先 dump 一台已知在同一台 PS5、同一連接方式下 PASS 的 G1500 BAR。
2. 對照 raw configuration、interface order、terminal type、Feature Unit、HID report 與 endpoint。
3. 若兩者靜態 descriptor 仍無可解釋差異，再做 PS5 enumeration USB protocol capture；這才可看到真正的 STALL／timeout／host decision point。

## 10. Melo Bar Comparison Plan

Melo Bar 到手後，使用相同方法保存：

- Device descriptor：VID/PID、bcdUSB、bcdDevice、device class triple、string descriptors。
- Configuration descriptor raw bytes：configuration 數、interface 數、power attributes、bMaxPower。
- Interface ordering：HID / AudioControl / AudioStreaming / Vendor Specific 的 number、class、subclass、protocol。
- AC entities：bcdADC、Header collection、Terminal Type、Feature Unit controls、Clock Source/Selector（若為 UAC2）。
- 每個 AS alternate setting：format tag、channels、subslot/subframe、bit depth、sample rates。
- 每個 endpoint：address、direction、transfer、sync、usage、wMaxPacketSize、interval、feedback relation。
- HID：report descriptor length、top-level usage pairs、vendor-defined usages、IN/OUT endpoint。
- 實測條件：同一台 PS5、同一個 port/轉接器、cold boot/replug 結果與 system software version。

### Required comparison template

| Field | M16 Pro | Melo Bar | G1500 BAR |
|---|---|---|---|
| VID:PID | `2D99:A020` | UNKNOWN | UNKNOWN |
| bcdUSB / speed | `0110` / Full Speed | UNKNOWN | UNKNOWN |
| bcdDevice | `0000` | UNKNOWN | UNKNOWN |
| UAC version | UAC1 (`0100`) | UNKNOWN | UNKNOWN |
| Audio Protocol | `01/01/00`, `01/02/00` | UNKNOWN | UNKNOWN |
| 16/48 stereo | YES; only active mode | UNKNOWN | UNKNOWN |
| PCM format | Type I PCM (`0001`) | UNKNOWN | UNKNOWN |
| Async endpoint | NO; Adaptive | UNKNOWN | UNKNOWN |
| Audio endpoint | `0x02` OUT, Iso Adaptive, 192 B, 1 ms | UNKNOWN | UNKNOWN |
| Terminal path | USB Streaming → Feature Unit → Speaker | UNKNOWN | UNKNOWN |
| Feature controls | master mute + volume | UNKNOWN | UNKNOWN |
| Composite | YES; 3 interfaces | UNKNOWN | UNKNOWN |
| Interface order | HID 0, AC 1, AS 2 | UNKNOWN | UNKNOWN |
| HID | YES; bidirectional, broad usage set | UNKNOWN | UNKNOWN |
| Vendor interface | NO | UNKNOWN | UNKNOWN |
| Vendor usage inside HID | YES | UNKNOWN | UNKNOWN |
| Capture / microphone AS | NO | UNKNOWN | UNKNOWN |
| Bus power declaration | bus-powered, 100 mA | UNKNOWN | UNKNOWN |
| PS5 result | **FAIL** | **UNKNOWN** | **PASS** |

G1500 BAR 除已知的 PS5 結果 `PASS` 外，所有 descriptor 欄位維持 `UNKNOWN`，沒有自行填值。

## Reproducibility notes

本輪指定的六個 macOS 命令都已保存。因內建輸出無法提供完整 class-specific descriptor，才使用環境中**原本就已安裝**的 Homebrew `libusb 1.0.30`。未安裝 `usbutils` 或 PyUSB。

工具檢查結果：Homebrew=`/opt/homebrew/bin/brew`；libusb=`1.0.30`；`lsusb`=not found；`python3` command=`/usr/bin/python3`（實際 interpreter 回報 `/Applications/Xcode.app/Contents/Developer/usr/bin/python3`）；PyUSB import=`ModuleNotFoundError`。因既有 libusb 已足夠，本輪沒有新增任何依賴。

唯讀 dumper source：[`m16-usb-dump.c`](../tools/usb-descriptor-dump/m16-usb-dump.c)。它只定位 `2D99:A020`、讀取標準 Device/Configuration/String descriptors，並解析 libusb 已取得的 class-specific bytes；沒有 claim interface、detach kernel driver、送 vendor command 或寫入裝置。
