# e-amusement pass

Konami 发行的玩家卡，用于 BEMANI 全系（beatmania IIDX、SOUND VOLTEX、
pop'n music、jubeat、DanceDanceRevolution 等）。

## 旧版 e-amusement pass（MIFARE Classic 1K）

2012-2019 年代发行的白色小卡。卡内存储一个 **16 位十六进制的 card ID**
（通常以 `E004` 开头），游戏读卡后拿 card ID 向 e-amusement 服务器换账号。

- 介质：MIFARE Classic 1K ✅（CardCipher 类型字段 = 1 对应 "old-style"）
- card ID：16 个 hex 字符，`E004` 前缀是 e-amusement 旧卡的标志 ✅
- 卡号字符串（印在卡背面）= card ID 的 [CardCipher 编码](/protocol/cardcipher.md)

## 新版 e-amusement pass（Amusement IC）

新版即 AIC 卡，发行方代码 **DFC = 0068** ✅ 已验证（实测真卡）。

### SPAD0：Konami 数据区

AIC 卡服务区（0x000B）的 **块 0（SPAD0）** 是 Konami 专用，
存加密后的 access code。实测卡数据 ✅：

```
SPAD0 密文：37 91 60 26 63 a0 2e c8 cd a4 a2 c0 bf c0 09 69
解密明文：  00 00 00 00 00 00 50 14 27 88 36 45 34 65 21 26
                          └──── BCD "50142788364534652126" ────┘
```

- 明文布局与 Aime 的块 2 完全一致：6 字节零 + 10 字节 BCD
- 加密算法为 Konami 定制的分组变换，已被 HINATA 完整逆向，
  见 [SPAD0 算法章](/protocol/spad0.md)
- SPAD0 **明文可读**（无需认证），只有写需要 Konami 的 MAC 密钥

### 卡号字符串 = CardCipher(IDm)

新版卡的 16 位卡号不再依赖存储的 card ID，而是直接由 **IDm** 计算 ✅ 已验证：

| 实测卡 | IDm | 卡号 |
|---|---|---|
| Konami 发行 | `012E61197853A35D` | `K7GS NPJB C6CM SR22` |
| Sega 发行（注册过 Konami） | `012E5939995894B0` | `KJAL 9Z3H LEGA LM2P` |

用 `CardCipher.decode()` 反解卡号，输出正好是 IDm，双向验证通过。
算法细节见 [CardCipher 章](/protocol/cardcipher.md)。

### access code：服务器分配制

实测两张卡的 Konami access code：

| 卡 | 发行方 | access code | 号段 |
|---|---|---|---|
| Konami 卡 | DFC 0068 | `52042115209693652588` | **520** |
| Sega 卡 | DFC 0078 | `50142788364534652126` | **501** |

- 号段跟**发行方**走，不跟 IDm 走（IDm 里没有发行方信息）
- access code 是 66 bit 的 20 位十进制数，装不进 8 字节，不可能是 UID 的十进制变形
- 结论：access code 由 e-amusement 服务器在注册时分配并写入 SPAD0，
  完整论证见 [access code 专题](/protocol/access-code.md)

## 读卡器

- 官方：Konami 读卡器（随 e-amusement 套件，USB）
- 私服替代：PN532 直通（游戏经读卡器协议读 SPAD0）、HINATA
