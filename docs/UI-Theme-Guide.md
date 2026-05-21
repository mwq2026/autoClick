# ImGui 蓝紫主题 UI 设计规范

> 基于 AutoClicker-Pro 项目提炼，适用于所有使用 Dear ImGui + D3D11 的 Windows 桌面工具。
> 按照本文档可快速复现完全一致的视觉风格。

---

## 目录

1. [设计理念](#1-设计理念)
2. [技术栈与初始化](#2-技术栈与初始化)
3. [颜色系统](#3-颜色系统)
4. [字体系统](#4-字体系统)
5. [布局参数](#5-布局参数)
6. [背景系统](#6-背景系统)
7. [组件库](#7-组件库)
8. [动效规范](#8-动效规范)
9. [窗口系统集成](#9-窗口系统集成)
10. [DPI 自适应](#10-dpi-自适应)
11. [完整初始化代码](#11-完整初始化代码)

---

## 1. 设计理念

**风格定位**：深色科技感 · 蓝紫渐变 · 玻璃拟态

| 关键词 | 说明 |
|--------|------|
| 深色基调 | 深海军蓝 `#231d3b` 作为窗口底色，避免纯黑的压抑感 |
| 蓝紫渐变 | 从深蓝 `#232048` → 紫色 `#442a80` → 品红 `#693594` 的三段渐变背景 |
| 玻璃拟态 | 卡片使用半透明背景 + 紫色边框发光，营造磨砂玻璃质感 |
| 霓虹强调 | 交互元素（按钮、选中态）使用高饱和度紫/青/粉作为强调色 |
| 动态粒子 | 背景漂浮 30 个半透明紫色粒子，增加科技氛围 |

---

## 2. 技术栈与初始化

### 依赖

```
Dear ImGui  v1.91.9
D3D11 + DXGI  (Windows SDK)
DWM API       (dwmapi.lib) — 深色标题栏 + 标题栏着色
```

### 窗口创建参数

```cpp
// 初始窗口尺寸（逻辑像素，会按 DPI 缩放）
const int kBaseW = 980;
const int kBaseH = 640;

// 窗口样式：标准可调整大小窗口
WS_OVERLAPPEDWINDOW

// 深色标题栏（Windows 10 1809+ / Windows 11）
BOOL useDark = TRUE;
DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDark, sizeof(useDark));

// 标题栏颜色匹配主题（Windows 11 专属）
COLORREF captionColor = RGB(42, 36, 78);  // 中等蓝紫色
DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &captionColor, sizeof(captionColor));
```

### D3D11 SwapChain 参数

```cpp
sd.BufferCount  = 2;
sd.Format       = DXGI_FORMAT_R8G8B8A8_UNORM;
sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;   // 兼容性最好
sd.SampleDesc   = { 1, 0 };                    // 无 MSAA（ImGui 自带抗锯齿）
sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

// 帧率控制：Present(1, 0) — 垂直同步 60fps
g_pSwapChain->Present(1, 0);
```

### 清屏颜色

```cpp
// 与 ImGuiCol_WindowBg 完全一致，避免边缘闪烁
const float clear_color[4] = { 0.13f, 0.11f, 0.23f, 1.00f };
```

---

## 3. 颜色系统

### 3.1 调色板总览

| 角色 | 名称 | RGBA (0-1) | HEX 近似 | 用途 |
|------|------|-----------|----------|------|
| 主背景 | Deep Navy | `(0.13, 0.11, 0.23, 1.0)` | `#211d3b` | 窗口底色、清屏色 |
| 次背景 | Dark Indigo | `(0.16, 0.14, 0.28, 0.65)` | `#292347` | 子窗口、菜单栏 |
| 卡片背景 | Glass Purple | `(0.18, 0.15, 0.30, 0.60)` | `#2e264d` | 玻璃卡片 |
| 输入框 | Frame Dark | `(0.18, 0.15, 0.34, 0.75)` | `#2e2657` | 输入框背景 |
| 弹窗背景 | Popup Dark | `(0.18, 0.15, 0.32, 0.96)` | `#2e2652` | 弹出菜单 |
| 主文字 | Lavender White | `(0.92, 0.90, 0.98, 1.0)` | `#eae6fa` | 正文文字 |
| 次文字 | Muted Purple | `(0.52, 0.48, 0.65, 1.0)` | `#857aa6` | 禁用/提示文字 |
| 卡片标题 | Soft Lavender | `(0.78, 0.75, 0.95, 1.0)` | `#c7bff2` | 卡片标题文字 |
| 边框 | Purple Border | `(0.40, 0.34, 0.62, 0.45)` | `#66579e` | 通用边框 |
| 卡片边框 | Glow Border | `(0.55, 0.45, 0.85, 0.40)` | `#8c73d9` | 玻璃卡片边框 |
| 强调色 | Vivid Purple | `(0.58, 0.45, 1.00, 1.0)` | `#9473ff` | 勾选框、滑块 |
| 按钮默认 | Button Base | `(0.26, 0.22, 0.48, 0.85)` | `#42387a` | 普通按钮 |
| 按钮悬停 | Button Hover | `(0.36, 0.30, 0.62, 0.92)` | `#5c4d9e` | 按钮悬停 |
| 按钮激活 | Button Active | `(0.46, 0.38, 0.76, 1.0)` | `#7561c2` | 按钮按下 |

### 3.2 背景渐变三色

```cpp
// 背景从上到下三段渐变
const ImU32 colTop = IM_COL32( 35,  32,  72, 255);  // #232048 深蓝
const ImU32 colMid = IM_COL32( 68,  42, 128, 255);  // #442a80 紫色
const ImU32 colBot = IM_COL32(105,  55, 148, 255);  // #693594 品红紫
```

### 3.3 GlowButton 预设色对

| 按钮语义 | 左色 (IM_COL32) | 右色 (IM_COL32) |
|----------|----------------|----------------|
| 危险/停止 | `(200, 50, 80, 255)` 红 | `(220, 80, 60, 255)` 橙红 |
| 成功/开始 | `(40, 160, 80, 255)` 绿 | `(30, 200, 120, 255)` 青绿 |
| 信息/保存 | `(60, 120, 200, 255)` 蓝 | `(40, 100, 220, 255)` 深蓝 |
| 警告/暂停 | `(180, 140, 40, 255)` 金 | `(200, 160, 60, 255)` 亮金 |
| 中性/导出 | `(100, 80, 180, 255)` 紫 | `(120, 100, 200, 255)` 亮紫 |

### 3.4 完整 ImGui 颜色表

```cpp
ImVec4* colors = ImGui::GetStyle().Colors;

// ── 文字 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.90f, 0.98f, 1.00f);
colors[ImGuiCol_TextDisabled]          = ImVec4(0.52f, 0.48f, 0.65f, 1.00f);

// ── 窗口 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.11f, 0.23f, 1.00f);
colors[ImGuiCol_ChildBg]               = ImVec4(0.16f, 0.14f, 0.28f, 0.65f);
colors[ImGuiCol_PopupBg]               = ImVec4(0.18f, 0.15f, 0.32f, 0.96f);

// ── 边框 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_Border]                = ImVec4(0.40f, 0.34f, 0.62f, 0.45f);
colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

// ── 输入框 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.15f, 0.34f, 0.75f);
colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.25f, 0.21f, 0.44f, 0.85f);
colors[ImGuiCol_FrameBgActive]         = ImVec4(0.32f, 0.27f, 0.55f, 0.95f);

// ── 标题栏 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_TitleBg]               = ImVec4(0.13f, 0.11f, 0.23f, 1.00f);
colors[ImGuiCol_TitleBgActive]         = ImVec4(0.18f, 0.15f, 0.34f, 1.00f);
colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.13f, 0.11f, 0.23f, 0.50f);
colors[ImGuiCol_MenuBarBg]             = ImVec4(0.16f, 0.14f, 0.28f, 1.00f);

// ── 滚动条 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.09f, 0.18f, 0.60f);
colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.40f, 0.34f, 0.65f, 0.70f);
colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.50f, 0.42f, 0.78f, 0.85f);
colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.58f, 0.48f, 0.90f, 1.00f);

// ── 控件 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_CheckMark]             = ImVec4(0.58f, 0.45f, 1.00f, 1.00f);
colors[ImGuiCol_SliderGrab]            = ImVec4(0.52f, 0.40f, 0.92f, 0.85f);
colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.62f, 0.50f, 1.00f, 1.00f);
colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.22f, 0.48f, 0.85f);
colors[ImGuiCol_ButtonHovered]         = ImVec4(0.36f, 0.30f, 0.62f, 0.92f);
colors[ImGuiCol_ButtonActive]          = ImVec4(0.46f, 0.38f, 0.76f, 1.00f);

// ── 列表头 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.25f, 0.52f, 0.55f);
colors[ImGuiCol_HeaderHovered]         = ImVec4(0.40f, 0.34f, 0.65f, 0.75f);
colors[ImGuiCol_HeaderActive]          = ImVec4(0.50f, 0.42f, 0.80f, 0.92f);

// ── 分隔线 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_Separator]             = ImVec4(0.35f, 0.30f, 0.55f, 0.45f);
colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.52f, 0.42f, 0.82f, 0.65f);
colors[ImGuiCol_SeparatorActive]       = ImVec4(0.62f, 0.50f, 0.95f, 1.00f);

// ── 调整手柄 ──────────────────────────────────────────────────────────────
colors[ImGuiCol_ResizeGrip]            = ImVec4(0.44f, 0.36f, 0.75f, 0.25f);
colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.54f, 0.44f, 0.88f, 0.55f);
colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.64f, 0.52f, 0.98f, 0.88f);

// ── 标签页 ────────────────────────────────────────────────────────────────
colors[ImGuiCol_Tab]                   = ImVec4(0.22f, 0.18f, 0.40f, 0.85f);
colors[ImGuiCol_TabHovered]            = ImVec4(0.42f, 0.35f, 0.70f, 0.85f);
colors[ImGuiCol_TabSelected]           = ImVec4(0.50f, 0.40f, 0.82f, 1.00f);
colors[ImGuiCol_TabDimmed]             = ImVec4(0.16f, 0.14f, 0.30f, 0.92f);
colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.32f, 0.27f, 0.55f, 1.00f);

// ── 图表 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_PlotLines]             = ImVec4(0.58f, 0.52f, 0.85f, 1.00f);
colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.92f, 0.45f, 0.55f, 1.00f);
colors[ImGuiCol_PlotHistogram]         = ImVec4(0.55f, 0.42f, 0.92f, 0.85f);
colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.65f, 0.52f, 1.00f, 1.00f);

// ── 表格 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.20f, 0.17f, 0.36f, 1.00f);
colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.35f, 0.30f, 0.55f, 0.65f);
colors[ImGuiCol_TableBorderLight]      = ImVec4(0.30f, 0.25f, 0.48f, 0.45f);
colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.18f, 0.15f, 0.32f, 0.22f);

// ── 其他 ──────────────────────────────────────────────────────────────────
colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.50f, 0.40f, 0.85f, 0.38f);
colors[ImGuiCol_DragDropTarget]        = ImVec4(0.58f, 0.45f, 1.00f, 0.92f);
colors[ImGuiCol_NavCursor]             = ImVec4(0.58f, 0.45f, 1.00f, 0.75f);
colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.55f, 0.45f, 0.85f, 0.75f);
colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.12f, 0.10f, 0.22f, 0.45f);
colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.08f, 0.06f, 0.15f, 0.65f);
```

---

## 4. 字体系统

### 4.1 字体规格

| 参数 | 值 | 说明 |
|------|----|------|
| 基础字号 | `18.0px` | 96 DPI 下的逻辑像素，随 DPI 线性缩放 |
| 字体族 | 微软雅黑 UI (msyh.ttc index=1) | 优先级最高，依次 fallback |
| 字符集 | `GetGlyphRangesChineseSimplifiedCommon()` | 常用简体中文约 2500 字 |
| 纹理宽度 | `4096px` | 防止超出 D3D11 纹理尺寸限制 |
| 过采样 | H=2, V=2 | 平衡质量与性能 |
| 像素对齐 | `PixelSnapH = true` | 水平方向像素对齐，避免模糊 |
| 对比度增强 | `RasterizerMultiply = 1.2f` | 补偿 stb_truetype 渲染偏淡的问题 |

### 4.2 字体候选列表（按优先级）

```cpp
// 按顺序尝试，找到第一个存在的文件
const std::vector<std::wstring> candidates = {
    L"msyh.ttc",    // 微软雅黑（Win8+，推荐）
    L"msyh.ttf",    // 微软雅黑（旧版）
    L"msyhl.ttc",   // 微软雅黑 Light
    L"simhei.ttf",  // 黑体（XP 兼容）
    L"simsun.ttc",  // 宋体（最后备选）
    L"arialuni.ttf" // Arial Unicode（极端 fallback）
};
// 若全部不存在，使用 ImGui 内置默认字体 + FontGlobalScale 缩放
```

### 4.3 DPI 缩放规则

```cpp
// 字号随 DPI 线性缩放
float scaledFontSize = std::floor(18.0f * dpiScale);

// UiScale() — 在绘制代码中获取当前缩放比
static float UiScale() {
    const float fontSize = ImGui::GetFontSize();
    return fontSize > 0.0f ? (fontSize / 18.0f) : 1.0f;
}

// 所有像素值乘以 s = UiScale()，例如：
const float btnH = 36.0f * s;
const float gutter = 70.0f * s;
```

### 4.4 字体重建安全流程

```cpp
// DPI 变化时必须按此顺序操作，否则会崩溃
io.Fonts->Clear();
io.FontDefault = nullptr;
// ... 添加字体 ...
ImGui_ImplDX11_InvalidateDeviceObjects();
if (!io.Fonts->Build()) {
    // 构建失败（纹理过大）时回退到默认字体
    io.Fonts->Clear();
    io.Fonts->AddFontDefault();
    io.FontGlobalScale = dpiScale;
    io.Fonts->Build();
}
ImGui_ImplDX11_CreateDeviceObjects();
```

---

## 5. 布局参数

### 5.1 间距与圆角（基础值，96 DPI）

```cpp
ImGuiStyle& style = ImGui::GetStyle();

// 间距
style.WindowPadding     = ImVec2(14.0f, 12.0f);  // 窗口内边距
style.FramePadding      = ImVec2(10.0f,  6.0f);  // 控件内边距
style.ItemSpacing       = ImVec2(10.0f,  8.0f);  // 控件间距
style.ItemInnerSpacing  = ImVec2( 6.0f,  4.0f);  // 控件内部间距
style.IndentSpacing     = 20.0f;                  // 缩进宽度
style.ScrollbarSize     = 12.0f;                  // 滚动条宽度
style.GrabMinSize       = 10.0f;                  // 滑块最小尺寸

// 边框宽度
style.WindowBorderSize  = 0.0f;   // 主窗口无边框（背景渐变替代）
style.ChildBorderSize   = 1.0f;   // 子窗口/卡片有边框
style.PopupBorderSize   = 1.0f;   // 弹窗有边框
style.FrameBorderSize   = 0.0f;   // 输入框无边框（背景色区分）
style.TabBorderSize     = 0.0f;   // 标签页无边框

// 圆角半径
style.WindowRounding    = 0.0f;   // 主窗口直角（全屏感）
style.ChildRounding     = 10.0f;  // 卡片圆角（玻璃感关键）
style.FrameRounding     = 6.0f;   // 输入框圆角
style.PopupRounding     = 8.0f;   // 弹窗圆角
style.ScrollbarRounding = 8.0f;   // 滚动条圆角
style.GrabRounding      = 6.0f;   // 滑块圆角
style.TabRounding       = 6.0f;   // 标签页圆角
```

> **注意**：以上值是 96 DPI 的基础值。调用 `style.ScaleAllSizes(dpiScale)` 后所有尺寸自动缩放。

### 5.2 标准按钮高度

| 场景 | 高度 |
|------|------|
| 主操作按钮（开始/停止） | `36.0f * s` |
| 工具栏按钮 | `32.0f * s` |
| 紧凑按钮 | `ImGui::GetFrameHeight()` |
| 全宽按钮 | `ImVec2(-1, btnH)` |

### 5.3 多列布局

使用 `ImGui::BeginTable` 实现可拖拽调整宽度的多列布局：

```cpp
// 三列布局示例（比例可持久化）
float col1 = 0.30f, col2 = 0.35f, col3 = 1.0f - col1 - col2;
ImGui::BeginTable("##layout", 3,
    ImGuiTableFlags_Resizable |
    ImGuiTableFlags_BordersInnerV |
    ImGuiTableFlags_NoSavedSettings,
    ImVec2(0, availH));
ImGui::TableSetupColumn("列1", ImGuiTableColumnFlags_WidthStretch, col1);
ImGui::TableSetupColumn("列2", ImGuiTableColumnFlags_WidthStretch, col2);
ImGui::TableSetupColumn("列3", ImGuiTableColumnFlags_WidthStretch, col3);
```

---

## 6. 背景系统

### 6.1 三段渐变背景

```cpp
void DrawBackground(ImDrawList* dl, ImVec2 tl, ImVec2 br, float workH) {
    const ImU32 colTop = IM_COL32( 35,  32,  72, 255);  // 深蓝
    const ImU32 colMid = IM_COL32( 68,  42, 128, 255);  // 紫色
    const ImU32 colBot = IM_COL32(105,  55, 148, 255);  // 品红紫

    const float midY = tl.y + workH * 0.5f;
    // 上半段：深蓝 → 紫色
    dl->AddRectFilledMultiColor(tl, ImVec2(br.x, midY), colTop, colTop, colMid, colMid);
    // 下半段：紫色 → 品红紫
    dl->AddRectFilledMultiColor(ImVec2(tl.x, midY), br, colMid, colMid, colBot, colBot);
}
// 调用：ImGui::GetBackgroundDrawList() 确保在所有控件之后
```

### 6.2 光晕扫光效果

每隔约 25 秒，一道半透明白色斜光从左向右扫过：

```cpp
const float streakPhase = fmodf(animTime * 0.08f, 2.0f);  // 周期 ~25s
if (streakPhase < 1.0f) {
    const float t = streakPhase;
    const float sx = tl.x + workW * (t * 1.5f - 0.25f);
    const float w  = workW * 0.15f;
    dl->AddRectFilledMultiColor(
        ImVec2(sx, tl.y), ImVec2(sx + w, br.y),
        IM_COL32(255,255,255, 0), IM_COL32(255,255,255, 8),  // 左透明→右微亮
        IM_COL32(255,255,255, 0), IM_COL32(255,255,255, 0)); // 右侧再透明
}
```

### 6.3 漂浮粒子系统

```cpp
struct Particle { float x, y, vx, vy, radius, alpha, phase; };

// 初始化（30 个粒子）
particles.resize(30);
for (auto& p : particles) {
    p.x      = rand() % (int)workW;
    p.y      = rand() % (int)workH;
    p.vx     = (rand() % 100 / 100.0f - 0.5f) * 0.3f;   // 水平漂移
    p.vy     = (rand() % 100 / 100.0f - 0.5f) * 0.2f - 0.1f; // 轻微上浮
    p.radius = 1.0f + (rand() % 30) / 10.0f;              // 1~4px
    p.alpha  = 0.2f + (rand() % 60) / 100.0f;             // 20%~80%
    p.phase  = (rand() % 628) / 100.0f;                   // 闪烁相位
}

// 每帧更新与绘制
for (auto& p : particles) {
    p.x += p.vx; p.y += p.vy;
    // 环绕边界
    if (p.x < 0) p.x += workW; if (p.x > workW) p.x -= workW;
    if (p.y < 0) p.y += workH; if (p.y > workH) p.y -= workH;

    const float flicker = 0.6f + 0.4f * sinf(animTime * 1.5f + p.phase);
    const int alpha = (int)(p.alpha * flicker * 255.0f);
    dl->AddCircleFilled(
        ImVec2(tl.x + p.x, tl.y + p.y),
        p.radius * uiScale,
        IM_COL32(180, 160, 255, alpha), 8);  // 淡紫色，8段近似圆
}
```

---

## 7. 组件库

### 7.1 玻璃卡片 (GlassCard)

卡片是界面的核心容器，半透明背景 + 紫色边框 + 顶部内发光。

```cpp
// ── 不可滚动卡片（高度自适应或固定）──────────────────────────────────────
void BeginGlassCard(const char* id, const char* title, ImVec2 size) {
    const float s = UiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.15f, 0.30f, 0.60f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.55f, 0.45f, 0.85f, 0.40f));

    ImGuiChildFlags cflags = ImGuiChildFlags_Borders;
    if (size.y == 0.0f) cflags |= ImGuiChildFlags_AutoResizeY;
    ImGui::BeginChild(id, size, cflags,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // 顶部内发光条（蓝紫渐变，高度 3px）
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(
        wp, ImVec2(wp.x + ws.x, wp.y + 3.0f * s),
        IM_COL32(140,120,255,40), IM_COL32(200,100,255,40),
        IM_COL32(200,100,255, 0), IM_COL32(140,120,255, 0));

    if (title && title[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.75f, 0.95f, 1.0f));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

void EndGlassCard() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

// ── 可滚动卡片（固定高度，内容超出时显示滚动条）──────────────────────────
void BeginGlassScrollCard(const char* id, const char* title, ImVec2 size) {
    // 与 BeginGlassCard 相同，但 BeginChild 不传 NoScrollbar 标志
    // ...（同上，去掉 ImGuiWindowFlags_NoScrollbar）
}
```

**使用示例**：
```cpp
BeginGlassCard("##my_card", "设置", ImVec2(0, 0));  // 宽度填满，高度自适应
{
    ImGui::Text("内容...");
}
EndGlassCard();
```

### 7.2 渐变发光按钮 (GlowButton)

```cpp
// label   : 按钮文字（支持 ## 后缀作为 ID）
// sizeArg : ImVec2(-1, h) 全宽，ImVec2(0, 0) 自适应
// colLeft/colRight : 左右渐变色（IM_COL32 格式）
bool GlowButton(const char* label, ImVec2 sizeArg,
                ImU32 colLeft, ImU32 colRight, float rounding = 0.0f)
{
    const float s = UiScale();
    const float r = rounding > 0.0f ? rounding : 6.0f * s;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 size(
        sizeArg.x < 0 ? avail.x : (sizeArg.x == 0 ? ImGui::CalcTextSize(label).x + 20.0f*s : sizeArg.x),
        sizeArg.y < 0 ? avail.y : (sizeArg.y == 0 ? ImGui::GetFrameHeight() : sizeArg.y));

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 br  = ImVec2(pos.x + size.x, pos.y + size.y);
    ImGui::InvisibleButton(label, size);

    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 悬停时外发光
    if (hovered) {
        ImU32 glow = (ColorWithAlpha(colLeft, 0.25f));
        dl->AddRectFilled(
            ImVec2(pos.x-3*s, pos.y-3*s), ImVec2(br.x+3*s, br.y+3*s),
            glow, r + 3*s);
    }

    // 按钮体：左右渐变，按下时变暗
    float darken = active ? 0.7f : (hovered ? 0.85f : 1.0f);
    ImU32 cL = LerpColor(IM_COL32(0,0,0,255), colLeft,  darken);
    ImU32 cR = LerpColor(IM_COL32(0,0,0,255), colRight, darken);
    dl->AddRectFilledMultiColor(pos, br, cL, cR, cR, cL);

    // 白色边框（悬停时更亮）
    dl->AddRect(pos, br, IM_COL32(255,255,255, hovered ? 80 : 40), r, 0, 1.5f*s);

    // 居中文字（白色）
    const char* end = strstr(label, "##");
    ImVec2 tsz = ImGui::CalcTextSize(label, end);
    dl->AddText(
        ImVec2(pos.x + (size.x-tsz.x)*0.5f, pos.y + (size.y-tsz.y)*0.5f),
        IM_COL32(255,255,255,240), label, end);

    return clicked;
}
```

### 7.3 工具栏

```cpp
// 工具栏：深色半透明条，圆角，位于内容区顶部
const float toolbarH = 44.0f * s;
ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.06f, 0.18f, 0.60f));
ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * s);
ImGui::BeginChild("##toolbar", ImVec2(-1, toolbarH), ImGuiChildFlags_None,
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
// ... 工具栏内容 ...
ImGui::EndChild();
ImGui::PopStyleVar();
ImGui::PopStyleColor();
```

### 7.4 状态栏

```cpp
// 状态栏：固定在底部，高度 28px，深色背景
const float statusH = 28.0f * s;
ImGui::SetCursorPosY(ImGui::GetWindowHeight() - statusH);
ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.06f, 0.18f, 0.85f));
ImGui::BeginChild("##statusbar", ImVec2(-1, statusH), ImGuiChildFlags_None,
    ImGuiWindowFlags_NoScrollbar);

// 状态文字颜色规范
// 信息：ImVec4(0.70f, 0.68f, 0.90f, 1.0f)  淡紫
// 成功：ImVec4(0.40f, 0.90f, 0.55f, 1.0f)  绿色
// 警告：ImVec4(1.00f, 0.80f, 0.30f, 1.0f)  金色
// 错误：ImVec4(1.00f, 0.45f, 0.45f, 1.0f)  红色

ImGui::EndChild();
ImGui::PopStyleColor();
```

### 7.5 标签页导航

```cpp
// 标签页样式已在全局主题中设置，直接使用即可
// 选中态：ImVec4(0.50, 0.40, 0.82, 1.0) 亮紫
// 未选中：ImVec4(0.22, 0.18, 0.40, 0.85) 暗紫

ImGui::BeginTabBar("##tabs");
if (ImGui::BeginTabItem("录制回放")) { /* ... */ ImGui::EndTabItem(); }
if (ImGui::BeginTabItem("Lua 脚本")) { /* ... */ ImGui::EndTabItem(); }
ImGui::EndTabBar();
```

### 7.6 彩色文字标签

```cpp
// 常用彩色文字
ImGui::TextColored(ImVec4(0.55f, 0.50f, 0.75f, 0.8f), "次要信息");
ImGui::TextColored(ImVec4(0.78f, 0.75f, 0.95f, 1.0f), "卡片标题");
ImGui::TextColored(ImVec4(0.45f, 0.40f, 0.65f, 0.8f), "序号/编号");
ImGui::TextColored(ImVec4(0.82f, 0.80f, 0.92f, 1.0f), "列表内容");
ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.0f), "错误信息");
ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f), "成功信息");
ImGui::TextColored(ImVec4(1.00f, 0.80f, 0.30f, 1.0f), "警告信息");
```

### 7.7 进度条

```cpp
// 自定义进度条（渐变色，带光点）
const float progress = replayer.Progress01();
ImVec2 barPos = ImGui::GetCursorScreenPos();
const float barW = ImGui::GetContentRegionAvail().x;
const float barH = 6.0f * s;
ImDrawList* dl = ImGui::GetWindowDrawList();

// 背景槽
dl->AddRectFilled(barPos, ImVec2(barPos.x+barW, barPos.y+barH),
    IM_COL32(40,30,80,150), 3.0f*s);
// 填充条（青色）
dl->AddRectFilled(barPos, ImVec2(barPos.x+barW*progress, barPos.y+barH),
    IM_COL32(100,200,255,220), 3.0f*s);
// 端点光点
if (progress > 0.01f)
    dl->AddCircleFilled(ImVec2(barPos.x+barW*progress, barPos.y+barH*0.5f),
        4.0f*s, IM_COL32(100,200,255,150));
ImGui::Dummy(ImVec2(0, barH + 6.0f*s));
```

---

## 8. 动效规范

### 8.1 动画时钟

```cpp
// 在每帧 DrawBackground() 末尾累加
float animTime = 0.0f;
animTime += ImGui::GetIO().DeltaTime;  // 单位：秒，从启动开始累计
```

### 8.2 呼吸动画

用于图标、发光效果的周期性脉动：

```cpp
// 周期约 2.86s，振幅 0.4，中心 0.6
const float breathe = 0.6f + 0.4f * sinf(animTime * 2.2f);

// 用法示例：呼吸发光圆
dl->AddCircleFilled(center, radius * 1.1f,
    IM_COL32(100, 80, 220, (int)(30 * breathe)), 48);
```

### 8.3 点击涟漪

```cpp
// 周期 1.5s，前 60% 时间内扩散
const float clickPeriod = 1.5f;
const float clickPhase  = fmodf(animTime, clickPeriod) / clickPeriod;

for (int i = 0; i < 2; ++i) {
    float phase = clickPhase - i * 0.18f;
    if (phase < 0.0f) phase += 1.0f;
    if (phase < 0.6f) {
        const float t  = phase / 0.6f;
        const float rr = radius * (0.35f + t * 0.75f);  // 从 35% 扩散到 110%
        const float a  = (1.0f - t * t) * 0.5f;         // 二次衰减
        dl->AddCircle(center, rr,
            IM_COL32(160, 200, 255, (int)(a * 255)), 48, 1.5f * s);
    }
}
```

### 8.4 微浮动（Bob）

用于图标内的箭头/指针，增加生动感：

```cpp
// X 轴：正弦，周期 ~3.5s，幅度 2%
const float bobX = sinf(animTime * 1.8f) * radius * 0.02f;
// Y 轴：余弦，周期 ~2.7s，幅度 2.5%
const float bobY = cosf(animTime * 2.3f) * radius * 0.025f;
```

### 8.5 颜色脉动

用于箭头/指针颜色的周期变化：

```cpp
// 在青色和白色之间脉动
const float cp = 0.5f + 0.5f * sinf(animTime * 1.5f);
const ImU32 arrowCol = IM_COL32(
    (int)(220 + 35 * cp),   // R: 220~255
    (int)(240 + 15 * cp),   // G: 240~255
    255, 255);               // B: 固定 255
```

### 8.6 录制状态指示灯

```cpp
// 红色圆点，4Hz 脉动
const float pulse = 0.7f + 0.3f * sinf(animTime * 4.0f);
ImVec2 dotPos = ImGui::GetCursorScreenPos();
dl->AddCircleFilled(
    ImVec2(dotPos.x + 8.0f*s, dotPos.y + 8.0f*s),
    5.0f * s,
    IM_COL32(255, 60, 60, (int)(pulse * 255)));
ImGui::Dummy(ImVec2(0, 4.0f * s));
```

### 8.7 任务栏图标动画

每 100ms 重绘一次 32×32 的 HICON，包含：
- 深蓝紫背景圆
- 青色涟漪环（与 UI 同步）
- 微浮动的白色箭头
- 呼吸发光边框

```cpp
// 更新频率控制
if (animTime - lastIconUpdateTime < 0.10f) return;
lastIconUpdateTime = animTime;
// ... 软件光栅化绘制 32x32 像素 ...
HICON newIcon = CreateIcon(hInstance, 32, 32, 1, 32, nullptr, (BYTE*)pixels.data());
SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)newIcon);
SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)newIcon);
if (oldIcon) DestroyIcon(oldIcon);
```

---

## 9. 窗口系统集成

### 9.1 深色标题栏

```cpp
// Windows 10 1809+ 深色模式标题栏
BOOL useDark = TRUE;
DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,
    &useDark, sizeof(useDark));

// Windows 11 标题栏颜色（与主题匹配）
COLORREF captionColor = RGB(42, 36, 78);  // #2a244e 中等蓝紫
DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/,
    &captionColor, sizeof(captionColor));
```

### 9.2 DPI 感知声明

```cpp
// 在 wWinMain 最开始调用，按优先级尝试三种方式
void EnableBestEffortDpiAwareness() {
    // 方式1：Per-Monitor V2（Win10 1703+，最佳）
    // SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    // 方式2：Per-Monitor（Win8.1+）
    // SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)
    // 方式3：System DPI（XP+，最低）
    // SetProcessDPIAware()
}
```

### 9.3 窗口几何持久化

```cpp
// 保存（在 DestroyWindow 之前调用）
WINDOWPLACEMENT wp{}; wp.length = sizeof(wp);
GetWindowPlacement(hwnd, &wp);
// 写入 config.ini: [Window] X/Y/W/H/Maximized

// 恢复（在 ShowWindow 之后调用）
// 读取 config.ini，调用 SetWindowPos + ShowWindow(SW_MAXIMIZE)
```

### 9.4 全局热键

```cpp
RegisterHotKey(hwnd, 1, MOD_CONTROL, VK_F12);  // 停止
RegisterHotKey(hwnd, 2, MOD_CONTROL, VK_F10);  // 开始/继续
RegisterHotKey(hwnd, 3, MOD_CONTROL, VK_F11);  // 暂停
// WM_HOTKEY 消息中处理
```

---

## 10. DPI 自适应

### 10.1 核心原则

所有像素值都乘以 `UiScale()`，不要硬编码像素：

```cpp
// ✅ 正确
const float btnH = 36.0f * UiScale();
const float pad  = 10.0f * UiScale();

// ❌ 错误
const float btnH = 36.0f;
```

### 10.2 DPI 变化响应

```cpp
// WndProc 中处理 WM_DPICHANGED
case WM_DPICHANGED: {
    const UINT dpi = LOWORD(wParam);
    const float scale = (float)dpi / 96.0f;

    // 1. 移动窗口到建议位置
    auto* rc = reinterpret_cast<RECT*>(lParam);
    SetWindowPos(hwnd, nullptr, rc->left, rc->top,
        rc->right-rc->left, rc->bottom-rc->top,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // 2. 重建字体和样式
    ApplyDpiToImGui(scale);
    return 0;
}
```

### 10.3 样式缩放

```cpp
// g_baseStyle 保存 96 DPI 的基础样式
// 每次 DPI 变化时从基础样式派生
ImGuiStyle style = g_baseStyle;
style.ScaleAllSizes(dpiScale);  // 自动缩放所有尺寸参数
ImGui::GetStyle() = style;
```

---

## 11. 完整初始化代码

将以下代码复制到新项目的 `main.cpp` 中，即可获得完全一致的主题效果。

```cpp
// ─── 常量 ────────────────────────────────────────────────────────────────
static constexpr float kBaseUiFontPx = 18.0f;
static float           g_uiDpiScale  = 1.0f;
static ImGuiStyle      g_baseStyle{};
static bool            g_imguiBackendReady = false;

// ─── 字体加载 ─────────────────────────────────────────────────────────────
static bool SetupChineseFonts(ImGuiIO& io, float sizePx) {
    // 候选字体路径（Windows Fonts 目录）
    wchar_t winDir[MAX_PATH]{};
    GetWindowsDirectoryW(winDir, MAX_PATH);
    std::wstring fontsDir = std::wstring(winDir) + L"\\Fonts\\";

    const wchar_t* candidates[] = {
        L"msyh.ttc", L"msyh.ttf", L"msyhl.ttc",
        L"simhei.ttf", L"simsun.ttc", L"arialuni.ttf"
    };
    std::wstring chosen;
    for (auto* name : candidates) {
        std::wstring path = fontsDir + name;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            chosen = path; break;
        }
    }
    if (chosen.empty()) return false;

    // Wide → UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, chosen.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string pathUtf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, chosen.c_str(), -1, pathUtf8.data(), len, nullptr, nullptr);

    ImFontConfig cfg{};
    cfg.OversampleH = 2; cfg.OversampleV = 2;
    cfg.PixelSnapH  = true;
    cfg.RasterizerMultiply = 1.2f;
    // msyh.ttc index=1 是 "Microsoft YaHei UI"
    if (chosen.find(L"msyh.ttc") != std::wstring::npos ||
        chosen.find(L"msyhl.ttc") != std::wstring::npos)
        cfg.FontNo = 1;

    io.Fonts->TexDesiredWidth = 4096;
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        pathUtf8.c_str(), std::floor(sizePx), &cfg,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    return io.FontDefault != nullptr;
}

// ─── DPI 变化时重建字体和样式 ─────────────────────────────────────────────
static void ApplyDpiToImGui(float dpiScale) {
    if (!ImGui::GetCurrentContext()) { g_uiDpiScale = dpiScale; return; }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;
    io.FontGlobalScale = 1.0f;

    if (!SetupChineseFonts(io, kBaseUiFontPx * dpiScale)) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = dpiScale;
    }

    ImGuiStyle style = g_baseStyle;
    style.ScaleAllSizes(dpiScale);
    ImGui::GetStyle() = style;

    if (g_imguiBackendReady) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        if (!io.Fonts->Build()) {
            io.Fonts->Clear();
            io.Fonts->AddFontDefault();
            io.FontGlobalScale = dpiScale;
            io.Fonts->Build();
        }
        ImGui_ImplDX11_CreateDeviceObjects();
    }
    g_uiDpiScale = dpiScale;
}

// ─── 主题初始化（在 ImGui::CreateContext() 之后调用）─────────────────────
static void InitTheme(HWND hwnd) {
    // 深色标题栏
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    COLORREF cap = RGB(42, 36, 78);
    DwmSetWindowAttribute(hwnd, 35, &cap, sizeof(cap));

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    // 文字
    c[ImGuiCol_Text]         = ImVec4(0.92f,0.90f,0.98f,1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.52f,0.48f,0.65f,1.00f);
    // 窗口
    c[ImGuiCol_WindowBg]  = ImVec4(0.13f,0.11f,0.23f,1.00f);
    c[ImGuiCol_ChildBg]   = ImVec4(0.16f,0.14f,0.28f,0.65f);
    c[ImGuiCol_PopupBg]   = ImVec4(0.18f,0.15f,0.32f,0.96f);
    // 边框
    c[ImGuiCol_Border]       = ImVec4(0.40f,0.34f,0.62f,0.45f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f,0.00f,0.00f,0.00f);
    // 输入框
    c[ImGuiCol_FrameBg]        = ImVec4(0.18f,0.15f,0.34f,0.75f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.25f,0.21f,0.44f,0.85f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.32f,0.27f,0.55f,0.95f);
    // 标题栏
    c[ImGuiCol_TitleBg]          = ImVec4(0.13f,0.11f,0.23f,1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.18f,0.15f,0.34f,1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.13f,0.11f,0.23f,0.50f);
    c[ImGuiCol_MenuBarBg]        = ImVec4(0.16f,0.14f,0.28f,1.00f);
    // 滚动条
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f,0.09f,0.18f,0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.40f,0.34f,0.65f,0.70f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f,0.42f,0.78f,0.85f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.58f,0.48f,0.90f,1.00f);
    // 控件
    c[ImGuiCol_CheckMark]       = ImVec4(0.58f,0.45f,1.00f,1.00f);
    c[ImGuiCol_SliderGrab]      = ImVec4(0.52f,0.40f,0.92f,0.85f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.62f,0.50f,1.00f,1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.26f,0.22f,0.48f,0.85f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.36f,0.30f,0.62f,0.92f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.46f,0.38f,0.76f,1.00f);
    // 列表头
    c[ImGuiCol_Header]        = ImVec4(0.30f,0.25f,0.52f,0.55f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.40f,0.34f,0.65f,0.75f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.50f,0.42f,0.80f,0.92f);
    // 分隔线
    c[ImGuiCol_Separator]        = ImVec4(0.35f,0.30f,0.55f,0.45f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.52f,0.42f,0.82f,0.65f);
    c[ImGuiCol_SeparatorActive]  = ImVec4(0.62f,0.50f,0.95f,1.00f);
    // 调整手柄
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.44f,0.36f,0.75f,0.25f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.54f,0.44f,0.88f,0.55f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.64f,0.52f,0.98f,0.88f);
    // 标签页
    c[ImGuiCol_Tab]              = ImVec4(0.22f,0.18f,0.40f,0.85f);
    c[ImGuiCol_TabHovered]       = ImVec4(0.42f,0.35f,0.70f,0.85f);
    c[ImGuiCol_TabSelected]      = ImVec4(0.50f,0.40f,0.82f,1.00f);
    c[ImGuiCol_TabDimmed]        = ImVec4(0.16f,0.14f,0.30f,0.92f);
    c[ImGuiCol_TabDimmedSelected]= ImVec4(0.32f,0.27f,0.55f,1.00f);
    // 图表
    c[ImGuiCol_PlotLines]            = ImVec4(0.58f,0.52f,0.85f,1.00f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(0.92f,0.45f,0.55f,1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.55f,0.42f,0.92f,0.85f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.65f,0.52f,1.00f,1.00f);
    // 表格
    c[ImGuiCol_TableHeaderBg]    = ImVec4(0.20f,0.17f,0.36f,1.00f);
    c[ImGuiCol_TableBorderStrong]= ImVec4(0.35f,0.30f,0.55f,0.65f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.30f,0.25f,0.48f,0.45f);
    c[ImGuiCol_TableRowBg]       = ImVec4(0.00f,0.00f,0.00f,0.00f);
    c[ImGuiCol_TableRowBgAlt]    = ImVec4(0.18f,0.15f,0.32f,0.22f);
    // 其他
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.50f,0.40f,0.85f,0.38f);
    c[ImGuiCol_DragDropTarget]        = ImVec4(0.58f,0.45f,1.00f,0.92f);
    c[ImGuiCol_NavCursor]             = ImVec4(0.58f,0.45f,1.00f,0.75f);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.55f,0.45f,0.85f,0.75f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.12f,0.10f,0.22f,0.45f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.08f,0.06f,0.15f,0.65f);

    // 布局参数
    style.WindowPadding    = ImVec2(14,12); style.FramePadding     = ImVec2(10, 6);
    style.ItemSpacing      = ImVec2(10, 8); style.ItemInnerSpacing = ImVec2( 6, 4);
    style.IndentSpacing    = 20; style.ScrollbarSize = 12; style.GrabMinSize = 10;
    style.WindowBorderSize = 0;  style.ChildBorderSize = 1;
    style.PopupBorderSize  = 1;  style.FrameBorderSize = 0; style.TabBorderSize = 0;
    style.WindowRounding   = 0;  style.ChildRounding   = 10;
    style.FrameRounding    = 6;  style.PopupRounding   = 8;
    style.ScrollbarRounding= 8;  style.GrabRounding    = 6; style.TabRounding = 6;

    g_baseStyle = style;

    // 应用 DPI 缩放
    const UINT dpi = []() -> UINT {
        HDC dc = GetDC(nullptr);
        UINT d = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(nullptr, dc);
        return d ? d : 96;
    }();
    ApplyDpiToImGui((float)dpi / 96.0f);
}
```

---

## 附录 A：颜色速查表（HEX）

| 用途 | HEX | RGBA (0-1) |
|------|-----|-----------|
| 窗口底色 / 清屏色 | `#211d3b` | `(0.13, 0.11, 0.23, 1.0)` |
| 子窗口背景 | `#292347` | `(0.16, 0.14, 0.28, 0.65)` |
| 玻璃卡片背景 | `#2e264d` | `(0.18, 0.15, 0.30, 0.60)` |
| 输入框背景 | `#2e2657` | `(0.18, 0.15, 0.34, 0.75)` |
| 正文文字 | `#eae6fa` | `(0.92, 0.90, 0.98, 1.0)` |
| 次要文字 | `#857aa6` | `(0.52, 0.48, 0.65, 1.0)` |
| 卡片标题 | `#c7bff2` | `(0.78, 0.75, 0.95, 1.0)` |
| 通用边框 | `#66579e` | `(0.40, 0.34, 0.62, 0.45)` |
| 卡片发光边框 | `#8c73d9` | `(0.55, 0.45, 0.85, 0.40)` |
| 强调紫 | `#9473ff` | `(0.58, 0.45, 1.00, 1.0)` |
| 按钮默认 | `#42387a` | `(0.26, 0.22, 0.48, 0.85)` |
| 背景顶色 | `#232048` | `(0.14, 0.13, 0.28, 1.0)` |
| 背景中色 | `#442a80` | `(0.27, 0.16, 0.50, 1.0)` |
| 背景底色 | `#693594` | `(0.41, 0.21, 0.58, 1.0)` |
| 标题栏颜色 | `#2a244e` | `RGB(42, 36, 78)` |
| 粒子颜色 | `#b4a0ff` | `IM_COL32(180,160,255,α)` |
| 涟漪颜色 | `#a0c8ff` | `IM_COL32(160,200,255,α)` |

---

## 附录 B：快速移植检查清单

新项目接入本主题时，按顺序完成以下步骤：

- [ ] 引入 Dear ImGui v1.91.9，配置 D3D11 backend
- [ ] 链接 `dwmapi.lib`，调用深色标题栏 API
- [ ] 在 `wWinMain` 最开始调用 `EnableBestEffortDpiAwareness()`
- [ ] 复制 `SetupChineseFonts()` 和 `ApplyDpiToImGui()` 函数
- [ ] 在 `ImGui::CreateContext()` 之后调用 `InitTheme(hwnd)`
- [ ] 在 `ImGui_ImplDX11_Init()` 之后设置 `g_imguiBackendReady = true`
- [ ] 在 `WndProc` 中处理 `WM_DPICHANGED`，调用 `ApplyDpiToImGui()`
- [ ] 清屏颜色设为 `{ 0.13f, 0.11f, 0.23f, 1.00f }`
- [ ] 复制 `BeginGlassCard / EndGlassCard / GlowButton` 工具函数
- [ ] 复制 `DrawBackground()` 并在每帧 `OnFrame()` 开头调用
- [ ] 所有像素值使用 `UiScale()` 乘数，不硬编码

---

*文档版本：1.0 · 基于 AutoClicker-Pro 2026-05 版本提炼*
