# HenYacReader 0.1.0 — Windows x64

完整解压后运行 `YACReaderLibrary.exe` 打开书库，或运行 `YACReader.exe` 单独阅读。请保留同目录的 DLL、插件及其他文件，不要单独移动 EXE。

缺少 Microsoft Visual C++ 运行库时，可运行包内的 `vc_redist.x64.exe`。

EXE 名称、应用配置标识和 `.yacreaderlibrary` 路径保留旧标识用于兼容，窗口名称使用 HenYacReader。本包免安装，设置默认仍在用户配置目录；不是所有设置都跟随解压目录移动。

升级前备份书库数据和应用设置。普通书库数据在漫画根目录 `.yacreaderlibrary`；漫画压缩包需另外备份。不要同时打开多个程序修改同一书库。

本阶段包括新版元数据、标签筛选、普通／智能阅读列表、文件名解析与重命名、资料视图及删除二次确认。旧已读／未读和阅读方式的写入失败处理仍为后续维护项；其他限制与源码说明见项目仓库。

项目与更新：https://github.com/LeonGu-dev/HenYacReader

本仓库当前私有，下载需要权限。基于 YACReader 改造，原作者 Luis Ángel San Martín Rodríguez；GPL v3 见 `COPYING.txt`，第三方说明见 `licenses/`。仅从本项目 Releases 手动更新。
