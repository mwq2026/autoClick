set_speed(1.0)

local w, h = screen_size()
if not w then
  return
end

local cx, cy = cursor_pos()
if not cx then
  return
end

local r, g, b = pixel_get(cx, cy)
if not r then
  return
end

text("current pixel rgb=")
text(tostring(r) .. "," .. tostring(g) .. "," .. tostring(b))
vk_press("enter")
text("move cursor to a new point; waiting for color change at old point...")
vk_press("enter")

local ok = color_wait(cx, cy, r, g, b, 0, 8000, 50)
if ok then
  text("color matched again")
else
  text("timeout waiting color")
end
vk_press("enter")
