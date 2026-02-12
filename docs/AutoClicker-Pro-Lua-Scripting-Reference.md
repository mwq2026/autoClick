# AutoClicker-Pro Lua 脚本编写参考手册

> 本文档是 AutoClicker-Pro 内置 Lua 脚本引擎的完整 API 参考（共 129 个函数）。
> 将本文档提供给 AI，即可让 AI 为你编写可直接在 AutoClicker-Pro 中运行的自动化脚本。

---

## 运行环境

| 项目 | 说明 |
|------|------|
| Lua 版本 | Lua 5.4（内置，无需安装） |
| 运行方式 | 在 AutoClicker-Pro 的「Lua 脚本」标签页中编写或加载 `.lua` 文件，点击「运行」执行 |
| 停止方式 | 点击「停止」按钮，或按 `Ctrl+F12` 停止运行 |
| 快捷键 | `Ctrl+F10` 开始/继续回放，`Ctrl+F11` 暂停回放，`Ctrl+F12` 紧急停止 |
| 坐标系 | 所有坐标均为 **屏幕绝对像素坐标**（虚拟桌面坐标系，多显示器时左上角可能为负值） |
| 窗口句柄 | `hwnd` 类型为 lightuserdata，由 `window_find` / `window_from_point` 等函数返回 |
| 按钮参数 | 鼠标按钮可传字符串 `"left"` / `"right"` / `"middle"` 或数字 `1` / `2` / `3` |
| 布尔参数 | 支持 `true/false`、`1/0`、`"true"/"false"`、`"yes"/"no"` |
| 字符编码 | 所有字符串参数均为 **UTF-8** 编码 |
| 标准库 | 完整的 Lua 标准库可用（`string`、`table`、`math`、`io`、`os` 等） |

---

## 脚本编写规则

1. 脚本从第一行开始顺序执行，执行完毕自动结束。
2. 建议在脚本开头调用 `set_speed(1.0)` 设置执行速度。
3. 操作之间用 `wait_ms()` 或 `sleep()` 插入适当延时，确保目标程序有时间响应。
4. 查找窗口时建议用 `window_wait()` 而非 `window_find()`，因为窗口可能尚未出现。
5. 所有可取消的等待函数在用户按下停止时会抛出 `"cancelled"` 错误并终止脚本。
6. 脚本中可使用 `return` 提前退出。
7. 建议使用 `pcall` 包裹可能失败的操作以实现错误处理。

---

## API 完整参考

### 1. 基础控制

| 函数 | 签名 | 说明 |
|------|------|------|
| `set_speed` | `set_speed(factor)` | 设置脚本执行速度倍率。`1.0`=正常，`2.0`=两倍速，`0.5`=半速 |
| `wait_ms` | `wait_ms(ms)` | 等待指定毫秒数（可取消） |
| `sleep` | `sleep(ms)` | `wait_ms` 的别名，功能完全相同 |
| `wait_us` | `wait_us(us)` | 等待指定微秒数（可取消） |
| `playback` | `playback(path_trc) -> boolean` | 回放一个 `.trc` 录制文件 |

```lua
set_speed(1.0)
wait_ms(500)    -- 等待 0.5 秒
sleep(1000)     -- 等待 1 秒（与 wait_ms 相同）
wait_us(16000)  -- 等待约 16ms（一帧）
playback("task.trc")
```

---

### 2. 鼠标操作（底层）

| 函数 | 签名 | 说明 |
|------|------|------|
| `mouse_move` | `mouse_move(x, y)` | 立即移动鼠标到屏幕坐标 |
| `mouse_down` | `mouse_down(btn[, x, y])` | 按下鼠标按键，可选先移动到坐标 |
| `mouse_up` | `mouse_up(btn[, x, y])` | 抬起鼠标按键 |
| `mouse_wheel` | `mouse_wheel(delta[, x, y[, horizontal]])` | 滚动滚轮。`horizontal=1` 水平滚动 |

- `btn`：`"left"` / `"right"` / `"middle"` 或 `1` / `2` / `3`（4=X1, 5=X2）
- `delta`：正值向上/向右，负值向下/向左。小于 120 时自动乘以 `WHEEL_DELTA(120)`

```lua
mouse_move(500, 300)
mouse_down("left", 100, 200)
wait_ms(60)
mouse_up("left")
mouse_wheel(-3)           -- 向下滚 3 格
mouse_wheel(1, 500, 300)  -- 在 (500,300) 处向上滚 1 格
```

---

### 3. 鼠标操作（拟人）

拟人系列函数使用贝塞尔曲线模拟人类操作轨迹和随机性，适合需要反检测的场景。

| 函数 | 签名 | 说明 |
|------|------|------|
| `human_move` | `human_move(x, y[, speed])` | 拟人曲线移动鼠标。`speed` 默认 `1.0` |
| `human_click` | `human_click(btn[, x, y])` | 拟人方式点击（含按下-随机延时-抬起） |
| `human_scroll` | `human_scroll(delta[, x, y])` | 拟人方式滚动 |

```lua
human_move(800, 600, 1.5)   -- 1.5 倍速拟人移动
human_click("left", 400, 300)
```

---

### 4. 键盘操作

| 函数 | 签名 | 说明 |
|------|------|------|
| `key_down` | `key_down(scancode[, extended])` | 按下扫描码键 |
| `key_up` | `key_up(scancode[, extended])` | 抬起扫描码键 |
| `vk_down` | `vk_down(vk_or_name[, extended])` | 按下虚拟键 |
| `vk_up` | `vk_up(vk_or_name[, extended])` | 抬起虚拟键 |
| `vk_press` | `vk_press(vk_or_char[, hold_ms[, extended]])` | 按下并抬起键。`hold_ms` 默认 60 |
| `text` | `text(str_utf8)` | 通过 Unicode 模式输入 UTF-8 文本 |

`vk_press` 支持三种输入方式：
- 键名字符串：`vk_press("enter")`
- VK 码数字：`vk_press(0x41)` (A 键)
- 单个字符：`vk_press("a")`、`vk_press("A")`（自动处理 Shift）
- 无法映射的字符串会回退到 `SendInput(KEYEVENTF_UNICODE)` 方式

**支持的键名字符串**（不区分大小写）：

| 键名 | 别名 | 说明 | 键名 | 别名 | 说明 |
|------|------|------|------|------|------|
| `"enter"` | `"return"` | 回车 | `"tab"` | — | Tab |
| `"esc"` | `"escape"` | Esc | `"space"` | — | 空格 |
| `"backspace"` | `"bs"` | 退格 | `"delete"` | `"del"` | Delete |
| `"insert"` | `"ins"` | Insert | `"home"` | — | Home |
| `"end"` | — | End | `"pageup"` | `"pgup"` | Page Up |
| `"pagedown"` | `"pgdn"` | Page Down | `"left"` | — | 左箭头 |
| `"right"` | — | 右箭头 | `"up"` | — | 上箭头 |
| `"down"` | — | 下箭头 | | | |

> 注意：`"ctrl"`、`"shift"`、`"alt"` 不在命名键列表中，需使用 VK 码：`vk_down(0xA2)` (Ctrl)、`vk_down(0xA0)` (Shift)、`vk_down(0xA4)` (Alt)，或使用通用 VK 码 `17` (Ctrl)、`16` (Shift)、`18` (Alt)。

```lua
-- 输入文本
text("Hello, 你好世界！")

-- 按回车
vk_press("enter")

-- 组合键 Ctrl+C
vk_down(0xA2)       -- 左 Ctrl 按下
vk_press("c")
vk_up(0xA2)         -- 左 Ctrl 抬起

-- 组合键 Ctrl+V
vk_down(17)          -- VK_CONTROL
vk_press("v")
vk_up(17)

-- 按 Shift+A（大写 A）
vk_press("A")       -- 自动处理 Shift
```

---

### 5. 窗口查找与等待

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_find` | `window_find(title[, class[, visible_only[, skip_self]]])` | `hwnd` 或 `nil` | 按标题/类名模糊查找第一个匹配窗口 |
| `window_find_all` | `window_find_all(title[, class[, visible_only[, skip_self]]])` | `{hwnd,...}` | 查找所有匹配窗口 |
| `window_wait` | `window_wait(title, timeout_ms[, interval[, class[, visible_only[, skip_self]]]])` | `hwnd` 或 `nil` | 等待窗口出现（可取消） |
| `window_from_point` | `window_from_point(x, y)` | `hwnd` 或 `nil` | 获取坐标处顶层窗口（跳过自身） |
| `window_foreground` | `window_foreground()` | `hwnd` 或 `nil` | 获取当前前台窗口（跳过自身） |

- `title` / `class`：子串模糊匹配
- `visible_only`：默认 `true`，只查找可见窗口
- `skip_self`：默认 `true`，跳过 AutoClicker-Pro 自身
- `interval`：轮询间隔毫秒，默认 `50`

```lua
local hwnd = window_find("记事本")
local all = window_find_all("Chrome")
local hwnd = window_wait("记事本", 5000, 100)
local hwnd = window_from_point(500, 300)
local fg = window_foreground()
```

---

### 6. 窗口信息

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_is_valid` | `window_is_valid(hwnd)` | `boolean` | 判断窗口句柄是否有效 |
| `window_title` | `window_title(hwnd)` | `string` 或 `nil` | 读取窗口标题 |
| `window_class` | `window_class(hwnd)` | `string` 或 `nil` | 读取窗口类名 |
| `window_pid` | `window_pid(hwnd)` | `integer` 或 `nil` | 获取窗口进程 PID |
| `window_rect` | `window_rect(hwnd)` | `x, y, w, h` 或 `nil` | 获取窗口矩形（屏幕坐标） |
| `window_client_rect` | `window_client_rect(hwnd)` | `w, h` 或 `nil` | 获取客户区大小 |

```lua
local hwnd = window_find("记事本")
if window_is_valid(hwnd) then
    print("标题: " .. (window_title(hwnd) or ""))
    print("类名: " .. (window_class(hwnd) or ""))
    print("PID: " .. (window_pid(hwnd) or 0))
    local x, y, w, h = window_rect(hwnd)
    print(string.format("位置: (%d,%d) 大小: %dx%d", x, y, w, h))
end
```

---

### 7. 窗口控制

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_activate` | `window_activate(hwnd)` | `boolean` | 激活并置前窗口 |
| `window_activate_at` | `window_activate_at(x, y)` | `boolean` | 按坐标激活顶层窗口 |
| `window_set_topmost` | `window_set_topmost(hwnd, on)` | `boolean` | 设置/取消窗口置顶 |
| `window_bring_to_top` | `window_bring_to_top(hwnd)` | `boolean` | 放到最前（不置顶） |
| `window_send_to_back` | `window_send_to_back(hwnd)` | `boolean` | 放到最后 |
| `window_show` | `window_show(hwnd)` | `boolean` | 显示窗口 |
| `window_hide` | `window_hide(hwnd)` | `boolean` | 隐藏窗口 |
| `window_minimize` | `window_minimize(hwnd)` | `boolean` | 最小化 |
| `window_maximize` | `window_maximize(hwnd)` | `boolean` | 最大化 |
| `window_restore` | `window_restore(hwnd)` | `boolean` | 还原 |
| `window_move` | `window_move(hwnd, x, y)` | `boolean` | 移动窗口 |
| `window_resize` | `window_resize(hwnd, w, h)` | `boolean` | 调整大小 |
| `window_set_rect` | `window_set_rect(hwnd, x, y, w, h)` | `boolean` | 移动并调整大小 |
| `window_close` | `window_close(hwnd)` | `boolean` | 温和关闭 (WM_CLOSE) |
| `window_close_force` | `window_close_force(hwnd[, wait_ms])` | `boolean` | 强制关闭（超时杀进程），默认等待 3000ms |

```lua
local hwnd = window_find("记事本")
window_activate(hwnd)
window_set_topmost(hwnd, true)   -- 置顶
window_move(hwnd, 100, 100)
window_resize(hwnd, 800, 600)
```

---

### 8. 窗口树遍历（Spy++ 风格）

这组 API 可以像 Spy++ 一样遍历整个窗口层级结构。

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_parent` | `window_parent(hwnd)` | `hwnd` 或 `nil` | 获取父窗口 |
| `window_owner` | `window_owner(hwnd)` | `hwnd` 或 `nil` | 获取所有者窗口 |
| `window_child` | `window_child(hwnd)` | `hwnd` 或 `nil` | 获取第一个子窗口 |
| `window_next_sibling` | `window_next_sibling(hwnd)` | `hwnd` 或 `nil` | 获取下一个兄弟窗口 |
| `window_prev_sibling` | `window_prev_sibling(hwnd)` | `hwnd` 或 `nil` | 获取上一个兄弟窗口 |
| `window_children` | `window_children(hwnd[, recursive])` | `{hwnd,...}` | 枚举子窗口列表。`recursive=true` 递归 |
| `window_desktop` | `window_desktop()` | `hwnd` | 获取桌面窗口句柄 |

```lua
-- 递归遍历窗口树
function dump_tree(hwnd, depth)
    depth = depth or 0
    local prefix = string.rep("  ", depth)
    local cls = window_class(hwnd) or "?"
    local title = window_title(hwnd) or ""
    print(prefix .. cls .. " \"" .. title .. "\"")
    local child = window_child(hwnd)
    while child do
        dump_tree(child, depth + 1)
        child = window_next_sibling(child)
    end
end

-- 枚举记事本所有子控件
local hwnd = window_find("记事本")
local children = window_children(hwnd, true)
for i, child in ipairs(children) do
    print(string.format("#%d class=%s text=%s",
        i, window_class(child) or "", control_get_text(child) or ""))
end
```

---

### 9. 窗口属性（Spy++ 风格）

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_style` | `window_style(hwnd)` | `integer` | 获取 WS_ 样式值 |
| `window_exstyle` | `window_exstyle(hwnd)` | `integer` | 获取 WS_EX_ 扩展样式值 |
| `window_set_style` | `window_set_style(hwnd, style)` | `boolean` | 设置窗口样式 |
| `window_set_exstyle` | `window_set_exstyle(hwnd, exstyle)` | `boolean` | 设置窗口扩展样式 |
| `window_is_visible` | `window_is_visible(hwnd)` | `boolean` | 窗口是否可见 |
| `window_is_enabled` | `window_is_enabled(hwnd)` | `boolean` | 窗口是否启用 |
| `window_is_focused` | `window_is_focused(hwnd)` | `boolean` | 窗口是否拥有焦点 |
| `window_is_minimized` | `window_is_minimized(hwnd)` | `boolean` | 窗口是否最小化 |
| `window_is_maximized` | `window_is_maximized(hwnd)` | `boolean` | 窗口是否最大化 |
| `window_thread_id` | `window_thread_id(hwnd)` | `integer` | 获取窗口线程 ID |
| `window_text_length` | `window_text_length(hwnd)` | `integer` | 获取控件文本长度（字符数） |

```lua
-- 去掉窗口标题栏
local WS_CAPTION = 0x00C00000
local style = window_style(hwnd)
window_set_style(hwnd, style & ~WS_CAPTION)

-- 检查窗口状态
if window_is_visible(hwnd) and not window_is_minimized(hwnd) then
    print("窗口可见且未最小化")
end
```

---

### 10. 控件文本与状态

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `control_get_text` | `control_get_text(hwnd)` | `string` 或 `nil` | 读取控件文本 (WM_GETTEXT) |
| `control_set_text` | `control_set_text(hwnd, text)` | `boolean` | 设置控件文本 (WM_SETTEXT) |
| `window_enable` | `window_enable(hwnd, enable)` | `boolean` | 启用/禁用窗口或控件 |
| `window_set_focus` | `window_set_focus(hwnd)` | `boolean` | 设置键盘焦点到窗口 |

```lua
local edit = find_child_by_class(hwnd, "Edit", 0)
local txt = control_get_text(edit)
print("内容: " .. (txt or ""))
control_set_text(edit, "新内容")
```

---

### 11. 消息机制

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `window_send_msg` | `window_send_msg(hwnd, msg[, wParam[, lParam]])` | `integer` | SendMessage 同步发送，返回结果 |
| `window_post_msg` | `window_post_msg(hwnd, msg[, wParam[, lParam]])` | `boolean` | PostMessage 异步投递 |

- `wParam` / `lParam` 默认为 `0`
- SendMessage 会阻塞直到目标窗口处理完消息
- PostMessage 投递后立即返回

```lua
window_send_msg(hwnd, 0x0010)              -- WM_CLOSE
window_post_msg(hwnd, 0x0400 + 1, 42, 0)  -- WM_USER + 1
```

---

### 12. 控件操作（按钮、复选框、组合框、列表框）

#### 按钮

| 函数 | 签名 | 说明 |
|------|------|------|
| `button_click` | `button_click(hwnd)` | 点击按钮 (BM_CLICK) |

#### 复选框

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `checkbox_get` | `checkbox_get(hwnd)` | `integer` | 获取状态：0=未选, 1=选中, 2=不确定 |
| `checkbox_set` | `checkbox_set(hwnd, state)` | `boolean` | 设置状态 |

#### 组合框 (ComboBox)

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `combo_get_sel` | `combo_get_sel(hwnd)` | `integer` | 获取当前选中索引（0-based，-1=无选中） |
| `combo_set_sel` | `combo_set_sel(hwnd, index)` | `boolean` | 设置选中项 |
| `combo_get_count` | `combo_get_count(hwnd)` | `integer` | 获取项数 |
| `combo_get_item` | `combo_get_item(hwnd, index)` | `string` 或 `nil` | 获取指定项文本 |

#### 列表框 (ListBox)

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `listbox_get_sel` | `listbox_get_sel(hwnd)` | `integer` | 获取当前选中索引 |
| `listbox_set_sel` | `listbox_set_sel(hwnd, index)` | `boolean` | 设置选中项 |
| `listbox_get_count` | `listbox_get_count(hwnd)` | `integer` | 获取项数 |
| `listbox_get_item` | `listbox_get_item(hwnd, index)` | `string` 或 `nil` | 获取指定项文本 |

```lua
-- 遍历组合框所有项
local combo = find_child_by_class(hwnd, "ComboBox", 0)
for i = 0, combo_get_count(combo) - 1 do
    print(i .. ": " .. (combo_get_item(combo, i) or ""))
end
combo_set_sel(combo, 2)  -- 选中第 3 项（0-based）

-- 操作复选框
local chk = find_child_by_text(dlg, "启用")
if checkbox_get(chk) == 0 then
    checkbox_set(chk, 1)  -- 勾选
end
```

---

### 13. 编辑框操作 (Edit Control)

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `edit_get_line_count` | `edit_get_line_count(hwnd)` | `integer` | 获取行数 |
| `edit_get_line` | `edit_get_line(hwnd, line)` | `string` 或 `nil` | 获取指定行文本（0-based） |
| `edit_set_sel` | `edit_set_sel(hwnd, start, end)` | `boolean` | 设置选区（字符索引） |
| `edit_replace_sel` | `edit_replace_sel(hwnd, text)` | `boolean` | 替换当前选区文本 |
| `edit_get_sel` | `edit_get_sel(hwnd)` | `start, end` | 获取选区范围 |

```lua
-- 在编辑框末尾追加文本
local edit = find_child_by_class(hwnd, "Edit", 0)
local len = window_text_length(edit)
edit_set_sel(edit, len, len)
edit_replace_sel(edit, "\n追加的新行")

-- 读取每一行
for i = 0, edit_get_line_count(edit) - 1 do
    print(edit_get_line(edit, i) or "")
end
```

---

### 14. 滚动条操作

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `scroll_set` | `scroll_set(hwnd, bar, pos)` | `boolean` | 设置滚动位置。`bar`: 0=水平, 1=垂直 |
| `scroll_get_pos` | `scroll_get_pos(hwnd, bar)` | `integer` | 获取滚动位置 |
| `scroll_get_range` | `scroll_get_range(hwnd, bar)` | `min, max` | 获取滚动范围 |

```lua
-- 滚动到顶部
scroll_set(edit, 1, 0)
-- 获取垂直滚动范围
local min, max = scroll_get_range(edit, 1)
```

---

### 15. 高级控件（Tab / TreeView / ListView）

#### Tab 控件

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `tab_get_sel` | `tab_get_sel(hwnd)` | `integer` | 获取当前选中页索引（0-based） |
| `tab_set_sel` | `tab_set_sel(hwnd, index)` | `boolean` | 设置选中页 |
| `tab_get_count` | `tab_get_count(hwnd)` | `integer` | 获取页数 |

#### TreeView 控件

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `treeview_get_count` | `treeview_get_count(hwnd)` | `integer` | 获取节点总数 |
| `treeview_get_sel` | `treeview_get_sel(hwnd)` | `hItem` 或 `nil` | 获取选中节点句柄 |
| `treeview_select` | `treeview_select(hwnd, hItem)` | `boolean` | 选中指定节点 |

#### ListView 控件

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `listview_get_count` | `listview_get_count(hwnd)` | `integer` | 获取项数 |
| `listview_get_sel_count` | `listview_get_sel_count(hwnd)` | `integer` | 获取选中项数 |
| `listview_next_sel` | `listview_next_sel(hwnd[, start])` | `index` 或 `nil` | 获取下一个选中项索引。`start` 默认 -1（从头开始） |

```lua
-- 遍历 ListView 选中项
local lv = find_child_by_class(hwnd, "SysListView32", 0)
local count = listview_get_count(lv)
print("总项数: " .. count)

local idx = listview_next_sel(lv)
while idx do
    print("选中: " .. idx)
    idx = listview_next_sel(lv, idx)
end
```

---

### 16. 子控件查找

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `find_child_by_class` | `find_child_by_class(hwnd, class[, index])` | `hwnd` 或 `nil` | 按类名查找第 N 个子控件。`index` 从 0 开始，默认 0 |
| `find_child_by_text` | `find_child_by_text(hwnd, text_substr)` | `hwnd` 或 `nil` | 按文本子串模糊查找子控件 |

```lua
-- 查找记事本编辑区域
local edit = find_child_by_class(notepad, "Edit", 0)

-- 查找第二个 Button
local btn2 = find_child_by_class(dlg, "Button", 1)

-- 按文本查找按钮
local ok = find_child_by_text(dlg, "确定")
if ok then button_click(ok) end
```

---

### 17. 进程操作

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `process_start` | `process_start(path[, args[, cwd]])` | `pid` 或 `nil` | 启动进程 (CreateProcess) |
| `process_is_running` | `process_is_running(pid)` | `boolean` | 判断进程是否仍在运行 |
| `process_wait` | `process_wait(pid, timeout_ms)` | `boolean` | 等待进程退出（可取消） |
| `process_kill` | `process_kill(pid[, exit_code])` | `boolean` | 强制结束进程。`exit_code` 默认 1 |

- `path`：可执行文件路径（绝对或 PATH 中的程序名）
- `args`：命令行参数字符串
- `cwd`：工作目录

```lua
-- 启动记事本
local pid = process_start("notepad.exe")

-- 启动带参数的程序
local pid = process_start("cmd.exe", "/c dir C:\\", "C:\\")

-- 等待进程退出
if pid then
    local exited = process_wait(pid, 10000)
    if not exited then
        process_kill(pid)
    end
end
```

---

### 18. 剪贴板

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `clipboard_set` | `clipboard_set(text_utf8)` | `boolean` | 写入剪贴板文本 |
| `clipboard_get` | `clipboard_get()` | `string` 或 `nil` | 读取剪贴板文本 |

```lua
clipboard_set("复制的内容")
local txt = clipboard_get()
print(txt or "剪贴板为空")
```

---

### 19. 屏幕与视觉

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `screen_size` | `screen_size()` | `w, h` | 虚拟屏幕总大小（多显示器合并） |
| `cursor_pos` | `cursor_pos()` | `x, y` | 鼠标当前屏幕位置 |
| `cursor_set` | `cursor_set(x, y)` | `boolean` | 设置鼠标位置 |
| `pixel_get` | `pixel_get(x, y)` | `r, g, b` 或 `nil` | 获取屏幕像素颜色 (0-255) |
| `color_wait` | `color_wait(x, y, r, g, b[, tol[, timeout[, interval]]])` | `boolean` | 等待像素颜色匹配（可取消） |
| `screen_capture` | `screen_capture(x, y, w, h, bmp_path)` | `boolean` | 截取屏幕区域保存为 24 位 BMP |

- `tol`：颜色容差（每通道），默认 `0`
- `timeout`：超时毫秒，默认 `5000`
- `interval`：轮询间隔毫秒，默认 `50`

```lua
-- 获取像素颜色
local r, g, b = pixel_get(500, 300)
print(string.format("RGB: %d, %d, %d", r, g, b))

-- 等待某位置变为绿色（容差 50，超时 30 秒）
local ok = color_wait(960, 540, 0, 255, 0, 50, 30000, 100)
if ok then human_click("left", 960, 540) end

-- 截取窗口
local x, y, w, h = window_rect(hwnd)
screen_capture(x, y, w, h, "window.bmp")
```

---

### 20. 显示器与 DPI

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `monitor_count` | `monitor_count()` | `integer` | 显示器数量 |
| `monitor_rect` | `monitor_rect(index)` | `x, y, w, h` 或 `nil` | 指定显示器矩形。`index` 从 0 开始 |
| `system_dpi` | `system_dpi()` | `integer` | 系统 DPI（通常 96/120/144） |
| `window_dpi` | `window_dpi(hwnd)` | `integer` | 窗口所在显示器的 DPI |

```lua
local n = monitor_count()
for i = 0, n - 1 do
    local x, y, w, h = monitor_rect(i)
    print(string.format("显示器 %d: %dx%d 位置(%d,%d)", i, w, h, x, y))
end
print("系统 DPI: " .. system_dpi())
```

---

### 21. 注册表

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `reg_read` | `reg_read(key, value_name)` | `string` 或 `nil` | 读取 REG_SZ 字符串值 |
| `reg_write` | `reg_write(key, value_name, data)` | `boolean` | 写入 REG_SZ 字符串值 |
| `reg_read_dword` | `reg_read_dword(key, value_name[, default])` | `integer` | 读取 REG_DWORD 值。未找到返回 `default`（默认 0） |
| `reg_write_dword` | `reg_write_dword(key, value_name, data)` | `boolean` | 写入 REG_DWORD 值 |

`key` 格式：`"根键\\子路径"`，支持的根键前缀：

| 缩写 | 全名 |
|------|------|
| `HKLM` | `HKEY_LOCAL_MACHINE` |
| `HKCU` | `HKEY_CURRENT_USER` |
| `HKCR` | `HKEY_CLASSES_ROOT` |
| `HKU` | `HKEY_USERS` |

```lua
-- 读取系统信息
local os_name = reg_read("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName")
print("操作系统: " .. (os_name or "未知"))

-- 写入自定义项
reg_write("HKCU\\Software\\MyScript", "LastRun", os.date())
reg_write_dword("HKCU\\Software\\MyScript", "RunCount", 1)

-- 读取 DWORD（带默认值）
local count = reg_read_dword("HKCU\\Software\\MyScript", "RunCount", 0)
```

---

### 22. 环境变量

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `env_get` | `env_get(name)` | `string` 或 `nil` | 读取当前进程环境变量 |
| `env_set` | `env_set(name, value)` | `boolean` | 设置当前进程环境变量 |

```lua
local temp = env_get("TEMP")
print("TEMP: " .. (temp or "未设置"))
env_set("MY_VAR", "hello")
```

---

### 23. 文件系统

| 函数 | 签名 | 返回值 | 说明 |
|------|------|--------|------|
| `file_exists` | `file_exists(path)` | `boolean` | 判断文件是否存在 |
| `dir_exists` | `dir_exists(path)` | `boolean` | 判断目录是否存在 |
| `file_delete` | `file_delete(path)` | `boolean` | 删除文件 |
| `dir_create` | `dir_create(path)` | `boolean` | 创建目录（含中间目录） |
| `file_size` | `file_size(path)` | `integer` | 获取文件大小（字节），失败返回 -1 |

```lua
if not dir_exists("output") then
    dir_create("output")
end
if file_exists("output/old.txt") then
    file_delete("output/old.txt")
end
print("文件大小: " .. file_size("data.bin") .. " 字节")
```

---

### 24. 调试与对话框

#### `msgbox(text[, title[, flags]]) -> integer`

弹出 Windows 消息框，脚本阻塞直到用户关闭。

| 参数 | 类型 | 说明 |
|------|------|------|
| `text` | string | 消息内容 |
| `title` | string（可选） | 标题，默认 `"AutoClicker-Pro"` |
| `flags` | integer（可选） | MessageBox 标志位，默认 `0` (MB_OK) |
| **返回** | integer | 按钮 ID：1=OK, 2=Cancel, 6=Yes, 7=No |

常用 flags 值：

| 值 | 常量名 | 按钮 |
|----|--------|------|
| 0 | MB_OK | 确定 |
| 1 | MB_OKCANCEL | 确定 / 取消 |
| 4 | MB_YESNO | 是 / 否 |
| 3 | MB_YESNOCANCEL | 是 / 否 / 取消 |

```lua
msgbox("脚本执行完毕")

-- 确认对话框
local r = msgbox("是否继续？", "确认", 4)
if r ~= 6 then return end  -- 6 = Yes
```

---

### 25. 高级

| 函数 | 签名 | 说明 |
|------|------|------|
| `set_target_window` | `set_target_window(hwnd)` | 设置目标窗口（供部分模式使用） |
| `clear_target_window` | `clear_target_window()` | 清除目标窗口 |
| `activate_window` | `activate_window([x, y])` | 按坐标激活顶层窗口（兼容旧脚本，建议用 `window_activate_at`） |

---

## 综合示例

### 示例 1：记事本自动化

```lua
-- 打开记事本，输入文本，保存
process_start("notepad.exe")
local hwnd = window_wait("记事本", 5000)
if not hwnd then
    msgbox("未找到记事本窗口")
    return
end

window_activate(hwnd)
wait_ms(300)
text("Hello, AutoClicker-Pro!\n这是自动输入的文本。")
wait_ms(500)

-- Ctrl+S 保存
vk_down(0xA2)    -- 左 Ctrl
vk_press("s")
vk_up(0xA2)
```

### 示例 2：Spy++ 风格窗口树遍历

```lua
function dump_window_tree(hwnd, indent)
    indent = indent or 0
    local prefix = string.rep("  ", indent)
    local cls = window_class(hwnd) or "?"
    local title = window_title(hwnd) or ""
    local vis = window_is_visible(hwnd) and "V" or "H"
    local x, y, w, h = window_rect(hwnd)

    print(string.format("%s[%s] class=%s title=\"%s\" rect=(%d,%d,%d,%d)",
        prefix, vis, cls, title, x or 0, y or 0, w or 0, h or 0))

    local child = window_child(hwnd)
    while child do
        dump_window_tree(child, indent + 1)
        child = window_next_sibling(child)
    end
end

local hwnd = window_find("记事本")
if hwnd then dump_window_tree(hwnd) end
```

### 示例 3：对话框控件自动化

```lua
local dlg = window_wait("设置", 3000)
if not dlg then return end

-- 勾选复选框
local chk = find_child_by_text(dlg, "启用通知")
if chk and checkbox_get(chk) == 0 then
    checkbox_set(chk, 1)
end

-- 选择组合框第 2 项
local combo = find_child_by_class(dlg, "ComboBox", 0)
if combo then combo_set_sel(combo, 1) end

-- 点击确定
local ok = find_child_by_text(dlg, "确定")
if ok then button_click(ok) end
```

### 示例 4：编辑框批量替换

```lua
local notepad = window_find("记事本")
local edit = find_child_by_class(notepad, "Edit", 0)
    or find_child_by_class(notepad, "RichEditD2DPT", 0)
if not edit then return end

local full = control_get_text(edit)
if full then
    control_set_text(edit, string.gsub(full, "旧文本", "新文本"))
end
```

### 示例 5：多显示器截图

```lua
dir_create("screenshots")
local n = monitor_count()
for i = 0, n - 1 do
    local x, y, w, h = monitor_rect(i)
    if x then
        screen_capture(x, y, w, h, string.format("screenshots/monitor_%d.bmp", i))
    end
end
msgbox(string.format("已截取 %d 个显示器", n))
```

### 示例 6：剪贴板监控

```lua
local last = clipboard_get() or ""
local log = io.open("clipboard_log.txt", "a")

for i = 1, 100 do
    wait_ms(1000)
    local cur = clipboard_get() or ""
    if cur ~= last then
        log:write(string.format("[%s] %s\n", os.date(), cur))
        log:flush()
        print("变化: " .. cur)
        last = cur
    end
end
log:close()
```

### 示例 7：窗口样式修改（去边框全屏）

```lua
local hwnd = window_find("目标窗口")
if not hwnd then return end

local WS_CAPTION    = 0x00C00000
local WS_THICKFRAME = 0x00040000

local style = window_style(hwnd)
window_set_style(hwnd, style & ~WS_CAPTION & ~WS_THICKFRAME)

local sw, sh = screen_size()
window_set_rect(hwnd, 0, 0, sw, sh)
window_set_topmost(hwnd, true)
```

### 示例 8：注册表与系统信息

```lua
local os_name = reg_read("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName")
local build = reg_read("HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild")
print("系统: " .. (os_name or "?") .. " Build " .. (build or "?"))
print("DPI: " .. system_dpi())
print("TEMP: " .. (env_get("TEMP") or "?"))
```

### 示例 9：等待颜色变化后点击

```lua
print("等待目标变绿...")
local ok = color_wait(960, 540, 0, 255, 0, 50, 30000, 100)
if ok then
    human_click("left", 960, 540)
    print("已点击")
else
    print("超时")
end
```

### 示例 10：完整自动化流程（启动-操作-验证-关闭）

```lua
set_speed(1.0)

-- 1. 启动程序
local pid = process_start("notepad.exe")
local hwnd = window_wait("记事本", 5000)
if not hwnd then
    msgbox("启动失败")
    return
end

-- 2. 操作
window_activate(hwnd)
wait_ms(200)
text("自动化测试内容\n第二行\n第三行")
wait_ms(300)

-- 3. 验证
local edit = find_child_by_class(hwnd, "Edit", 0)
    or find_child_by_class(hwnd, "RichEditD2DPT", 0)
if edit then
    local content = control_get_text(edit)
    if content and string.find(content, "自动化测试") then
        print("验证通过：内容已写入")
    end
end

-- 4. 截图留证
local x, y, w, h = window_rect(hwnd)
screen_capture(x, y, w, h, "result.bmp")

-- 5. 关闭（不保存）
window_close(hwnd)
wait_ms(500)
-- 如果弹出保存对话框，点击"不保存"
local save_dlg = window_find("记事本", "", true, true)
if save_dlg then
    local no_btn = find_child_by_text(save_dlg, "不保存")
        or find_child_by_text(save_dlg, "Don't Save")
    if no_btn then button_click(no_btn) end
end

print("自动化流程完成")
```

---

## 附录 A：常用虚拟键码 (VK Code)

`vk_down` / `vk_up` / `vk_press` 支持数字 VK 码。以下为常用值：

| VK 码 | 说明 | VK 码 | 说明 |
|-------|------|-------|------|
| 0x08 | Backspace | 0x09 | Tab |
| 0x0D | Enter | 0x10 | Shift（通用） |
| 0x11 | Ctrl（通用） | 0x12 | Alt（通用） |
| 0x13 | Pause | 0x14 | Caps Lock |
| 0x1B | Escape | 0x20 | Space |
| 0x21 | Page Up | 0x22 | Page Down |
| 0x23 | End | 0x24 | Home |
| 0x25 | 左箭头 | 0x26 | 上箭头 |
| 0x27 | 右箭头 | 0x28 | 下箭头 |
| 0x2C | Print Screen | 0x2D | Insert |
| 0x2E | Delete | 0x30-0x39 | 数字 0-9 |
| 0x41-0x5A | 字母 A-Z | 0x5B | 左 Win |
| 0x5C | 右 Win | 0x5D | 菜单键 |
| 0x60-0x69 | 小键盘 0-9 | 0x6A | 小键盘 * |
| 0x6B | 小键盘 + | 0x6D | 小键盘 - |
| 0x6E | 小键盘 . | 0x6F | 小键盘 / |
| 0x70-0x7B | F1-F12 | 0x90 | Num Lock |
| 0x91 | Scroll Lock | 0xA0 | 左 Shift |
| 0xA1 | 右 Shift | 0xA2 | 左 Ctrl |
| 0xA3 | 右 Ctrl | 0xA4 | 左 Alt |
| 0xA5 | 右 Alt | | |

> 提示：`vk_press` 也支持直接传单个字符，如 `vk_press("a")`、`vk_press("1")`。

---

## 附录 B：常用 Windows 消息常量

用于 `window_send_msg` / `window_post_msg`：

| 常量名 | 值 | 说明 |
|--------|------|------|
| WM_CLOSE | 0x0010 | 关闭窗口 |
| WM_DESTROY | 0x0002 | 销毁窗口 |
| WM_SETTEXT | 0x000C | 设置文本 |
| WM_GETTEXT | 0x000D | 获取文本 |
| WM_COMMAND | 0x0111 | 命令消息 |
| WM_KEYDOWN | 0x0100 | 键按下 |
| WM_KEYUP | 0x0101 | 键抬起 |
| WM_CHAR | 0x0102 | 字符输入 |
| WM_LBUTTONDOWN | 0x0201 | 鼠标左键按下 |
| WM_LBUTTONUP | 0x0202 | 鼠标左键抬起 |
| WM_RBUTTONDOWN | 0x0204 | 鼠标右键按下 |
| WM_RBUTTONUP | 0x0205 | 鼠标右键抬起 |
| WM_USER | 0x0400 | 用户自定义消息起始值 |
| BM_CLICK | 0x00F5 | 按钮点击 |
| BM_GETCHECK | 0x00F0 | 获取复选框状态 |
| BM_SETCHECK | 0x00F1 | 设置复选框状态 |
| CB_GETCOUNT | 0x0146 | 组合框项数 |
| CB_GETCURSEL | 0x0147 | 组合框当前选中 |
| CB_SETCURSEL | 0x014E | 组合框设置选中 |
| CB_GETLBTEXT | 0x0148 | 组合框获取项文本 |
| LB_GETCOUNT | 0x018B | 列表框项数 |
| LB_GETCURSEL | 0x0188 | 列表框当前选中 |
| LB_SETCURSEL | 0x0186 | 列表框设置选中 |
| LB_GETTEXT | 0x0189 | 列表框获取项文本 |
| EM_GETLINECOUNT | 0x00BA | 编辑框行数 |
| EM_GETLINE | 0x00C4 | 编辑框获取行 |
| EM_SETSEL | 0x00B1 | 编辑框设置选区 |
| EM_REPLACESEL | 0x00C2 | 编辑框替换选区 |

---

## 附录 C：常用窗口样式常量

用于 `window_style` / `window_set_style`：

| 常量名 | 值 | 说明 |
|--------|------|------|
| WS_OVERLAPPED | 0x00000000 | 重叠窗口 |
| WS_CAPTION | 0x00C00000 | 标题栏 |
| WS_SYSMENU | 0x00080000 | 系统菜单 |
| WS_THICKFRAME | 0x00040000 | 可调整大小边框 |
| WS_MINIMIZEBOX | 0x00020000 | 最小化按钮 |
| WS_MAXIMIZEBOX | 0x00010000 | 最大化按钮 |
| WS_VISIBLE | 0x10000000 | 可见 |
| WS_DISABLED | 0x08000000 | 禁用 |
| WS_CHILD | 0x40000000 | 子窗口 |
| WS_POPUP | 0x80000000 | 弹出窗口 |
| WS_BORDER | 0x00800000 | 细边框 |
| WS_VSCROLL | 0x00200000 | 垂直滚动条 |
| WS_HSCROLL | 0x00100000 | 水平滚动条 |

用于 `window_exstyle` / `window_set_exstyle`：

| 常量名 | 值 | 说明 |
|--------|------|------|
| WS_EX_TOPMOST | 0x00000008 | 置顶 |
| WS_EX_TRANSPARENT | 0x00000020 | 透明（鼠标穿透） |
| WS_EX_TOOLWINDOW | 0x00000080 | 工具窗口（不在任务栏显示） |
| WS_EX_LAYERED | 0x00080000 | 分层窗口（支持透明度） |
| WS_EX_NOACTIVATE | 0x08000000 | 不激活 |

---

## 注意事项

1. **坐标系统**：所有坐标均为屏幕绝对坐标。多显示器环境下，副屏坐标可能为负值。使用 `monitor_rect()` 获取各显示器的实际位置。

2. **窗口句柄生命周期**：`hwnd` 是 lightuserdata，不要长期缓存。窗口关闭后句柄失效，使用前建议用 `window_is_valid()` 检查。

3. **编码**：所有字符串参数和返回值均为 UTF-8 编码。Windows API 内部使用 UTF-16，引擎自动转换。

4. **权限**：操作其他进程的窗口可能需要管理员权限。注册表 HKLM 写入通常需要管理员权限。

5. **线程安全**：脚本在独立线程中运行。避免在脚本中进行可能导致死锁的操作。

6. **取消机制**：`wait_ms`、`sleep`、`window_wait`、`color_wait`、`process_wait` 等等待函数均支持用户取消。取消时会抛出 `"cancelled"` 错误并终止脚本。

7. **SendMessage vs PostMessage**：`window_send_msg` 是同步的，会等待目标窗口处理完消息后返回；`window_post_msg` 是异步的，投递后立即返回。操作控件时通常使用 SendMessage。

8. **控件操作前提**：`button_click`、`checkbox_*`、`combo_*`、`listbox_*`、`edit_*` 等函数要求传入的 hwnd 是对应类型的控件句柄。使用 `find_child_by_class` 或 `find_child_by_text` 定位控件。

9. **屏幕截图**：`screen_capture` 保存为 24 位 BMP 格式。大分辨率截图文件较大，建议指定合适的区域。

10. **注册表操作**：`reg_read` / `reg_write` 操作 REG_SZ 类型，`reg_read_dword` / `reg_write_dword` 操作 REG_DWORD 类型。key 路径使用 `\\` 分隔。

11. **拟人操作**：`human_move`、`human_click`、`human_scroll` 使用贝塞尔曲线模拟人类操作轨迹，适合需要模拟真实用户行为的场景。

12. **错误处理**：建议使用 `pcall` 包裹可能失败的操作：
    ```lua
    local ok, err = pcall(function()
        local hwnd = window_wait("目标", 5000)
        if not hwnd then error("窗口未找到") end
        -- ... 操作
    end)
    if not ok then print("错误: " .. tostring(err)) end
    ```

13. **索引约定**：控件操作中的 `index` 参数均为 0-based（从 0 开始），与 Windows API 一致。Lua 的 `ipairs` 遍历返回的数组是 1-based。

14. **跨进程控件操作**：`edit_get_line`、`combo_get_item`、`listbox_get_item` 等需要跨进程读取内存的操作，在 64 位系统上操作 32 位进程（或反之）时可能失败。
