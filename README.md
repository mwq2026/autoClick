# AutoClicker-Pro

基于 Win32 + DirectX 11 + Dear ImGui 的桌面自动点击工具，支持录制/回放与 Lua 脚本模式，并针对高分辨率/高 DPI 显示做了自适配。

## 功能

- 简单模式：录制与回放（`.trc`）
- 高级模式：Lua 脚本执行与编辑（`.lua`）
- 热键：`Ctrl + F12` 紧急停止
- 高 DPI 自适配：字体/控件尺寸随系统缩放自动调整，跨显示器移动自动更新

## 构建

推荐使用仓库自带脚本编译（会自动选择可用的 Visual Studio CMake，并生成 VS 工程后进行构建）：

```bat
rebuild.bat Release clean
```

仅重新编译（不清理构建目录）：

```bat
rebuild.bat Release
```

构建成功后输出通常在：

- `build\bin\Release\AutoClickerPro.exe`

## 运行与权限

- “屏蔽系统输入（BlockInput）”相关功能可能需要管理员权限。
- 如果编译时报 `LNK1104 cannot open file ...AutoClickerPro.exe`，通常是程序正在运行且被占用；请先退出程序，必要时用管理员权限结束进程后再编译。

## 依赖/技术栈

- Windows (Win32)
- Direct3D 11
- Dear ImGui（Win32 + DX11 backend）
- Lua

