# 总览：街机 Passport 卡

## 这些卡是干什么的

日本街机（音游、赛车、卡牌对战等）普遍采用"刷卡登录"模式：
玩家把卡放在读卡器上，机器读取卡内的**身份凭证**，
向厂商服务器查询/同步玩家档案（成绩、解锁内容、段位等）。

四家厂商各自的卡：

| 厂商 | 卡名 | 首发 | 旧介质 | 新介质（AIC） |
|---|---|---|---|---|
| Sega | **Aime**（アイメ） | 2012 | MIFARE Classic 1K | FeliCa Lite-S（DFC 0078） |
| Bandai Namco | **Banapassport**（バナパスポート） | 2012 | MIFARE Classic 1K | FeliCa Lite-S（DFC 002A/003A） |
| Konami | **e-amusement pass** | 2012 | MIFARE Classic 1K | FeliCa Lite-S（DFC 0068） |
| Taito | **NESiCA**（ネシカ） | 2010 | MIFARE Classic（特殊） | FeliCa Lite-S（DFC 0079） |

::: tip 一句话版本
旧时代：四家各用一张 MIFARE Classic 1K，各写各的扇区格式，互不兼容。
新时代：一张 FeliCa Lite-S 的 **Amusement IC 卡**，四家共用，各占各的存储区。
:::

## 卡里的核心概念：access code

无论哪家卡，核心数据都是一个 **20 位十进制数字**，称为 access code（访问码）：

- 它是服务器数据库里"卡 ↔ 账号"的主键
- 印在卡背面（或卡套上），也存进手机的二维码里
- 号段分配互不冲突：`3` 开头是 Banapassport，其余归 Aime 体系；
  Konami 在 AIC 卡上另起 `501/520` 段

::: warning
access code 不等于 UID。UID 是芯片出厂序列号，access code 是发卡系统分配的账号凭证。
两者之间的关系是本文档的核心问题之一，见 [access code 专题](/protocol/access-code.md)。
:::

## 两代格式的对照

```
旧时代（~2012-2021）              新时代（2021- 至今）
┌─────────────────────┐          ┌──────────────────────────┐
│ MIFARE Classic 1K   │          │ FeliCa Lite-S (AIC)      │
│ 16 扇区 × 4 块      │          │ syscode 88B4, svc 000B   │
│ 每家自定义布局       │   ──→    │ SPAD0..4 分区            │
│ 互不兼容            │          │ 一张卡通吃四家            │
└─────────────────────┘          └──────────────────────────┘
```

旧卡在新机器上不能用；新 AIC 卡在旧机器上也不能用（介质都不同）。
日本机厅普遍两种读卡器并存，或者用同时支持两种介质的读头。

## 文档结构

1. [NFC 基础](/guide/nfc-basics.md)：MIFARE Classic 与 FeliCa Lite-S 的技术底子
2. [简史](/guide/history.md)：从磁卡到 Amusement IC
3. 四张卡各一章：[Aime](/cards/aime.md) / [Banapassport](/cards/banapassport.md) /
   [e-amusement pass](/cards/eamusement.md) / [NESiCA](/cards/nesica.md)
4. [Amusement IC 协议](/protocol/amusement-ic.md)：新卡的完整协议
5. [实践](/practice/tools.md)：用什么硬件和软件读写这些卡
