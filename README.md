# 日本街机 Passport 卡技术指南

Aime / Banapassport / e-amusement pass / NESiCA 与 Amusement IC 协议的逆向工程文档。

- 在线文档：见本仓库 GitHub Pages（Settings → Pages）
- 文档源码：[`docs/`](docs/)
- 配套工具（PN532 读写 / SPAD0 加解密 / CardCipher）：[`tools/`](tools/)

## 本地预览

```bash
npm install
npm run docs:dev
```

## 构建

```bash
npm run docs:build   # 输出到 docs/.vuepress/dist
```

## 内容可信度标注

| 标记 | 含义 |
|---|---|
| ✅ 已验证 | 实卡 + PN532 实测 |
| 🔍 社区逆向 | 来自公开逆向工程项目 |
| ⚠️ 推测 | 基于证据的推断 |

仅供学习与私服研究使用，请支持正版街机。
