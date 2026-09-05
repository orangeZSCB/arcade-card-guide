# 工具与硬件

## 读卡器：PN532

研究街机卡性价比最高的硬件是 NXP **PN532** 模块（UART 接法最稳）：

- 支持 ISO 14443A（MIFARE）与 FeliCa 212/424 kbps
- **内置 crypto1 引擎**：发原始认证帧即可完成 MIFARE 认证，
  不需要软件实现被破解的密码
- macOS/Linux 下走 libnfc；串口设备形如 `/dev/cu.wchusbserialXXXX`

libnfc 配置（Homebrew 实际路径是
`/opt/homebrew/Cellar/libnfc/1.8.0/etc/nfc/libnfc.conf`）：

```
allow_intrusive_scan=yes
device.name = "PN532_UART"
device.connstring = "pn532_uart:/dev/cu.wchusbserial110:115200"
device.autopoll = yes
```

- 波特率写在 connstring 的第三段，`device.speed` 不是 libnfc 的配置项
- libnfc 只加载目录下 `.conf` 结尾的文件，备份成 `.conf.bak` 不会被误读

::: warning CH340 的节点号每次插拔都会变
实测同一台机器上出现过 `wchusbserial1130` → `110` → `120` → `130` → `110`。
每次重新插拔都要把 `libnfc.conf` 里的节点改回来，否则报
`Unable to open NFC device`。配套脚本 `pn532_fixport.sh` 会自动找
`/dev/cu.wchusbserial*` 并重写配置。
:::

另外两个常见错误：

| 报错 | 原因 |
|---|---|
| `Serial port already claimed` | 有别的进程占着串口，`lsof /dev/cu.wchusbserial*` 查 |
| `pn53x_check_communication error` | 串口能开但 PN532 不回握手：被强杀的进程把芯片留在轮询态、TX/RX 没交叉、没共地、拨码不在 HSU 档、供电不足 |

被强杀（比如 Ctrl-C 掉一个正在 `nfc_initiator_poll_target` 的程序）之后，
PN532 会一直不回话，**拔电重插**是最快的恢复方式。

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
| `aic_pn532.c` | AIC 卡完整工具：dump / read / write / probe / findsvc，**服务码可选** |
| `spad0_crypto.py` | SPAD0 加解密（命令行） |
| `bemani_card.py` | CardCipher 卡号编解码 |
| `aime_proto.py` | 官方 Aime 读卡器（837-15396）串口协议客户端 |
| `aic_write.py` | 通过官方读卡器写 AIC（走 0x71 FeliCa 直通） |
| `pn532_fixport.sh` | CH340 节点号变了以后自动重写 libnfc.conf |

`aic_pn532.c` 用法：

```bash
cc -O2 -o aic_pn532 aic_pn532.c $(pkg-config --cflags --libs libnfc)
./aic_pn532 dump   backup.txt          # 全卡备份，用户区 + 系统区 0x80-0x91
./aic_pn532 findsvc 0d                 # 探测哪个服务码能写（自动还原）
./aic_pn532 probe  0d 0009             # 可逆写探针：写图案→回读→还原→回读
./aic_pn532 write  00 <32位hex> 0009   # 写块，写完自动回读校验
./aic_pn532 read   00 000b             # 读块
```

块号和服务码都是十六进制，服务码缺省 `000B`。**写卡必须显式指定 `0009`**，
`000B` 是只读服务，用它写一定被拒。

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

## 读卡器：官方 Aime 读卡器（837-15396）

除 PN532 之外，SEGA 官方框体读卡器也能直接接到电脑上用，它走 USB CDC
虚拟串口（STM32F072 一类主控，VID `0x0483` / PID `0x5740`，
设备名形如 `GENERIC_F072C8TX CDC in FS Mode`），说的是 JVS 风格的串口协议。

自报身份：FW `0x94`、HW `837-15396`、LED 板 `000-00000 FF 11 40`。

### 帧格式 ✅

```
请求  E0 <frame_len> <addr> <seq> <cmd> <payload_len> <payload...> <sum>
      frame_len = 5 + payload_len
响应  E0 <frame_len> <addr> <seq> <cmd> <status> <payload_len> <payload...> <sum>
      frame_len = 6 + payload_len
转义  D0 <b-1> 表示字节 b ∈ {E0, D0}
校验  sum = (frame_len + 所有数据字节) & 0xFF，按转义前的值算
```

`frame_len` 统计的是它自己之后的字节数（含校验字节），所以请求是
`addr + seq + cmd + payload_len + payload + sum` = `5 + payload_len`。

### 命令表

| cmd | 名称 | 说明 |
|---|---|---|
| `0x30` | GET_FW_VERSION | 返回固件版本字符串 |
| `0x32` | GET_HW_VERSION | 返回硬件型号 |
| `0x40` | RADIO_ON | **带 1 字节载荷 `03`** |
| `0x41` | RADIO_OFF | |
| `0x42` | POLL | 返回 count + 类型 + IDm/PMm 或 MIFARE UID |
| `0x50` / `0x54` | MIFARE KeyB / KeyA | Banapassport `6090D00632F5` / Aime `WCCFv2` |
| `0x52` | MIFARE_READ | |
| `0x62` | RESET | 启动第一步 |
| `0x71` | FELICA_THROUGH | FeliCa 直通，见下 |
| `0xF0` / `0xF5` | LED 板 info / reset | addr 用 `08` |

状态码：`00` OK、`01` CARD_ERROR、`02` NOT_ACCEPT、`03` INVALID_COMMAND、
`04` INVALID_DATA、`05` SUM_ERROR、`06` INTERNAL_ERROR。

启动序列（segatools `doc/nfc.txt`）：
`62` → `30` → `32` → addr08 `F5` → addr08 `F0` → `54` KeyA → `50` KeyB →
`40`（载荷 `03`）→ `42`。

### 0x71 FeliCa 直通

载荷结构：

```
<IDm 8B> <encap_len> <FeliCa 帧...>
encap_len   = FeliCa 帧的 LEN 字节，等于 1 + 后续字节数
payload_len = 8 + encap_len
```

**关键认识**：所谓 encap 就是**原始 FeliCa 帧本身**。
`encap_len` 是帧的 LEN，紧跟的字节是 FeliCa 命令码，响应里命令码 +1。
所以官方读卡器的 0x71 只是把帧原样转发给 NFC 前端，主机侧完全可以自己构造。

单块读（服务码 `0x000B`，块 `0x00`）：

```
E0 1D 00 <seq> 71 18 <IDm×8> 10 06 <IDm×8> 01 0B 00 01 80 00 <sum>
                              └LEN └CMD      └1服务 └svc └1块 └块元素
```

块列表元素用 2 字节短格式 `80 <blk>`（最高位 1 表示 2 字节形式）。

响应：`E0 23 00 <seq> 71 00 1D 1D 07 <IDm×8> 00 00 01 <16B 数据> <sum>`，
其中 `1D` 是 payload_len 和帧 LEN，`07` = `06`+1，`00 00` 是 FeliCa 状态标志。

单块写（服务码 `0x0009`，块 `0x00`）：

```
E0 2D 00 <seq> 71 28 <IDm×8> 20 08 <IDm×8> 01 09 00 01 80 00 <16B 数据> <sum>
```

响应：`E0 12 00 <seq> 71 00 0C 0C 09 <IDm×8> 00 00 <sum>`。

### 官方读卡器的坑 ⚠️

- **`0x42` POLL 只在放卡那一刻上报一次**。卡一直躺在感应区上不会再报，
  必须拿起来重新贴，或者 `0x41` + `0x40` 重新武装射频
- 因此"等卡 → 激活 → 读写"必须在同一个进程里一次跑完
- `0x71` 依赖 NFC 前端处于已激活状态。上游 Arduino 实现里
  `inDataExchange` 用的目标号由 `felica_Polling` 设置，所以
  0x71 之前至少要成功执行过一次 FeliCa 轮询
- 未实现的命令回 `03 INVALID_COMMAND`，实现了但射频失败回 `01 CARD_ERROR`，
  可以用这个区别判断固件到底有没有实现直通
