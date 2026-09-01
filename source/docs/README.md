---
home: true
title: 首页
tagline: Aime / Banapassport / e-amusement pass / NESiCA 与 Amusement IC 协议的逆向工程文档
actions:
  - text: 开始阅读 →
    link: /guide/
    type: primary
  - text: 直达 Amusement IC 协议
    link: /protocol/amusement-ic.md
    type: secondary
features:
  - title: 四家卡片全覆盖
    details: Sega Aime、Bandai Namco Banapassport、Konami e-amusement pass、Taito NESiCA，新旧两代格式全部拆解
  - title: 协议级细节
    details: MIFARE Classic 扇区布局、FeliCa Lite-S 块结构、Amusement IC 服务区、SPAD 加密、卡号生成算法
  - title: 实测驱动
    details: 所有关键结论都来自 PN532 实卡实验，标注"已验证 / 社区逆向 / 推测"三级可信度
footer: 仅供学习与私服研究使用 | 请支持正版街机
---

## 这份文档是什么

这是一份关于**日本街机用卡（街机 Passport 卡）**的技术文档。日本四大街机厂商
（Sega、Bandai Namco、Konami、Taito）各自发行过自己的玩家身份卡，
2021 年前后又联合推出了统一的 **Amusement IC** 卡标准。

本文档基于对真实卡片的 dump、开源固件与客户端的源码分析，整理出各家卡片的：

- 物理介质（MIFARE Classic / FeliCa Lite-S）
- 存储布局（扇区、块、服务区）
- 密钥与访问控制
- access code 的编码、校验与来源
- 卡号（印在卡背面的字符串）生成算法
- Amusement IC 统一协议的全部已公开细节

## 可信度标注

| 标记 | 含义 |
|---|---|
| ✅ 已验证 | 本次研究中用实卡 + PN532 亲自验证过 |
| 🔍 社区逆向 | 来自公开的逆向工程项目（源码可查），未逐条复现 |
| ⚠️ 推测 | 基于证据的推断，仅供参考 |

## 快速导航

- [总览：街机卡是什么](/guide/README.md)
- [Sega Aime](/cards/aime.md)
- [Banapassport](/cards/banapassport.md)
- [e-amusement pass](/cards/eamusement.md)
- [NESiCA](/cards/nesica.md)
- [Amusement IC 协议](/protocol/amusement-ic.md)
- [工具与读写实践](/practice/tools.md)
