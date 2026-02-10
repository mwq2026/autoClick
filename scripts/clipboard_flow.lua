set_speed(1.0)

clipboard_set("clipboard -> Ctrl+V paste demo")

local pid = process_start("notepad.exe")
wait_ms(400)

local hwnd = window_wait("Notepad", 5000, 100) or window_wait("记事本", 5000, 100)
if not hwnd then
  return
end

window_activate(hwnd)
wait_ms(200)

vk_down(17, 0)
vk_press("v")
vk_up(17, 0)
wait_ms(200)
vk_press("enter")

text("clipboard_get() = ")
local s = clipboard_get()
if s then
  text(s)
end
vk_press("enter")
