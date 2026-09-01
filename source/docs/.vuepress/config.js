import { defaultTheme } from '@vuepress/theme-default'
import { viteBundler } from '@vuepress/bundler-vite'
import { defineUserConfig } from 'vuepress'

export default defineUserConfig({
  base: '/arcade-card-guide/',
  lang: 'zh-CN',
  title: '日本街机 Passport 卡技术指南',
  description: 'Aime / Banapassport / e-amusement pass / NESiCA 与 Amusement IC 协议的逆向工程文档',
  theme: defaultTheme({
    navbar: [
      { text: '首页', link: '/' },
      { text: '总览', link: '/guide/' },
      { text: '卡片', link: '/cards/aime.md' },
      { text: '协议', link: '/protocol/amusement-ic.md' },
      { text: '实践', link: '/practice/tools.md' },
    ],
    sidebar: {
      '/guide/': [
        {
          text: '总览',
          children: ['/guide/README.md', '/guide/nfc-basics.md', '/guide/history.md'],
        },
      ],
      '/cards/': [
        {
          text: '各家卡片',
          children: [
            '/cards/aime.md',
            '/cards/banapassport.md',
            '/cards/eamusement.md',
            '/cards/nesica.md',
          ],
        },
      ],
      '/protocol/': [
        {
          text: '协议与算法',
          children: [
            '/protocol/amusement-ic.md',
            '/protocol/spad0.md',
            '/protocol/cardcipher.md',
            '/protocol/access-code.md',
          ],
        },
      ],
      '/practice/': [
        {
          text: '动手实践',
          children: [
            '/practice/tools.md',
            '/practice/experiments.md',
            '/practice/compat-card.md',
          ],
        },
      ],
    },
    docsDir: 'docs',
    lastUpdated: true,
  }),
  bundler: viteBundler(),
})
