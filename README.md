# 日本街机 Passport 卡技术指南

Aime / Banapassport / e-amusement pass / NESiCA 与 Amusement IC 协议的逆向工程文档。

**本仓库根目录即静态站点**（已构建完成），可直接用 GitHub Pages 从 `main` 分支根目录发布。

## 目录说明

| 路径 | 内容 |
|---|---|
| 根目录（`index.html`、`assets/` 等） | VuePress 构建好的静态站点，`base = /arcade-card-guide/` |
| `source/docs/` | 文档 Markdown 源码 |
| `source/tools/` | 配套工具（PN532 读写 / SPAD0 加解密 / CardCipher） |
| `source/package.json` | VuePress 工程 |

## 重新构建

```bash
cd source
npm install
npm run docs:build   # 输出到 source/docs/.vuepress/dist，需自行复制回根目录
```

## 可信度标注

✅ 已验证（实卡 + PN532 实测）｜🔍 社区逆向｜⚠️ 推测

仅供学习与私服研究使用，请支持正版街机。
