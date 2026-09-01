# 简史：从磁卡到 Amusement IC

## 时间线

| 年代 | 事件 |
|---|---|
| 1990s-2000s | 街机存档靠磁卡（如早期高达联扎）或记忆卡，易磨损、易复制 |
| 2005-2010 | 部分厂商试验 IC 卡；Taito 2010 年随 NESiCAxLive 推出 **NESiCA** |
| 2012 | Sega **Aime**、Namco **Banapassport**、Konami **e-amusement pass** 相继上线，三家不约而同选了 MIFARE Classic 1K |
| 2012-2020 | 四卡并立时代：玩家去不同机厅要带不同的卡；跨厂商移籍要"转卡" |
| 2021 | 四家联合推出 **Amusement IC**：一张 FeliCa Lite-S 卡通吃全部 |
| 2021- | 旧卡逐步退役；新卡按发行方印不同卡面，但协议统一 |

## 为什么都选了 MIFARE Classic

2012 年前后，MIFARE Classic 1K 是全球铺货最广、读卡器最便宜的 IC 卡。
尽管其 crypto1 加密当时已被学术圈破解，对街机场景（卡便宜、数据在服务器侧）
仍然够用——**真正的安全边界从来在服务器，不在卡**。

这一判断在 Amusement IC 时代也没变：新卡用 FeliCa Lite-S 更多是出于
供应链（Sony 在日系读卡器生态里的地位）与统一标准的考虑。

## Amusement IC 的意义

- 玩家：一张卡四家通用，钱包和卡包都减负
- 机厅：一台读卡器服务所有机器
- 厂商：共享发卡/制卡渠道，降低运营成本
- 逆向圈：🙃 一个协议打四家，文档工作量除以四（本文档因此得以存在）

技术上，Amusement IC 卡 = FeliCa Lite-S + 约定的存储布局（见
[协议章](/protocol/amusement-ic.md)）+ 各家的加密扩展（见
[SPAD0 算法](/protocol/spad0.md)）。
