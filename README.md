# AutoClicker-Pro

基于 Win32 + DirectX 11 + Dear ImGui 的桌面自动点击工具，支持录制/回放与 Lua 脚本模式，并针对高分辨率/高 DPI 显示做了自适配。

## ✨ 最新更新 (v2.1 - 2026-02-10)

- 🎨 **全新浅色主题** - 现代化的浅色界面设计
- 📐 **卡片式布局** - 清晰的功能分组和视觉层次
- 🖥️ **响应式设计** - 完美适配任何分辨率和 DPI 缩放
- 🎯 **优化的交互** - 更大的按钮、更好的对齐、更明显的反馈

详细说明请查看：[UI 优化文档](./docs/ui-redesign/README.md)

## 功能

- 简单模式：录制与回放（`.trc`）
- 高级模式：Lua 脚本执行与编辑（`.lua`）
- 热键：`Ctrl + F12` 停止运行
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

## Lua 自动化 API

项目不会在运行脚本时自动插入/自动执行任何“查找窗口/激活窗口/置顶窗口”等逻辑；所有窗口与系统控制都必须在 Lua 中显式调用。

### 窗口句柄（hwnd）

窗口相关 API 使用 `hwnd` 表示一个窗口句柄（在 Lua 中以整数形式传递）。你可以通过 `window_from_point` / `window_find` / `window_foreground` 获得 hwnd，然后传给其它 `window_*` 指令。

### 窗口：查找与信息

- `window_is_valid(hwnd) -> boolean`
- `window_from_point(x, y) -> hwnd | nil`（默认跳过本程序窗口）
- `window_foreground() -> hwnd | nil`（默认跳过本程序窗口）
- `window_find(title_substr[, class_substr[, visible_only[, skip_self]]]) -> hwnd | nil`
- `window_find_all(title_substr[, class_substr[, visible_only[, skip_self]]]) -> {hwnd, ...}`
- `window_wait(title_substr, timeout_ms[, interval_ms[, class_substr[, visible_only[, skip_self]]]]) -> hwnd | nil`
- `window_title(hwnd) -> string | nil`
- `window_class(hwnd) -> string | nil`
- `window_pid(hwnd) -> pid | nil`
- `window_rect(hwnd) -> x, y, w, h | nil`
- `window_client_rect(hwnd) -> w, h | nil`

### 窗口：控制（均需显式调用）

- 激活/置前：
- `activate_window([x, y]) -> boolean`（按坐标激活顶层窗口，兼容旧脚本）
- `window_activate(hwnd) -> boolean`
- `window_activate_at(x, y) -> boolean`
- Z-Order：
- `window_set_topmost(hwnd, on) -> boolean`
- `window_bring_to_top(hwnd) -> boolean`
- `window_send_to_back(hwnd) -> boolean`
- 显示状态：
- `window_show(hwnd) / window_hide(hwnd) -> boolean`
- `window_minimize(hwnd) / window_restore(hwnd) / window_maximize(hwnd) -> boolean`
- 位置与尺寸：
- `window_move(hwnd, x, y) -> boolean`
- `window_resize(hwnd, w, h) -> boolean`
- `window_set_rect(hwnd, x, y, w, h) -> boolean`
- 关闭：
- `window_close(hwnd) -> boolean`（温和关闭，等价于向窗口发送关闭请求）
- `window_close_force(hwnd[, wait_ms]) -> boolean`（高风险：先尝试关闭，超时后终止进程）

注意：Windows 对“前台激活”有系统限制；如果激活返回 `false`，请先用鼠标手动点一下目标窗口再继续脚本。

### 进程：启动/等待/查询/强杀

- `process_start(path[, args[, cwd]]) -> pid | nil`
- `process_is_running(pid) -> boolean`
- `process_wait(pid, timeout_ms) -> boolean`
- `process_kill(pid[, exit_code]) -> boolean`（高风险：强制结束进程）

### 系统：剪贴板/屏幕/光标

- `clipboard_set(text_utf8) -> boolean`
- `clipboard_get() -> string | nil`
- `screen_size() -> w, h | nil`（虚拟屏幕大小，适配多显示器）
- `cursor_pos() -> x, y | nil`
- `cursor_set(x, y) -> boolean`

### 视觉：像素与等待（用于“看图操作”）

- `pixel_get(x, y) -> r, g, b | nil`
- `color_wait(x, y, r, g, b[, tol[, timeout_ms[, interval_ms]]]) -> boolean`

### 输入：鼠标与键盘（已提供）

- 鼠标：`mouse_move(x,y)`、`mouse_down(btn[,x,y])`、`mouse_up(btn[,x,y])`、`mouse_wheel(delta[,x,y,horizontal])`
- `horizontal` 建议传 `0/1`（0=竖向滚轮，1=横向滚轮）
- `delta` 支持 `120/-120`，也支持 `1/-1`（会自动按步进换算）
- 键盘：`key_down(scan[,ext])`、`key_up(scan[,ext])`、`vk_down(vk[,ext])`、`vk_up(vk[,ext])`、`vk_press(vk_or_char[,hold_ms[,ext]])`、`text(str)`
- `ext/horizontal` 一类参数建议传 `0/1`（避免 Lua 把数字 `0` 当成 true 的语义差异）

### 示例：启动记事本并自动输入后关闭

```lua
local pid = process_start("notepad.exe")
wait_ms(300)
local hwnd = window_wait("Notepad", 5000)
if hwnd then
  window_activate(hwnd)
  wait_ms(200)
  text("hello from lua")
  vk_press("enter")
  wait_ms(200)
  window_close(hwnd)
end
```

## 依赖/技术栈

- Windows (Win32)
- Direct3D 11
- Dear ImGui（Win32 + DX11 backend）
- Lua
