# Banapassport

Bandai Namco（万代南梦宫）发行的玩家卡，绿色系卡面，昵称"巴拿马护照"。
用于太鼓达人、湾岸午夜极速（WMMT）、铁拳、机动战士高达 极限对战等。

## 旧版 Banapassport（MIFARE Classic 1K）

### 扇区 0 布局

| 块 | 内容 | 可信度 |
|---|---|---|
| 0 | UID + 厂商数据 | ✅ |
| 1 | 16 字节卡头：偏移 2-6 = `4E 42 47 49 43`（"NBGIC"），其余含序列号等 | ✅ 标记已验证，完整语义 🔍 |
| 2 | `00×6` + **BCD access code（10 字节）** | 🔍 与 Aime 同布局 |
| 3（尾块） | KeyA `60 90 D0 06 32 F5`，其余位与标准配置一致 | ✅ KeyA 来自 aic_pico 固件源码 |

"NBGIC" = **N**andai **B**andai **G**ames **I**C 的缩写，是识别 Banapassport 卡的标记。
aic_pico 读卡器固件的判断逻辑：块 1 偏移 2 起 5 字节 == "NBGIC" → 判定为 BANA 卡。

### 双重结构：块 2 的码 + 块 1 的校验

HINATA 读卡逻辑揭示了一个有趣的细节 ✅：

1. 先用 Aime 的 KeyB `WCCFv2` 认证（两家卡扇区 0 的访问结构兼容）
2. 读块 2 偏移 6-15 解 BCD
3. 如果解出来的码**以 `3` 开头** → 这不是 Aime 卡，是 Banapassport
4. 再读块 1，用 `nbgiGetAccessCode(block1)` 从卡头数据重新推导 access code 并校验

也就是说，块 1 的 "NBGIC" 卡头里含有序列号，可以**推导出** access code，
块 2 里存的只是同一码的 BCD 副本。推导算法在 HINATA 的公开构建中被刻意移除
（`plugins/cardcipher/lib/bana.dart` 是空实现），属于未公开部分 ⚠️。

### access code 规则

- 20 位纯数字，**必须以 `3` 开头** ✅（HINATA 校验器 + OpenBanapass 示例码
  `30764352518498791337`）
- 3 段号码池是 Banapassport 专用，Aime 系刻意避开

## 新版 Banapassport（Amusement IC）

新版即 AIC 卡，发行方代码 **DFC = 002A 或 003A** ✅（aic_pico 固件对照表）。
两个值的区别 ⚠️ 推测与制卡批次/渠道有关。

Bana 的数据存在 AIC 卡的受保护块（对应 SPAD2 一带，需 Bana 的 MAC 认证）。

## 游戏侧数据格式

🔍 从 OpenBanapass（WMMT6 的 DLL 替身）可见，读卡器上报给游戏的卡数据结构里，
access code 以 ASCII 字符串形式放在偏移 `0x50` 处。游戏拿到码后走
Bandai Namco ID 服务器（BNID）查档案。

## 读卡器

- 官方：Banapassport 读卡器（USB，串口协议，与 Aime 读卡器命令格式相近但不同）
- 私服替代：PN532 方案、OpenBanapass（纯软件，不读真卡，直接喂码）
