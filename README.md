# HenYacReader

基于 [YACReader](https://github.com/YACReader/yacreader) 的个人漫画书库与阅读器，保留 Library / Reader 独立进程。当前阶段版本为 **0.1.0**，主要完善 Windows 书库整理与检索。

## 下载与运行

前往 [Releases](https://github.com/LeonGu-dev/HenYacReader/releases)，下载 `HenYacReader-0.1.0-windows-x64.zip`，解压到可写目录，运行 `YACReaderLibrary.exe`。单独阅读可运行 `YACReader.exe`。请保留解压后的整个目录，不要单独移动 EXE。

项目名称和窗口已改为 HenYacReader；EXE、配置标识、`.yacreaderlibrary` 目录及已有智能列表格式沿用上游／旧版标识，以兼容现有书库。ZIP 是免安装运行包，应用设置默认仍存于用户配置目录，并非所有设置都随 ZIP 移动。缺少 Microsoft VC++ 运行库时，可运行包内 `vc_redist.x64.exe`。

仓库目前为私有，Release 和源码下载均需要有仓库访问权限。后续对外发行前可另行调整可见性。

## 已实现

- 单页元数据：标题、标题翻译、作者／合同本、社团、作品、角色、tag、即卖会、系列、最爱、评分、色彩、修正、出版类型、翻译及译者。
- 已录入标签连续点选、颜色自定义、列表列配置及与／或组合筛选。
- 普通阅读列表、筛选快照、按全库或指定文件夹动态匹配的智能列表，编辑复用同一筛选组件。
- 文件名规则解析、生成可编辑预览、独立确认重命名并同步书库路径。
- 新版 Info view／网格横幅、可调整侧栏、精简菜单及删除二次确认。
- 数据库备份／恢复、高级修复、保留封面和普通书单归属的元数据清除。

## 文档

[使用说明](docs/henyacreader/user-guide.md) · [数据与兼容](docs/henyacreader/data-storage.md) · [本地构建](docs/henyacreader/building.md) · [已知限制](docs/henyacreader/known-issues.md) · [更新记录](CHANGELOG.md)

## 开发与来源

基于上游 develop `118213b9ad951cbe9391381877046935c4076a18`，保留上游提交历史。`develop` 用于日常开发，`release/v0.1.0` 与 `v0.1.0` 标记阶段版本。过程讨论和早期模板从当前目录清理，仍可从 Git 历史查阅；旧实现的 13 份参考快照在 `legacy/`，不进入程序构建或运行包。

代码采用 [GPL v3](COPYING.txt)，原作者 Luis Ángel San Martín Rodríguez。原项目说明与更新记录保存在 [docs/upstream](docs/upstream/README-YACReader.md)，第三方许可证和作者署名保留。本仓库为独立改造版，问题请反馈到本仓库，而非上游作者。
