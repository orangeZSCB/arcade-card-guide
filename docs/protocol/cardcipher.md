# CardCipher：e-amusement 卡号算法

印在 e-amusement 卡背面、游戏里显示的 16 位卡号
（如 `KJAL 9Z3H LEGA LM2P`）与卡内数据之间的换算算法。
由 Tau 逆向，bemaniutils / Bottersnike 文档整理，本文已用两张真卡双向验证 ✅。

## 输入：card ID（16 位 hex）

| 卡类型 | card ID 来源 | 类型字段 |
|---|---|---|
| 旧版 MIFARE 卡 | 卡内存储，通常 `E004` 开头 | 1 |
| 新版 FeliCa/AIC 卡 | **IDm 本身**（如 `012E...`） | 2 |

类型判定：`cardid[:4] == "E004"` → 旧式；`cardid[0] == "0"` → FeliCa 式。

## 编码流程（card ID → 卡号）

1. **反转字节序**：8 字节 card ID 倒过来
2. **3DES 加密**：DES3-ECB，密钥由字符串生成：

   ```
   KEY = "?I'llB2c.YouXXXeMeHaYpy!"
   DES_KEY = 每字节重复两次 → 24 字节（168 bit）
   ```

3. **5 bit 分组**：64 bit 密文拆成 13 组，每组 5 bit（0-31）
4. **扩到 16 组**：
   - `groups[13] = 1`
   - `groups[0] ^= 类型字段`
   - 滚动异或：`groups[i] ^= groups[i-1]`（i = 0..13）
   - `groups[14] = 类型字段`
   - `groups[15] = checksum`：`sum(groups[i] * (i%3+1), i=0..14)` 折叠到 5 bit
     （大于 31 时 `chk = (chk>>5) + (chk&31)` 循环）
5. **字符映射**：32 字符表（故意去掉易混淆字符）：

   ```
   0123456789ABCDEFGHJKLMNPRSTUWXYZ
   ```

   注意没有 `I`、`O`、`Q`、`V`。

## 解码流程（卡号 → card ID）

1. 清洗输入：去空格/横杠，大写化，`I→1`、`O→0`
2. 32 字符表反查回 16 组 5 bit 值
3. 校验末位 checksum，不匹配即非法卡号
4. 逆滚动异或、去掉类型/填充组、取 13 组拼回 64 bit
5. 3DES 解密、反转字节序 → card ID

## 实测验证 ✅

| 卡 | IDm / card ID | 编码结果 | 解码回环 |
|---|---|---|---|
| Konami 真卡 | `012E61197853A35D` | `K7GSNPJBC6CMSR22` | ✓ |
| Sega 发行卡 | `012E5939995894B0` | `KJAL9Z3HLEGALM2P` | ✓ |

::: tip 冷知识
`KJAL 9Z3H LEGA LM2P` 第三组正好是 `LEGA`——Sega 发行的卡，
卡号里藏着 LEGA。这是 3DES 的巧合，不是彩蛋（但很好玩）。
:::

## 参考实现

- Python：bemaniutils `bemani/common/card.py`（本文仓库附 `bemani_card.py`）
- WASM：eamuse-card-wasm（Go 编译）
- 文档：https://eamuse.bsnk.me/cardid.html
