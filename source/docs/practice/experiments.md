# 实验记录

本章记录本次研究的全部实卡实验，按时间顺序。读卡器：PN532 UART。

## 实验 1：真卡 Konami e-amusement（AIC）dump

**对象**：Konami 发行的 AIC 卡（DFC 0068）

```
IDm: 01 2e 61 19 78 53 a3 5d
PMm: 00 f1 00 00 00 01 43 00
块 00 (SPAD0): f5 e1 c0 bd ea 78 f7 a5 52 9d 1a fb 3a 1c 48 7d
块 01-04:      读拒绝（01 B1）
块 82:         IDm + DFC 00 68
```

SPAD0 用 spad0 算法解密 → `52042115209693652588`（520 号段）。
CardCipher 编码 IDm → `K7GSNPJBC6CMSR22`，与卡背印刷卡号一致。

尝试写 SPAD0：**拒绝（01 A6）**。

## 实验 2：空白 UID 卡 → Aime 卡

**对象**：Gen1a UID 卡，UID `BD 65 53 03`，SAK 08

**意外发现**：这张"空白卡"全扇区已被预置
KeyA = KeyB = `WCCFv2`，访问位 `08 77 8F 11`
（数据块仅 KeyB 可写）。默认 FF 密钥全灭，mfoc 字典也测不出。
用 MifareClassicTool（Android）dump 才看清。

**写入流程**（`aime_write` 工具）：

1. KeyB WCCFv2 认证成功
2. **写回测试**：块 2 原样写回并回读比对 → 可写 ✅
3. 写入：
   - 块 1 = `53 42 53 44` + 零（"SBSD"）
   - 块 2 = `00×6` + BCD `50100000003177534211`
     （= "5010000000" + decimal(UID)，UID 十进制 3177534211）
   - 尾块 = KeyA `FF×6` + `FF 07 80 69` + KeyB `WCCFv2`
4. 重选卡 + KeyB 重新认证，三块回读全部一致
5. 再次写回测试 → 写后仍可擦写 ✅

块 0（UID）全程未动。

## 实验 3：真卡 Sega Aime（AIC）dump

**对象**：Sega 发行的 AIC 卡（蓝白卡面，DFC 0078）

```
IDm: 01 2e 59 39 99 58 94 b0
块 00 (SPAD0): 37 91 60 26 63 a0 2e c8 cd a4 a2 c0 bf c0 09 69
块 01-04:      读拒绝（01 B1）
块 05-0D:      全零
块 0E:         全 FF
块 82:         IDm + DFC 00 78
```

SPAD0 解密 → `50142788364534652126`（501 号段）。
这张 Sega 卡注册过 Konami——AIC 互认的活证据。
CardCipher(IDm) → `KJAL9Z3HLEGALM2P`。

## 实验 4：AIC 卡安全写测试

对实验 3 的卡：

1. 全量备份可读块（23 块）到文件
2. 选空白块 0x0D 做"写图案→回读→还原→回读"可逆测试
3. 结果：**全部写拒绝（01 A6）**——0x0D、0x05、0x0E 均如此
4. SPAD1（0x01）写探测：同样 `01 A6` 拒绝
5. 全卡比对备份：零差异，卡无损

**结论**：正版 AIC 卡用户区整体写锁，写入通道只留给厂商 MAC 密钥。

## 实验 5：服务码探测

对 Konami AIC 卡用 Read 命令探测 0x0001-0x001F + 高位服务码：

- 仅 `0x000B` 返回数据
- 其余返回 `01 A6`（需认证）或 `01 A8`（不存在，仅见 0x0009）
- Request Service（0x02）命令：Lite-S 无响应

**结论**：卡上没有第二个可用的隐藏服务。

## 环境备注

- macOS（Apple Silicon），libnfc 1.8.0（Homebrew）
- PN532 经 CH340 USB 转串口
- 所有工具源码见[工具章](/practice/tools.md)
