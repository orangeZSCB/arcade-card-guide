# 工具与硬件

## 读卡器：PN532

研究街机卡性价比最高的硬件是 NXP **PN532** 模块（UART 接法最稳）：

- 支持 ISO 14443A（MIFARE）与 FeliCa 212/424 kbps
- **内置 crypto1 引擎**：发原始认证帧即可完成 MIFARE 认证，
  不需要软件实现被破解的密码
- macOS/Linux 下走 libnfc；串口设备形如 `/dev/cu.wchusbserialXXXX`

libnfc 配置（`/opt/homebrew/etc/nfc/libnfc.conf`）：

```
allow_intrusive_scan=yes
device.name = "PN532_UART"
device.connstring = "pn532_uart:/dev/cu.wchusbserial1130"
device.speed = 106
```

## 软件

| 工具 | 用途 | 备注 |
|---|---|---|
| libnfc（nfc-list 等） | 基础读写 | 见下方 brew 坑 |
| mfoc | MIFARE 默认密钥探测 | 字典里没有街机专用密钥，需 `-k` 追加 |
| nfc-mfclassic | MIFARE dump/写入 | 1.8.0 写模式有 bug，见坑清单 |
| MifareClassicTool（Android） | 手机 dump/写 MIFARE | 输出 JSON 格式，非常好用 |
| 自制 C 工具 | 本文实验全部基于它 | 见下方 |

### 自制工具清单（本文档配套）

| 工具 | 功能 |
|---|---|
| `felica_dump.c` | FeliCa AIC 服务区全块 dump |
| `felica_backup_test.c` | AIC 卡备份 + 安全可逆写测试 |
| `aime_write.c` | 把 MIFARE Classic 写成 Aime 格式 |
| `spad0_crypto.py` | SPAD0 加解密（命令行） |
| `bemani_card.py` | CardCipher 卡号编解码 |

编译方式（macOS）：

```bash
cc -O2 -o tool tool.c <libnfc源码>/utils/mifare.c \
   -I<libnfc源码>/utils -I/opt/homebrew/include \
   -L/opt/homebrew/lib -lnfc
```

## 坑清单（血泪实测）

### brew 的 libnfc 没有 MIFARE API

Homebrew 的 libnfc 1.8.0 bottle：

- dylib 里**没有** `nfc_initiator_mifare_cmd` 符号
- 头文件里没有 `mifare.h`
- 但 `nfc-mfclassic` 等工具照常工作——它们用发布 tarball 里的
  `utils/mifare.c`（用户态实现，内部走 `nfc_initiator_transceive_bytes`，
  依赖 PN532 芯片内置 crypto1）

**解法**：从官方发布 tarball 取 `utils/mifare.c/h` 一起编译即可。

### nfc-mfclassic 1.8.0 写循环 bug

写模式主循环把写操作嵌在 `if (is_first_block(uiBlock))` 分支里，
导致**只写每扇区第一块**（4, 8, ... 60），尾块和其余数据块根本不写。
用它写 Aime 数据会静默失败。自制工具是必要的。

### Clash 代理与 DNS

大陆网络下部分命令（dig 等）会被代理返回假 IP（198.18.x.x），
但裸 TCP/串口通信不受影响。下载 GitHub 资源建议挂代理。

## 卡片采购提示

| 卡 | 能做什么 | 注意 |
|---|---|---|
| 空白 MIFARE Classic 1K | 写 Aime / Banapassport 兼容卡 | 普通卡即可 |
| "UID 卡"（Gen1a 魔术卡） | 同上 + 可改块 0 | 部分预置卡自带密钥（实测见过全扇区 WCCFv2） |
| 空白 FeliCa Lite-S | 自制 AIC 卡的前提 | 必须确认用户区未锁且支持 88B4 系统 |
| 正版 AIC 卡 | 只能读，不能写 | 全区块写锁（01 A6） |
