set_speed(1.0)

local pid = process_start("notepad.exe")
wait_ms(300)

local function wait_notepad(timeout_ms)
  local t0 = 0
  local h = window_wait("Notepad", timeout_ms, 100)
  if h then return h end
  return window_wait("记事本", timeout_ms, 100)
end

local hwnd = wait_notepad(5000)
if not hwnd then
  return
end

window_activate(hwnd)
wait_ms(200)

text("hello from AutoClickerPro lua")
vk_press("enter")
text("window_move / topmost / close demo")
vk_press("enter")

local x, y, w, h = window_rect(hwnd)
if x then
  window_move(hwnd, x + 50, y + 50)
end

window_set_topmost(hwnd, 1)
wait_ms(400)
window_set_topmost(hwnd, 0)

wait_ms(400)
window_close(hwnd)
