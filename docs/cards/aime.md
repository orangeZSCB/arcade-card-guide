# Sega Aime

Sega（现 SEGA FAVE）发行的玩家卡，蓝色系卡面。用于 maimai、CHUNITHM、
ONGEKI、星与翼的悖论等 ALLNet 系游戏。

## 旧版 Aime（MIFARE Classic 1K）

### 扇区 0 布局 ✅ 已验证

只有**扇区 0** 被使用，其余扇区空白：

| 块 | 内容 | 说明 |
|---|---|---|
| 0 | UID + 厂商数据 | 不动 |
| 1 | `53 42 53 44`（"SBSD"）+ 12 字节 0 | Aime 识别标记 |
| 2 | `00×6` + **BCD access code（10 字节）** | 核心数据 |
| 3（尾块） | KeyA `FF×6` / 访问位 `FF 07 80 69` / KeyB `57 43 43 46 76 32` | KeyB = ASCII "WCCFv2" |

块 2 的 BCD 编码：20 位 access code 每两位压成一字节，大端。
例如 access code `50100000003177534211` →

```
50 10 00 00 00 31 77 53 42 11
```

### 读取流程

1. 用 KeyB `WCCFv2` 认证扇区 0
2. 读块 1，确认 "SBSD" 标记
3. 读块 2，取偏移 6-15 解码 BCD → access code

🔍 社区工具链里，`WCCFv2` 这个 KeyB 是公开的秘密（WCCF = World Club Champion Football，
Aime 系统的首发游戏）。

### access code 规则

- 20 位纯数字
- **不以 `3` 开头**（3 段保留给 Banapassport，见[校验规则](/protocol/access-code.md)）
- 与 UID 无数学关系，由发卡系统分配（详见 [access code 专题](/protocol/access-code.md)）

::: tip 私服生态
在私服场景（segatools / AimeIO 等），读不到合法 Aime 数据的 FeliCa 卡常被
映射为 `decimal(UID)` 补零到 20 位的伪码。这是私服行为，与官方无关。
:::

## 新版 Aime（Amusement IC）

新版 Aime 就是 [Amusement IC 卡](/protocol/amusement-ic.md)，卡面为 Aime 风格，
发行方代码 **DFC = 0078** ✅ 已验证（实测卡：系统块 0x82 偏移 8-9 = `00 78`）。

Aime 的数据存在 AIC 卡的受保护块（block 0x01 一带，需 Sega 的 MAC 认证，
外部不可读）。实际使用中，Sega 系游戏通过读卡器直通读取卡内数据，
或按发行方的服务器流程处理。

## 读卡器

- 官方：Sega Aime 读卡器（USB，串口协议，57600 8N1），命令以 `81`/`83` 等开头
- 私服常见替代：PN532 + segatools、AimeIO 固件、HINATA 等
- 兼容卡：本文 [实践章](/practice/compat-card.md) 记录了用普通 MIFARE Classic
  写一张 Aime 兼容卡的完整流程

## 本章实验记录

本文作者用 PN532 把一张空白 UID 卡写成了合法 Aime 格式（块 1 "SBSD" +
块 2 BCD 码 + 标准尾块），并验证写后仍可擦写。完整过程见
[实验记录](/practice/experiments.md)。
