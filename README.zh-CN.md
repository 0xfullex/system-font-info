# system-font-info

[English](README.md) | 简体中文

[![npm version](https://img.shields.io/npm/v/system-font-info.svg)](https://www.npmjs.com/package/system-font-info)
[![Node.js](https://img.shields.io/node/v/system-font-info.svg)](https://nodejs.org)
[![CI](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/0xfullex/system-font-info/actions/workflows/ci.yml)
[![License](https://img.shields.io/npm/l/system-font-info.svg)](LICENSE)

[![macOS](https://img.shields.io/badge/macOS-arm64%20%7C%20x64-555555.svg)](#支持的平台)
[![Windows](https://img.shields.io/badge/Windows-x64%20%7C%20arm64-0078D4.svg)](#支持的平台)
[![Linux](https://img.shields.io/badge/Linux%20glibc-x64%20%7C%20arm64-E95420.svg)](#支持的平台)
[![Electron](https://img.shields.io/badge/Electron-main%20process-47848F.svg)](#electron)

**让字体选择器掌握和操作系统一样多的字体信息。**

system-font-info 通过各系统自己的字体服务（macOS 的 CoreText、Windows 的 DirectWrite、Linux 的 fontconfig）列出已安装的字体，返回字体服务对每个字面（face）掌握的信息，并且在所有平台上结构一致。Node.js 上的大多数字体列表库，每个字体只返回一个名称和粗略的字重、样式，只够做一个单语言的普通菜单。system-font-info 则为 Electron 应用或其他桌面工具提供一个真正的字体菜单所需要的信息：

| | 常见的字体列表库 | system-font-info |
| --- | --- | --- |
| 家族名和样式名 | 一个名称，通常是英文 | 字体自带的每一种语言：`苹方-简`、`蘋方-簡`、`PingFang SC`；`常规体`、`標準體`、`Regular` |
| 字重、字宽、样式 | 粗略的标签 | 各平台统一的 CSS 数值（`font-weight` 1-1000，`font-width` 50-200%），另附各平台的原始值 |
| 语言 | 不提供 | 每个字面支持的语言，以及你指定语言的字符覆盖情况 |
| 可变字体 | 不提供 | 轴及其范围和本地化名称；命名实例作为独立的字面列出 |
| 文件与字体表 | 最多一个路径 | 文件路径和字体集合中的索引，字体自身的 OpenType 表 |
| 开销与类型 | 每次调用都取全部信息 | 只取你请求的字段，返回值的类型由请求自动推导 |
| 安装 | 常常要在安装时编译，或调用 `fc-list`、PowerShell 等外部命令 | 为六个目标平台预编译，不编译，也不启动任何子进程 |

### 效果示例

一个中文字体菜单，只列出能排简体中文的字体家族：

```js
import { getFamilies, getFamily, pickName } from 'system-font-info'

const families = await getFamilies({ fields: ['familyNames', 'supportedLanguages'] })
const menu = families
  .filter((family) => family.supportedLanguages.includes('zh-Hans'))
  .map((family) => ({ family: family.family, label: pickName(family.familyNames, ['zh-CN', 'en']) }))
// [..., { family: 'Hiragino Sans GB', label: '冬青黑体简体中文' }, ..., { family: 'PingFang SC', label: '苹方-简' }, ...]
```

可变字体的字重滑块，以命名实例作为刻度：

```js
const syriac = await getFamily('Noto Sans Syriac', { fields: ['faces.axes', 'faces.styleName', 'faces.weight'] })
// syriac.faces[0].axes:  [{ tag: 'wght', min: 100, max: 900, default: 400, names: [...] }]
// syriac.faces:          Regular 400, Thin 100, ExtraLight 200, Light 300, Medium 500, ... Black 900
```

（以上输出来自 macOS 27；每个平台列出的是它自己的字体服务中的字体。）

## 支持的平台

需要 Node.js 22 或更新版本，可在普通 Node.js 和 Electron 的主进程中使用（见 [Electron](#electron)）。包内为以下目标平台各附带一个预编译二进制，其他平台没有：

| 目标平台 | 最低版本 |
| --- | --- |
| macOS，arm64 和 x64 | macOS 11 |
| Windows，x64 和 arm64 | Windows 10 1809 版（build 17763） |
| Linux glibc，x64 和 arm64 | glibc 2.28（Debian 10、Ubuntu 18.10、RHEL 8 或更新），并安装 fontconfig 2.13 和 FreeType 2.9 或更新版本 |

在 Windows 上，后端使用的 DirectWrite 接口在 1809 之前的版本中不存在，也没有退路，会以错误的形式报告这一要求。在 Linux 上，本模块动态链接系统的 `libfontconfig.so.1` 和 `libfreetype.so.6`，所以它们必须已经安装（Debian 和 Ubuntu 上是 `apt install libfontconfig1`，Fedora 和 RHEL 上是 `dnf install fontconfig`；精简的容器镜像通常没有它们），缺少时加载会失败，错误信息会说明原因。不支持 musl（Alpine），加载时会明确报告。其他平台可以从源码构建，见 [CONTRIBUTING.md](CONTRIBUTING.md#building-from-source)（英文）。各平台列出哪些字体、彼此有什么差异，见 [docs/platforms.md](docs/platforms.md)（英文）。

## 安装

```sh
npm install system-font-info
```

## 用法

```js
const { getFamilies, getFamily, pickName } = require('system-font-info')

async function main() {
  // 每种语言的家族名。（开销最小的调用是 `fields: []`：只返回 `family` 名称。）
  const families = await getFamilies({ preset: 'names' })
  for (const family of families) {
    console.log(family.family, pickName(family.familyNames, ['zh-CN', 'en']))
  }

  // 某个家族的字面上指定的字段。
  const helvetica = await getFamily('Helvetica Neue', { fields: ['faces.postscriptName', 'faces.weight', 'faces.style'] })
  console.log(helvetica?.faces)
}

main()
```

`fields` 和 `preset` 必须指定其一（[原因](docs/api.md#options)）；`preset: 'full'` 返回除字符覆盖以外的全部信息。

## 文档

以下文档为英文：

- [docs/api.md](docs/api.md)：函数、选项、预设、每个字段，以及结果遵循的规则。
- [docs/platforms.md](docs/platforms.md)：每个字段在各平台上的来源、列出哪些字体、字体变化多快能被看到，以及实测的开销。

## Electron

只在主进程中使用本包，渲染进程通过 IPC 调用。原生模块不能从 asar 归档中加载，所以要把它解包出来：

```json
{
  "build": {
    "asarUnpack": ["**/node_modules/system-font-info/**"]
  }
}
```

（`asarUnpack` 是 electron-builder 的选项，其他打包工具请使用对应的设置。electron-builder 会自动解包含有 `.node` 文件的模块；使用 Electron Forge 时，用 `@electron-forge/plugin-auto-unpack-natives`。）

本模块是 Node-API（版本 8）模块，安装时不编译，包内也没有 `binding.gyp`，所以 `@electron/rebuild` 没有可重新构建的内容，不需要它。包内带有所有目标平台的二进制；只为一个平台打包的应用可以把其他平台的去掉，例如使用 electron-builder 的 `files` 规则 `!**/node_modules/system-font-info/prebuilds/{win32,linux}-*`。在 Linux 上，本模块绑定系统的 fontconfig，也就是 Electron 内 GTK 和 Pango 使用的那一份，所以用户机器上必须装有这个库。每个版本发布前，都会在每个目标平台上用 Electron 44 的主进程加载并运行一遍（[`scripts/ci/electron-smoke.mjs`](scripts/ci/README.md)）；在 Linux 上还会检查进程只映射了一份 `libfontconfig`，即系统自带的那一份。

## 参与开发

从源码构建（也是在没有预编译二进制的平台上使用本包的方法）、开发、持续集成和发布流程，见 [CONTRIBUTING.md](CONTRIBUTING.md)（英文）。

## 许可证

[MIT](LICENSE)
