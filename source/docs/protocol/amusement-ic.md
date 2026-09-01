# Amusement IC 协议

Amusement IC（AIC）是四家厂商的统一卡标准，2021 年落地。
本章基于对多张真卡的完整 dump（PN532 + libnfc），所有块数据均为实测 ✅。

## 物理层

| 项 | 值 |
|---|---|
| 芯片 | FeliCa Lite-S |
| 系统码（syscode） | `0x88B4` |
| 轮询速率 | 212 / 424 kbps |
| IDm 指纹 | 前 2 字节恒为 `01 2E`（Sony AIC 制卡批次） |
| PMm 指纹 | `00 F1 00 00 00 01 43 00` |

实测卡例：

```
IDm: 01 2e 59 39 99 58 94 b0
PMm: 00 f1 00 00 00 01 43 00
SC : 88 b4
```

## 服务区：0x000B

AIC 的所有数据都在服务码 **0x000B** 下。用 Read Without Encryption（0x06）
探测 0x0001-0x001F 及更大范围的服务码，实测只有 0x000B 返回数据，
其余一律 `01 A6`（需认证）或 `01 A8`（不存在）✅。
Lite-S 也不响应 Request Service（0x02）枚举 ✅。

### 用户区块图（0x00-0x0E）

```
块     用途                        读        写
0x00   SPAD0  Konami e-amusement   明文可读   需 Konami MAC（实测 a6 拒写）
0x01   SPAD1  （Sega 系）          需 MAC     需 MAC（实测 b1/a6）
0x02   SPAD2  （Bana 系）          需 MAC     需 MAC
0x03   SPAD3  （Capcom 系⚠️）      需 MAC     需 MAC
0x04   SPAD4  （预留 ⚠️）          需 MAC     需 MAC
0x05   ┐
...    │ 自由区，全零              明文可读   实测被写锁（a6）
0x0D   ┘ 9 块 = 144 字节
0x0E   全 FF，用途未知              明文可读   实测被写锁（a6）
```

- SPAD = 各厂商的专属数据块。**只有 SPAD0 对外明文可读**，
  这是设计使然：任何 AIC 读卡器都能判断"这张卡有没有注册过 Konami"
- SPAD1-4 连读都要 MAC 认证（密钥在官方读卡器 SAM / 服务器侧）
- 自由区在正版卡上被整体写锁，自制卡上则完全可用

### 系统区块图（0x80-0x87）

```
块     内容（实测）
0x80   全零（MC/锁配置区，勿动）
0x81   全零
0x82   IDm(8) + DFC(2) + 补零        ← 识别核心
0x83   IDm(8) + PMm(8)               ← 出厂镜像
0x84   全零
0x85   系统码 88 B4 + 补零
0x86   00 01 + 补零
0x87   全零
```

**块 0x82 是卡片身份证**：读卡器读它拿到 IDm 和 DFC。
aic_pico 读卡器固件的识别路径就是 `读 0x000B 块 0x82 → 按 DFC 报卡名`。

## DFC：发行方代码

块 0x82 偏移 8-9 的 2 字节（实测均为 `00 xx` 形式）：

| DFC 低字节 | 发行方 |
|---|---|
| `0x68` | Konami |
| `0x78` | Sega |
| `0x2A` / `0x3A` | Bandai Namco |
| `0x79` | Taito（NESiCA） |

## 写保护机制 ✅

两张不同发行方的真卡实测结论一致：

1. 用户区**所有块**的 Write Without Encryption 均返回 `01 A6`
2. SPAD1-4 的读返回 `01 B1`，写返回 `01 A6`
3. 全卡没有任何可用明文写入的块

::: danger
正版 AIC 卡是"只读资产"。想写数据只能走带 MAC 认证的写命令，
其密钥属于各厂商非公开体系。这也是 AIC 卡的防伪底线。
:::

## 识别流程（读卡器视角）

把 aic_pico 固件的逻辑抽象出来，一台 AIC 读卡器的判定树是：

```
轮询到卡
├─ FeliCa（syscode 88B4）
│   ├─ 读 0x000B/0x82 → 按 DFC 报发行方
│   └─ 读 0x000B/0x00 → SPAD0 解密 → Konami access code
├─ MIFARE
│   ├─ 免认证读块 0，偏移 10-11 = F8 01 → NESiCA（旧）
│   ├─ KeyA 60 90 D0 06 32 F5 认证块 3 → 读块 1
│   │   └─ 偏移 2-6 = "NBGIC" → Banapassport
│   └─ KeyB WCCFv2 认证块 3 → 读块 1
│       └─ 开头 "SBSD" → Aime
└─ ISO 15693 → 其他
```

## 一张卡通吃四家的原理

AIC 卡不存储"这是哪家的卡"这种状态——它是一张**带多个独立插槽**的卡：

- 第一次在 Konami 游戏上刷 → Konami 服务器写 SPAD0（501/520 号码段）
- 第一次在 Sega 游戏上刷 → Sega 侧建立记录（走 SPAD1 MAC 体系）
- 各家的数据互不可见、互不干扰

所以一张 Sega 发行的 Aime 卡里装着 Konami 的 `501...` access code
是完全正常的（本文实测卡就是如此）。
