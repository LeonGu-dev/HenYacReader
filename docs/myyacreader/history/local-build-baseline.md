> 历史记录：反映当时的检查或阶段状态；当前行为以[文档索引](../README.md)及现行需求为准。

# 原版本地构建记录

完成日期：2026-11-07。

后续更新：当前部署的书库 EXE 已包含启动顺序修复，详见 [书库启动修复记录](../library-startup-fix.md)。以下原版测试结果及原始 SHA-256 均为首次构建记录。

## 版本与配置

- 官方仓库：https://github.com/YACReader/yacreader
- 分支：develop。
- 提交：`118213b9ad951cbe9391381877046935c4076a18`。
- 应用版本：10.5.0。
- 平台：Windows x64，Release。
- 编译器：MSVC 19.44.35229，Visual Studio Build Tools 2022。
- Qt：6.9.3，MSVC 2022 x64。
- CMake：4.4.4；Ninja：1.13.2。
- 解压后端：7zip；PDF 后端：pdfium；上游测试开启。
- 应用源码无修改，Git 工作区干净。

## 产物

可直接运行的目录：`D:\GitHub\comic-reader\artifacts\baseline`，含 Qt DLL、QML、插件、翻译、7zip 和 PDFium。目录约 184 MiB。

- `YACReader.exe`：阅读器。
- `YACReaderLibrary.exe`：书库。
- `YACReaderLibraryServer.exe`：服务器。
- 测试漫画：`D:\GitHub\comic-reader\artifacts\smoke-test\Baseline Test.cbz`，三页，图中依次标有 1、2、3。

不要单独移动 EXE；运行依赖与 EXE 一起保存在上述目录中。

## 验证结果

- 原版所有应用及测试程序编译成功，1008 个构建步骤完成。
- 上游 CTest：48 / 48 通过，用时 89.04 秒。
- 打包后的阅读器及书库均启动成功，检查时两个进程均响应。
- 阅读器日志确认打开了测试漫画压缩包。
- 窗口检查工具授权超时，未完成截图检查、实际翻页及 Library → Reader → 进度回传的人工验收。
- 启动检查期间临时使用空书库配置。检查后两个测试进程已关闭，原有阅读器、书库和公共 INI 文件均已恢复，并通过 SHA-256 校验。
- 正常手动启动此原版构建会使用系统中既有的 YACReader 配置；人工验收宜使用书库副本。

首轮沙箱测试有 22 项失败。单项诊断显示临时目录文件写入失败；切换到项目内临时目录并在正常本机环境重跑后全部通过。最终配置探测同样在正常本机环境完成，避免沙箱中的 MSVC PDB 服务错误。

## 重复构建

在项目根目录用 PowerShell 执行：

```powershell
.\scripts\build-baseline.ps1 -Step All
```

可按阶段执行 `Configure`、`Build`、`Test`、`Deploy`。`Configure` 使用 fresh 配置，并清除旧的 Ninja 依赖记录，下一次构建会重新采集依赖。所有环境变量仅影响当前 PowerShell 进程。

工具位于 `.build-tools`；构建目录为 `upstream\yacreader\build-baseline`。后续在 Codex 内执行配置及测试时使用正常本机进程环境。

日志目录：`artifacts\logs`，包括 `configure.log`、`build.log`、`test.log` 和 `deploy.log`。初次编译中文 MSVC 消息导致日志包含较多头文件路径，脚本已过滤这类终端输出，完整日志仍保留。

产物 SHA-256：

```text
YACReader.exe
DD78647730CF7C0E45A50155448F7F277FB75C27F96C992FDBC7E88B48038E7B

YACReaderLibrary.exe
BF0B9AD7493F3DCC83136702EFAACFB3D910E91B9E8004CC1CB058E115DF8053
```


## 2026-11-08：中文 MSVC 依赖追踪修复

一次编辑器布局改动后，验收包启动即崩溃，Windows 报 `0xc0000374`。检查发现 Ninja 将 `properties_dialog.cpp.obj` 的头文件依赖记成了 0 条，导致调用方未随 `ComicMetadataEditor` 新增成员一起重编译，继续按旧大小分配对象。

本机只安装 MSVC 2052 语言资源；`VSLANG=1033` 无法让它输出英文。编译器 `/showIncludes` 实际输出 UTF-8 的「注意: 包含文件:  」，CMake 检测结果却被按 CP936 误解码为「娉ㄦ剰: 鍖呭惈鏂囦欢:  」，Ninja 因而无法识别头文件路径。

`scripts/repair-msvc-dependencies.py` 在每次 Build 前修正已确认的误解码前缀（编译器配置和 Ninja 规则）。发生修正时删除旧 `.ninja_deps`，强制重新采集依赖。新的 Configure 后也会在下一步 Build 自动处理。修正仅针对这个已确认的中文前缀；正常前缀不改动。

修复后对全部程序与测试重编译。`properties_dialog.cpp.obj` 已记录 596 条依赖，包含 `comic_metadata_editor.h`。构建脚本增加该依赖检查，不完整时直接报错，避免把这种构建当作成功结果交付。不要只复制修复前留下的旧 EXE。


本次修复还完成了以下检查：

- 全部 740 个构建步骤重新执行完成。
- 临时只改变编辑器头文件时间戳，用 Ninja dry-run 确认会触发 `properties_dialog.cpp.obj` 重新编译，随后恢复时间戳。
- 再次构建无工作需要执行，脚本的头文件依赖检查通过。
- 验收目录的标准 EXE 和启动入口所用的 `YACReaderLibrary-ui2.exe` 均已替换；使用现有验收配置启动，主窗口成功打开且进程响应正常。

重新编译后的全部 49 项 CTest 通过（70.33 秒）；启动验收版并等待回归测试结束，主窗口进程仍响应正常。
