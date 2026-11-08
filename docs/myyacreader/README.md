# MyYACReader 项目文档

当前版本已于 2026-11-08 由用户验收通过。基于官方 YACReader develop `118213b9ad951cbe9391381877046935c4076a18`，保留上游历史、独立 Library / Reader 架构及 GPL v3 许可证。

## 当前说明

| 文档 | 用途 |
| --- | --- |
| [项目方向](product-direction.md) | 已实现范围、后续优先级与验收节奏 |
| [漫画信息字段与规则](漫画信息编辑模板.md) | 当前 11 项信息、合同本、原创联动和保存规则 |
| [文件名解析与重命名](文件名解析与重命名第一轮.md) | 操作流程、格式、识别边界及保存机制 |
| [数据存储与文件识别](漫画识别与存储说明.md) | 文件记录、共享信息和书库数据目录 |
| [Windows 本地构建](local-build.md) | 工具布局、构建命令、依赖追踪修复及验证 |
| [后续轻量化范围](lightweight-scope.md) | 尚待逐项决定的裁剪候选 |
| [已验收变更记录](CHANGELOG.md) | 本次提交包含的功能与修复 |

## 历史记录

[初期方向](history/product-direction.md)、[裁剪讨论稿](history/lightweight-scope.md)、[原版构建及增量构建问题](history/local-build-baseline.md)、[第一轮元数据实现](history/漫画信息第一轮改造.md)、[书库初始化弹窗修复](library-startup-fix.md)。历史中的“待实现”“未构建”等描述只对应当时阶段。

源代码路径以仓库根目录为准。本机工作区保留 `docs`、`scripts` 的兼容目录链接；它们分别指向本仓库的 `docs/myyacreader` 与 `scripts/local`。
