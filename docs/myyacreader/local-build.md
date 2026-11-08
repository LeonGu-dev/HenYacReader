# Windows 本地构建

当前验收环境：Windows x64、Qt 6.9.3 `msvc2022_64`、MSVC 2022 14.44、CMake 4.4.4、Ninja。上游应用版本仍为 10.5.0，本项目数据库结构版本为 10.5.1。

## 标准 CMake 构建

通用依赖安装及平台说明见仓库根目录 [INSTALL.md](../../INSTALL.md)。源码根目录执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt目录> -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DDECOMPRESSION_BACKEND=7zip -DPDF_BACKEND=pdfium -DBUILD_TESTS=ON
cmake --build build --parallel <CPU线程数>
ctest --test-dir build --output-on-failure
```

Windows 命令需在 MSVC x64 环境中执行；运行时需有 Qt、7zip、PDFium 等 DLL。不要单独移动 EXE。

## 本机脚本

`scripts/local/build-baseline.ps1` 使用 `.build-tools` 中已安装的工具链，不安装依赖。原工作区将 `.build-tools`、`artifacts` 留在源码仓库外；普通独立 clone 则默认在仓库根目录寻找这两个目录。

在仓库根目录执行：

```powershell
.\scripts\local\build-baseline.ps1 -Step Build
.\scripts\local\build-baseline.ps1 -Step Test
.\scripts\local\build-baseline.ps1 -Step Deploy -OutputDirectory artifacts\review
```

首次生成或重新配置使用 `-Step Configure`；一次执行全部步骤使用 `-Step All`。可以用 `-Target YACReaderLibrary,comic_metadata_editor_test` 限定构建目标。实际构建目录为仓库内 `build-baseline`。原工作区的 `scripts/build-baseline.ps1` 兼容入口仍可用。

脚本仅修改当前进程环境：导入 `vcvars64.bat`、加入 Qt 和工具 PATH、使用项目内临时测试目录；并按 CPU 线程数并行编译。

## MSVC 依赖编码修复

本机只有中文 MSVC 语言资源。CMake 曾把 UTF-8 的 `/showIncludes` 前缀按 CP936 误解码，导致 Ninja 记录 0 条头文件依赖；修改编辑器成员后漏编调用方，曾造成启动堆损坏 `0xc0000374`。

`repair-msvc-dependencies.py` 在 Build 前修正这个已确认的误解码前缀；发生修正时清除旧 `.ninja_deps`，重新采集依赖。构建结束还核对 `properties_dialog.cpp.obj` 包含编辑器头文件依赖，不完整时直接报错。此修复不是对任意编译器输出进行猜测。

## 验证与验收包

- 全量重编译成功；全部 49 项 CTest 通过，整理后最近一次 62.64 秒。
- 编辑器 Qt Test 18 项、描述信息 Qt Test 12 项通过，已包含于上述 CTest。
- dry-run 已确认编辑器头文件变化会触发调用方重编译。
- 最新验收包位于本机 `artifacts/filename-v1`，通过 `启动文件名功能验收.cmd` 启动；程序窗口已实际启动并持续响应，用户已确认可用。
- 配置环境变量 `YACREADER_DATA_DIR` 可将验收配置及本地 IPC 与正常配置隔离；不设置时保持上游行为。
- 测试覆盖元数据保留、升级、解析、预览确认、同名拒绝、改名恢复、数据库提交失败回滚及阅读进度保持。

验收包、工具链、日志、个人 INI/数据库和压缩包不提交 Git。`create-smoke-comic.py` 可生成三页虚构测试漫画；`metadata_demo_library` 是测试辅助目标。

## 翻译

按根目录 AGENTS.md 使用完整的 `update_translations` / `release_translations` CMake 目标。`scripts/local/fill-editor-translations.py` 用于填充本项目新增简体中文文字，必须在完整更新之后运行。
