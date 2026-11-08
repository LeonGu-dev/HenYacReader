# 书库启动无窗口修复

日期：2026-11-07。

## 复现与定位

用户启动开发版书库后进程存在，但没有可见主窗口；重复启动只会连接到已运行的单实例。

诊断时现有书库数据库版本为 9.16.0，而本次构建要求 10.5.0。主线程调用栈停在 `QMessageBox::question()` 的模态事件循环。首次书库选择发生在 `LibraryWindow` 构造期间，因此升级询问可能在 `main()` 显示主窗口之前阻塞启动。

之前的首次构建验证采用空书库配置，没有覆盖这个旧书库升级分支。

## 修改

`upstream/yacreader/YACReaderLibrary/library_window.cpp`：将初次书库加载、初始页面选择及启动后任务排到下一轮 Qt 事件循环，让 `main()` 先完成窗口显示。数据库升级仍由原有确认流程决定。

`scripts/build-baseline.ps1`：增加可选的 `-Target` 参数，用于只构建需要更新的应用。

```powershell
.\scripts\build-baseline.ps1 -Step Build -Target YACReaderLibrary
```

## 验证

- 书库重新编译、链接成功。
- 使用同一份原配置启动修复版后，主窗口标题 `YACReader Library` 和非零窗口句柄均已出现；Computer Use 窗口清单也能找到该主窗口。
- 后续主线程调用栈显示已进入用户菜单操作路径，不再阻塞在构造期的数据库升级询问。
- clang-format 23.1.5 对修改文件的格式检查通过；`git diff --check` 通过。
- 本轮没有重新运行全部 48 项原版单元测试；本次关键验证为实际旧配置启动复测。

修复后的 EXE 已替换到 `artifacts/baseline/YACReaderLibrary.exe`；原 EXE 备份位于 `artifacts/diagnostics/library-startup/YACReaderLibrary.before.exe`。

修复版 SHA-256：

```text
AC9D9B9C42F293D32AABE28FF96B8484519EC6174145767CDF92A3A7E7189312
```

诊断材料保留在 `artifacts/diagnostics/library-startup`。本轮没有提交 Git commit，等待用户验收。
