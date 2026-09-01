# NESiCA

Taito 发行的玩家卡，随 2010 年的 **NESiCAxLive** 网络上线。用于格斗天尊、
圣灵之心、雷电等 Taito Type X 系游戏。

## 旧版 NESiCA（MIFARE，特殊布局）

旧 NESiCA 是四家里最"野"的格式：识别标记写在**块 0（厂商块）**里。

### 识别标记 ✅（标记位置来自 aic_pico 固件源码）

```
块 0 偏移 10-11 = F8 01  →  判定为 NESiCA
```

块 0 在普通 MIFARE 卡上是只读的，Taito 的做法是向制卡厂定制厂商数据区，
把 `F8 01` 直接做进卡里。读卡器的识别逻辑因此很简单：
**不认证、直接尝试读块 0**，读出来且偏移 10-11 是 `F8 01` 就是 NESiCA。

### 其余布局

旧 NESiCA 的存档完全在服务器侧（NESiCAxLive 账号体系），卡本身
更接近"带识别标记的空白卡" ⚠️。社区对其余扇区的用途记录很少，
也没有像 "WCCFv2" 那样广为人知的密钥。

::: warning
如果你手里有旧 NESiCA 卡，欢迎用 [工具章](/practice/tools.md) 的 MIFARE 流程
dump 补充数据，这是本文档收集度最低的一章。
:::

## 新版 NESiCA（Amusement IC）

新版即 AIC 卡，发行方代码 **DFC = 0079** ✅（aic_pico 固件对照表）。

Taito 的 NESiCA 品牌在 AIC 时代并入统一标准，新卡面仍保留 NESiCA 标识。

## DFC 对照总表

| DFC（卡内 0x82 偏移 8-9） | 发行方 |
|---|---|
| `00 68` | Konami（e-amusement） |
| `00 78` | Sega（Aime） |
| `00 2A` / `00 3A` | Bandai Namco（Banapassport） |
| `00 79` | Taito（NESiCA） |

DFC 是 Amusement IC 卡的"户口"，决定了卡面样式和各家服务器对它的
号段分配，详见 [Amusement IC 协议章](/protocol/amusement-ic.md)。
