# AutoClicker-Pro Lua 脚本编写参考手册

> 本文档是 AutoClicker-Pro 内置 Lua 脚本引擎的完整 API 参考。
> 将本文档提供给 AI，即可让 AI 为你编写可直接在 AutoClicker-Pro 中运行的自动化脚本。

---

## 运行环境

| 项目 | 说明 |
|------|------|
| Lua 版本 | Lua 5.4（内置，无需安装） |
| 运行方式 | 在 AutoClicker-Pro 的「Lua 脚本」标签页中编写或加载 `.lua` 文件，点击「运行」执行 |
| 停止方式 | 点击「停止」按钮，或按 `Ctrl+F12` 紧急停止 |
| 坐标系 | 所有坐标均为 **屏幕绝对像素坐标**（虚拟桌面坐标系，多显示器时左上角可能为负值） |
| 窗口句柄 | `hwnd` 类型为整数（`lua_Integer`），由 `window_find` / `window_from_point` 等函数返回 |
| 按钮参数 | 鼠标按钮可传字符串 `"left"` / `"right"` / `"middle"` 或数字 `1` / `2` / `3` |
| 布尔参数 | 支持 `true/false`、`1/0`、`"true"/"false"`、`"yes"/"no"` |
| 字符编码 | 所有字符串参数均为 **UTF-8** 编码 |
| 标准库 | 完整的 Lua 标准库可用（`string`、`table`、`math`、`io`、`os` 等） |

---

## 脚本编写规则

1. 脚本从第一行开始顺序执行，执行完毕自动结束。
2. 建议在脚本开头调用 `set_speed(1.0)` 设置执行速度。
3. 操作之间用 `wait_ms()` 插入适当延时，确保目标程序有时间响应。
4. 查找窗口时建议用 `window_wait()` 而非 `window_find()`，因为窗口可能尚未出现。
5. 所有可取消的等待函数在用户按下停止时会抛出 `"cancelled"` 错误并终止脚本。
6. 脚本中可使用 `return` 提前退出。

---

## API 完整参考

### 1. 基础控制

#### `set_speed(factor)`
设置脚本执行速度倍率。影响回放速度。

| 参数 | 类型 | 说明 |
|------|------|------|
| `factor` | number | 速度倍率，`1.0` = 正常速度，`2.0` = 两倍速，`0.5` = 半速 |

```lua
set_speed(1.0)
```

#### `wait_ms(ms)`
等待指定毫秒数。可被用户停止操作取消。

| 参数 | 类型 | 说明 |
|------|------|------|
| `ms` | integer | 等待的毫秒数 |

```lua
wait_ms(500)  -- 等待 0.5 秒
```

#### `wait_us(us)`
等待指定微秒数。可被用户停止操作取消。

| 参数 | 类型 | 说明 |
|------|------|------|
| `us` | integer | 等待的微秒数 |

```lua
wait_us(16000)  -- 等待约 16ms（一帧）
```

#### `playback(path_trc)`
回放一个 `.trc` 录制文件。

| 参数 | 类型 | 说明 |
|------|------|------|
| `path_trc` | string | `.trc` 文件路径（相对或绝对路径） |
| **返回** | boolean | 是否成功启动回放 |

```lua
playback("task.trc")
```

---

### 2. 鼠标操作（底层）

#### `mouse_move(x, y)`
立即移动鼠标到指定屏幕坐标。

| 参数 | 类型 | 说明 |
|------|------|------|
| `x` | integer | 屏幕 X 坐标 |
| `y` | integer | 屏幕 Y 坐标 |

```lua
mouse_move(500, 300)
```

#### `mouse_down(btn[, x, y])`
按下鼠标按键。如果提供坐标，先移动到该位置再按下。

| 参数 | 类型 | 说明 |
|------|------|------|
| `btn` | string 或 integer | `"left"` / `"right"` / `"middle"` 或 `1` / `2` / `3`（4=X1, 5=X2） |
| `x` | integer（可选） | 屏幕 X 坐标 |
| `y` | integer（可选） | 屏幕 Y 坐标 |

```lua
mouse_down("left", 100, 200)
```

#### `mouse_up(btn[, x, y])`
抬起鼠标按键。参数同 `mouse_down`。

```lua
mouse_up("left")
```

#### `mouse_wheel(delta[, x, y[, horizontal]])`
滚动鼠标滚轮。

| 参数 | 类型 | 说明 |
|------|------|------|
| `delta` | integer | 滚动量，正值向上/向右，负值向下/向左。小于 `WHEEL_DELTA(120)` 时自动乘以 120 |
| `x` | integer（可选） | 先移动到此 X 坐标 |
| `y` | integer（可选） | 先移动到此 Y 坐标 |
| `horizontal` | boolean（可选） | `1` = 水平滚动，`0` = 垂直滚动（默认） |

```lua
mouse_wheel(-3)           -- 向下滚 3 格
mouse_wheel(1, 500, 300)  -- 在 (500,300) 处向上滚 1 格
```

---

### 3. 鼠标操作（拟人）

拟人系列函数会模拟人类操作的随机性和曲线轨迹，适合需要反检测的场景。

#### `human_move(x, y[, speed])`
以拟人曲线轨迹移动鼠标。

| 参数 | 类型 | 说明 |
|------|------|------|
| `x` | integer | 目标 X 坐标 |
| `y` | integer | 目标 Y 坐标 |
| `speed` | number（可选） | 移动速度，默认 `1.0` |

```lua
human_move(800, 600, 1.5)
```

#### `human_click(btn[, x, y])`
以拟人方式点击鼠标（包含按下-延时-抬起）。

| 参数 | 类型 | 说明 |
|------|------|------|
| `btn` | string 或 integer | `"left"` / `"right"` / `"middle"` 或 `1` / `2` / `3` |

```lua
human_click("left")
```

#### `human_scroll(delta[, x, y])`
以拟人方式滚动。

| 参数 | 类型 | 说明 |
|------|------|------|
| `delta` | integer | 滚动量 |

```lua
human_scroll(-3)
```

---

### 4. 键盘操作

#### `key_down(scancode[, extended])`
按下指定扫描码的键。

| 参数 | 类型 | 说明 |
|------|------|------|
| `scancode` | integer | 键盘扫描码 |
| `extended` | boolean（可选） | 是否为扩展键，默认 `false` |

#### `key_up(scancode[, extended])`
抬起指定扫描码的键。参数同 `key_down`。

#### `vk_down(vk_or_name[, extended])`
按下虚拟键。

| 参数 | 类型 | 说明 |
|------|------|------|
| `vk_or_name` | integer 或 string | VK 码（如 `0x41`）或键名（见下表） |
| `extended` | boolean（可选） | 是否为扩展键 |

#### `vk_up(vk_or_name[, extended])`
抬起虚拟键。参数同 `vk_down`。

#### `vk_press(vk_or_char[, hold_ms[, extended]])`
按下并抬起一个键（完整的按键动作）。如果传入单个字符，会自动处理 Shift 等修饰键。如果传入无法映射为 VK 的字符串，会回退到 `SendInput(KEYEVENTF_UNICODE)` 方式输入。

| 参数 | 类型 | 说明 |
|------|------|------|
| `vk_or_char` | integer 或 string | VK 码、键名、或单个字符（如 `"a"`、`"A"`、`"enter"`） |
| `hold_ms` | integer（可选） | 按住时间（毫秒），默认 `60` |
| `extended` | boolean（可选） | 是否为扩展键 |

```lua
vk_press("enter")       -- 按回车
vk_press("a")           -- 按 a 键
vk_press("A")           -- 按 Shift+A
vk_press(0x41, 100)     -- 按 VK_A 并保持 100ms
```

**支持的键名字符串：**

| 键名 | 说明 | 键名 | 说明 |
|------|------|------|------|
| `"enter"` / `"return"` | 回车 | `"tab"` | Tab |
| `"esc"` / `"escape"` | Esc | `"space"` | 空格 |
| `"backspace"` / `"bs"` | 退格 | `"delete"` / `"del"` | Delete |
| `"insert"` / `"ins"` | Insert | `"home"` | Home |
| `"end"` | End | `"pageup"` / `"pgup"` | Page Up |
| `"pagedown"` / `"pgdn"` | Page Down | `"left"` | 左箭头 |
| `"right"` | 右箭头 | `"up"` | 上箭头 |
| `"down"` | 下箭头 | | |

#### `text(str_utf8)`
直接输入 UTF-8 文本（通过 `SendInput` 的 Unicode 模式逐字符发送）。适合输入中文、日文等非 ASCII 字符。

| 参数 | 类型 | 说明 |
|------|------|------|
| `str_utf8` | string | 要输入的文本（UTF-8） |

```lua
text("Hello, 你好世界！")
```

**组合键示例（Ctrl+C / Ctrl+V）：**

```lua
-- Ctrl+C 复制
vk_down(17, 0)    -- 17 = VK_CONTROL
vk_press("c")
vk_up(17, 0)

-- Ctrl+V 粘贴
vk_down(17, 0)
vk_press("v")
vk_up(17, 0)
```

---

### 5. 窗口操作

#### 查找与等待

##### `window_find(title_substr[, class_substr[, visible_only[, skip_self]]])`
按标题/类名模糊查找顶层窗口，返回第一个匹配的句柄。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `title_substr` | string | — | 窗口标题子串（模糊匹配） |
| `class_substr` | string | `""` | 窗口类名子串（可选） |
| `visible_only` | boolean | `true` | 是否只查找可见窗口 |
| `skip_self` | boolean | `true` | 是否跳过 AutoClicker-Pro 自身 |
| **返回** | hwnd 或 nil | | 窗口句柄，未找到返回 `nil` |

```lua
local hwnd = window_find("记事本")
```

##### `window_find_all(title_substr[, class_substr[, visible_only[, skip_self]]])`
查找所有匹配的窗口，返回句柄数组。参数同 `window_find`。

| **返回** | table | 句柄数组 `{hwnd1, hwnd2, ...}` |

```lua
local all = window_find_all("Chrome")
for i, h in ipairs(all) do
    print(window_title(h))
end
```

##### `window_wait(title_substr, timeout_ms[, interval_ms[, class_substr[, visible_only[, skip_self]]]])`
等待窗口出现，超时返回 `nil`。可被停止操作取消。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `title_substr` | string | — | 窗口标题子串 |
| `timeout_ms` | integer | — | 超时毫秒数 |
| `interval_ms` | integer | `50` | 轮询间隔毫秒 |
| `class_substr` | string | `""` | 窗口类名子串 |
| `visible_only` | boolean | `true` | 是否只查找可见窗口 |
| `skip_self` | boolean | `true` | 是否跳过自身 |
| **返回** | hwnd 或 nil | | 窗口句柄 |

```lua
local hwnd = window_wait("记事本", 5000, 100)
if not hwnd then
    return  -- 超时，退出脚本
end
```

##### `window_from_point(x, y)`
获取指定屏幕坐标处的顶层窗口句柄（跳过 AutoClicker-Pro 自身）。

| **返回** | hwnd 或 nil |

##### `window_foreground()`
获取当前前台窗口句柄（跳过 AutoClicker-Pro 自身）。

| **返回** | hwnd 或 nil |

#### 窗口信息

| 函数 | 签名 | 返回值 |
|------|------|--------|
| `window_is_valid` | `window_is_valid(hwnd)` | `boolean` |
| `window_title` | `window_title(hwnd)` | `string` 或 `nil` |
| `window_class` | `window_class(hwnd)` | `string` 或 `nil` |
| `window_pid` | `window_pid(hwnd)` | `integer` 或 `nil` |
| `window_rect` | `window_rect(hwnd)` | `x, y, w, h` 或 `nil`（屏幕坐标） |
| `window_client_rect` | `window_client_rect(hwnd)` | `w, h` 或 `nil`（客户区大小） |

```lua
local x, y, w, h = window_rect(hwnd)
local cw, ch = window_client_rect(hwnd)
```

#### 窗口控制

| 函数 | 签名 | 说明 | 返回 |
|------|------|------|------|
| `window_activate` | `window_activate(hwnd)` | 激活并置前窗口 | boolean |
| `window_activate_at` | `window_activate_at(x, y)` | 按坐标定位并激活顶层窗口 | boolean |
| `activate_window` | `activate_window([x, y])` | 兼容旧脚本，按坐标激活 | boolean |
| `window_set_topmost` | `window_set_topmost(hwnd, on)` | 设置/取消窗口置顶 | boolean |
| `window_bring_to_top` | `window_bring_to_top(hwnd)` | 放到最前（不置顶） | boolean |
| `window_send_to_back` | `window_send_to_back(hwnd)` | 放到最后 | boolean |
| `window_show` | `window_show(hwnd)` | 显示窗口 | boolean |
| `window_hide` | `window_hide(hwnd)` | 隐藏窗口 | boolean |
| `window_minimize` | `window_minimize(hwnd)` | 最小化 | boolean |
| `window_maximize` | `window_maximize(hwnd)` | 最大化 | boolean |
| `window_restore` | `window_restore(hwnd)` | 还原 | boolean |
| `window_move` | `window_move(hwnd, x, y)` | 移动窗口 | boolean |
| `window_resize` | `window_resize(hwnd, w, h)` | 调整大小 | boolean |
| `window_set_rect` | `window_set_rect(hwnd, x, y, w, h)` | 移动并调整大小 | boolean |
| `window_close` | `window_close(hwnd)` | 温和关闭（发送 WM_CLOSE） | boolean |
| `window_close_force` | `window_close_force(hwnd[, wait_ms])` | 强制关闭（超时后杀进程），默认等待 500ms | boolean |

---

### 6. 进程操作

#### `process_start(path[, args[, cwd]])`
启动一个新进程。

| 参数 | 类型 | 说明 |
|------|------|------|
| `path` | string | 可执行文件路径 |
| `args` | string（可选） | 命令行参数 |
| `cwd` | string（可选） | 工作目录 |
| **返回** | integer 或 nil | 进程 PID，失败返回 `nil` |

```lua
local pid = process_start("notepad.exe")
local pid2 = process_start("cmd.exe", "/c dir", "C:\\")
```

#### `process_is_running(pid)`
判断进程是否仍在运行。返回 `boolean`。

#### `process_wait(pid, timeout_ms)`
等待进程退出。返回 `boolean`（`true` = 已退出）。

#### `process_kill(pid[, exit_code])`
强制结束进程。`exit_code` 默认为 `1`。返回 `boolean`。

---

### 7. 系统与剪贴板

#### `clipboard_set(text_utf8)`
写入文本到系统剪贴板。返回 `boolean`。

#### `clipboard_get()`
读取系统剪贴板文本。返回 `string` 或 `nil`。

```lua
clipboard_set("要粘贴的内容")
local content = clipboard_get()
```

#### `screen_size()`
获取虚拟屏幕大小（多显示器合并区域）。

| **返回** | `w, h` 或 `nil` |

```lua
local w, h = screen_size()
```

#### `cursor_pos()`
获取鼠标当前屏幕坐标。

| **返回** | `x, y` 或 `nil` |

```lua
local x, y = cursor_pos()
```

#### `cursor_set(x, y)`
设置鼠标位置。返回 `boolean`。

---

### 8. 视觉/像素

#### `pixel_get(x, y)`
获取屏幕指定坐标的像素颜色。

| **返回** | `r, g, b`（0-255）或 `nil` |

```lua
local r, g, b = pixel_get(100, 200)
```

#### `color_wait(x, y, r, g, b[, tol[, timeout_ms[, interval_ms]]])`
等待屏幕指定坐标的像素颜色达到目标值。可被停止操作取消。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `x, y` | integer | — | 屏幕坐标 |
| `r, g, b` | integer | — | 目标颜色（0-255） |
| `tol` | integer | `0` | 容差（每通道允许的最大偏差） |
| `timeout_ms` | integer | `2000` | 超时毫秒 |
| `interval_ms` | integer | `50` | 轮询间隔毫秒 |
| **返回** | boolean | | `true` = 颜色匹配，`false` = 超时 |

```lua
-- 等待 (100,200) 处变为红色，容差 10，最多等 5 秒
local ok = color_wait(100, 200, 255, 0, 0, 10, 5000, 50)
```

---

### 9. 高级

#### `set_target_window(hwnd)`
设置目标窗口（供部分内部模式使用）。

#### `clear_target_window()`
清除目标窗口设置。

---

## 完整示例

### 示例 1：打开记事本并输入文字

```lua
set_speed(1.0)

-- 启动记事本
local pid = process_start("notepad.exe")
wait_ms(400)

-- 等待记事本窗口出现（兼容中英文系统）
local hwnd = window_wait("Notepad", 5000, 100)
            or window_wait("记事本", 5000, 100)
if not hwnd then
    return
end

-- 激活窗口并输入文字
window_activate(hwnd)
wait_ms(200)
text("Hello, AutoClicker-Pro!")
vk_press("enter")
text("这是自动输入的文字。")

-- 等待 2 秒后关闭
wait_ms(2000)
window_close(hwnd)
```

### 示例 2：剪贴板操作 + Ctrl+V 粘贴

```lua
set_speed(1.0)

-- 写入剪贴板
clipboard_set("通过剪贴板粘贴的内容")

-- 打开记事本
process_start("notepad.exe")
wait_ms(400)
local hwnd = window_wait("Notepad", 5000) or window_wait("记事本", 5000)
if not hwnd then return end

window_activate(hwnd)
wait_ms(200)

-- Ctrl+V 粘贴
vk_down(17, 0)   -- VK_CONTROL
vk_press("v")
vk_up(17, 0)
wait_ms(200)

-- 读回剪贴板验证
local content = clipboard_get()
if content then
    vk_press("enter")
    text("剪贴板内容: " .. content)
end
```

### 示例 3：像素颜色等待

```lua
set_speed(1.0)

-- 获取当前鼠标位置的像素颜色
local cx, cy = cursor_pos()
if not cx then return end

local r, g, b = pixel_get(cx, cy)
if not r then return end

-- 等待该位置颜色发生变化（容差 0 = 精确匹配当前颜色）
-- 当颜色不再匹配时 color_wait 返回 false（超时）
local still_same = color_wait(cx, cy, r, g, b, 0, 8000, 50)
if still_same then
    text("颜色未变化")
else
    text("检测到颜色变化！")
end
```

### 示例 4：窗口管理

```lua
set_speed(1.0)

-- 查找所有 Chrome 窗口
local all = window_find_all("Chrome")
for i, h in ipairs(all) do
    local title = window_title(h)
    local x, y, w, hh = window_rect(h)
    -- 将所有 Chrome 窗口排列
    if x then
        window_set_rect(h, 100 + (i-1) * 50, 100 + (i-1) * 50, 800, 600)
    end
    wait_ms(100)
end

-- 置顶第一个窗口
if #all > 0 then
    window_set_topmost(all[1], true)
    wait_ms(2000)
    window_set_topmost(all[1], false)
end
```

### 示例 5：拟人操作（反检测）

```lua
set_speed(1.0)

-- 拟人移动到目标位置（带曲线轨迹）
human_move(500, 400, 1.0)
wait_ms(100)

-- 拟人点击
human_click("left")
wait_ms(300)

-- 拟人滚动
human_scroll(-3)
```

### 示例 6：循环点击

```lua
set_speed(1.0)

-- 在指定位置循环点击 10 次，每次间隔 200ms
for i = 1, 10 do
    mouse_move(500, 300)
    mouse_down("left")
    wait_ms(30)
    mouse_up("left")
    wait_ms(200)
end
```

### 示例 7：启动程序并等待退出

```lua
set_speed(1.0)

local pid = process_start("notepad.exe")
if not pid then
    return
end

-- 等待用户手动关闭记事本，最多等 60 秒
local exited = process_wait(pid, 60000)
if exited then
    text("记事本已关闭")
else
    -- 超时，强制结束
    process_kill(pid)
end
```

---

## 常用 VK 码速查

| VK 码 | 十进制 | 说明 |
|--------|--------|------|
| `VK_CONTROL` | 17 | Ctrl |
| `VK_SHIFT` | 16 | Shift |
| `VK_MENU` | 18 | Alt |
| `VK_LWIN` | 91 | 左 Win |
| `VK_F1`~`VK_F12` | 112~123 | F1~F12 |
| `VK_TAB` | 9 | Tab |
| `VK_RETURN` | 13 | Enter |
| `VK_ESCAPE` | 27 | Esc |
| `VK_SPACE` | 32 | 空格 |
| `VK_BACK` | 8 | Backspace |
| `VK_DELETE` | 46 | Delete |
| `0`~`9` | 48~57 | 数字键 |
| `A`~`Z` | 65~90 | 字母键 |

> 提示：`vk_press` / `vk_down` / `vk_up` 支持直接传键名字符串（如 `"enter"`、`"tab"`、`"esc"`），无需记忆 VK 码。

---

## 注意事项

1. **管理员权限**：如果目标程序以管理员权限运行，AutoClicker-Pro 也需要以管理员权限运行，否则键盘/滚轮输入可能被拦截。
2. **屏蔽系统输入**：回放模式支持 `BlockInput` 功能，启用后鼠标键盘暂时不可用，`Ctrl+F12` 可紧急停止。
3. **多显示器**：坐标使用虚拟桌面坐标系，主显示器左上角通常为 `(0,0)`，副显示器坐标可能为负值。
4. **编码**：所有字符串参数和返回值均为 UTF-8 编码。
5. **错误处理**：大多数函数在失败时返回 `nil` 或 `false`，建议检查返回值。
