# 构建与发行

本阶段验证环境：Windows x64、Qt 6.9.3 MSVC 2022、CMake、Ninja、7zip 和 PDFium。其他平台保留上游构建结构，未完成本改造版发行验证。

0.1.0 发行前完整构建成功，56/56 组 CTest 通过（82.76 秒）。源码推荐用 Git clone 获取；GitHub 自动源码压缩包沿用上游 export-ignore 规则，不包含部分预编译依赖。

通用依赖见根目录 `INSTALL.md`。必须在源码目录外构建，在 MSVC x64 环境执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt目录> -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DDECOMPRESSION_BACKEND=7zip -DPDF_BACKEND=pdfium -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

本机兼容脚本为 `scripts/local/build-baseline.ps1`，读取 `.build-tools` 内已安装工具；构建输出在 `build-baseline`，验收包／日志在被 Git 忽略的 `artifacts`。此脚本包含中文 MSVC 头文件依赖修复，避免增量编译漏编导致启动崩溃。

```powershell
.\scripts\local\build-baseline.ps1 -Step Build
.\scripts\local\build-baseline.ps1 -Step Test
.\scripts\local\stage-release.ps1 -OutputDirectory artifacts/releases/HenYacReader-0.1.0-windows-x64
```

翻译必须先运行完整 CMake `update_translations` 目标，再运行 `scripts/local/fill-editor-translations.py`，随后构建。不得只扫描几个文件。

发行从全新目录按依赖清单部署，验证 13 份 legacy 参考快照不进入编译／资源／翻译，排除源码、测试库、配置、临时脚本和历史程序。禁止直接压缩整个工作区。运行包保留 GPL 与第三方文件，附版本和使用说明。

GitHub 版本使用标签 `v0.1.0` 和对应 `release/v0.1.0` 分支，提供 Windows x64 ZIP 与 SHA-256 校验文件。当前仓库私有，下载需要授权；对外公开发行前需另行调整仓库或发布位置。

源码提交后执行打包；脚本检查源码工作区干净、版本一致，记录提交 ID，并解压读取每个 ZIP 条目验证 SHA-256：

```powershell
python scripts/local/package-release.py --payload artifacts/releases/HenYacReader-0.1.0-windows-x64 --output artifacts/distributions/v0.1.0 --version 0.1.0
```

本机原工作区的 artifacts 位于外层 `D:/GitHub/comic-reader/artifacts`，调用打包脚本时应传入对应绝对路径；独立 clone 按上述仓库内路径使用。修改 VERSION 后先重新执行 CMake 配置。
