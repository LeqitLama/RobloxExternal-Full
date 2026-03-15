#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../features/settings.h"
#include <string>
#include <cmath>
#include <map>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// FontAwesome 6 Free Solid definitions
#define FONT_ICON_FILE_NAME_FAS "fa-solid-900.ttf"
#define ICON_FA_CROSSHAIRS "\uf05b"
#define ICON_FA_EYE "\uf06e"
#define ICON_FA_GLOBE "\uf0ac"
#define ICON_FA_COG "\uf013"
#define ICON_FA_USER "\uf007"
#define ICON_FA_USERS "\uf0c0"
#define ICON_FA_LIST "\uf03a"
#define ICON_FA_CODE "\uf121"
#define ICON_FA_TERMINAL "\uf120"
#define ICON_FA_PLAY "\uf04b"
#define ICON_FA_PAUSE "\uf04c"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Animation helpers
namespace Anim {
    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    inline ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
        return ImVec4(
            Lerp(a.x, b.x, t),
            Lerp(a.y, b.y, t),
            Lerp(a.z, b.z, t),
            Lerp(a.w, b.w, t)
        );
    }
}

// Updated Color Palette - Cleaner, more professional
namespace Colors {
    const ImVec4 Primary = ImVec4(0.42f, 0.36f, 0.91f, 1.0f);       // #6B5CE7 (Soft Indigo)
    const ImVec4 PrimaryDark = ImVec4(0.32f, 0.26f, 0.81f, 1.0f);
    
    const ImVec4 Background = ImVec4(0.09f, 0.09f, 0.11f, 0.98f);   // Deep clear dark
    const ImVec4 Panel = ImVec4(0.13f, 0.13f, 0.16f, 1.0f);         // Slightly lighter panel
    const ImVec4 Border = ImVec4(0.20f, 0.20f, 0.25f, 0.60f);       // Subtle borders
    
    const ImVec4 TextBright = ImVec4(0.98f, 0.98f, 1.00f, 1.0f);
    const ImVec4 TextDim = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
    
    inline ImU32 ToU32(const ImVec4& col) {
        return IM_COL32((int)(col.x * 255), (int)(col.y * 255), (int)(col.z * 255), (int)(col.w * 255));
    }
    
    inline ImU32 WithAlpha(const ImVec4& col, float alpha) {
        return IM_COL32((int)(col.x * 255), (int)(col.y * 255), (int)(col.z * 255), (int)(alpha * 255));
    }
}

Overlay::Overlay() {}

Overlay::~Overlay()
{
    Cleanup();
}

bool Overlay::Initialize()
{
    CreateOverlayWindow();
    InitializeDirectX();
    InitializeImGui();
    LoadMinecraftFont();
    return true;
}

void Overlay::CreateOverlayWindow()
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("FreeLamaOverlay");
    RegisterClassEx(&wc);
    
    window_handle = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        TEXT("FreeLamaOverlay"), TEXT("Free Lama External"),
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    SetLayeredWindowAttributes(window_handle, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(window_handle, SW_SHOW);
    UpdateWindow(window_handle);
}

void Overlay::InitializeDirectX()
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = GetSystemMetrics(SM_CXSCREEN);
    scd.BufferDesc.Height = GetSystemMetrics(SM_CYSCREEN);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = window_handle;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        NULL, 0, D3D11_SDK_VERSION,
        &scd, &swapchain, &device, &featureLevel, &context
    );

    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (dxgiDevice)
    {
        IDXGIAdapter* dxgiAdapter = nullptr;
        dxgiDevice->GetAdapter(&dxgiAdapter);
        if (dxgiAdapter)
        {
            IDXGIFactory* dxgiFactory = nullptr;
            dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
            if (dxgiFactory)
            {
                dxgiFactory->MakeWindowAssociation(window_handle, DXGI_MWA_NO_ALT_ENTER);
                dxgiFactory->Release();
            }
            dxgiAdapter->Release();
        }
        dxgiDevice->Release();
    }
    
    ID3D11Texture2D* back_buffer;
    swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    device->CreateRenderTargetView(back_buffer, NULL, &render_target_view);
    back_buffer->Release();
}

void Overlay::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    
    ImGuiStyle& style = ImGui::GetStyle();
    
    // High quality anti-aliasing
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    
    // Consistent corner rounding - all corners smooth
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    
    // Clean borders
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    
    // Padding & spacing
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 8.0f;
    style.GrabMinSize = 12.0f;
    
    // Full color theme
    ImVec4* colors = style.Colors;
    
    // Backgrounds
    colors[ImGuiCol_WindowBg] = Colors::Background;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.15f, 0.98f);
    
    // Text
    colors[ImGuiCol_Text] = Colors::TextBright;
    colors[ImGuiCol_TextDisabled] = Colors::TextDim;
    
    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 0.40f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Frame (inputs, sliders, combos)
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
    
    // Title bar (for popups)
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.13f, 0.75f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.13f, 0.5f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = Colors::Primary;
    colors[ImGuiCol_ScrollbarGrabActive] = Colors::Primary;
    
    // Slider
    colors[ImGuiCol_SliderGrab] = Colors::Primary;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.52f, 0.46f, 1.0f, 1.0f);
    
    // Check/Radio
    colors[ImGuiCol_CheckMark] = Colors::Primary;
    
    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(Colors::Primary.x * 0.6f, Colors::Primary.y * 0.6f, Colors::Primary.z * 0.6f, 1.0f);
    
    // Headers (combo, tree)
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.30f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(Colors::Primary.x * 0.5f, Colors::Primary.y * 0.5f, Colors::Primary.z * 0.5f, 1.0f);
    
    // Separator
    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = Colors::Primary;
    colors[ImGuiCol_SeparatorActive] = Colors::Primary;
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.26f, 0.32f, 1.0f);
    colors[ImGuiCol_ResizeGripHovered] = Colors::Primary;
    colors[ImGuiCol_ResizeGripActive] = Colors::Primary;
    
    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(Colors::Primary.x * 0.6f, Colors::Primary.y * 0.6f, Colors::Primary.z * 0.6f, 1.0f);
    colors[ImGuiCol_TabActive] = Colors::Primary;
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);

    ImGui_ImplWin32_Init(window_handle);
    ImGui_ImplDX11_Init(device, context);
}

void Overlay::LoadMinecraftFont()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // Build path relative to executable
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    std::string base_path(exe_path);
    size_t last_slash = base_path.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        base_path = base_path.substr(0, last_slash + 1);
    }
    
    // Load Monocraft font for ESP names/distance
    ImFontConfig font_config;
    font_config.PixelSnapH = true;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    
    std::string monocraft_path = base_path + "Monocraft.ttf";
    minecraft_font = io.Fonts->AddFontFromFileTTF(monocraft_path.c_str(), 14.0f, &font_config);
    
    if (!minecraft_font) {
        // Fallback to system font if Monocraft not found
        minecraft_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &font_config);
    }
    
    if (!minecraft_font) {
        minecraft_font = io.Fonts->AddFontDefault();
    }
    
    // Merge Icons (FontAwesome)
    std::string icon_path = base_path + FONT_ICON_FILE_NAME_FAS;
    static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 18.0f;
    icons_config.GlyphOffset = ImVec2(0, 2);
    icons_config.OversampleH = 1;
    icons_config.OversampleV = 1;
    
    io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 14.0f, &icons_config, icons_ranges);
    
    io.Fonts->Build();
}

void Overlay::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    context->OMSetRenderTargets(1, &render_target_view, NULL);
}

void Overlay::EndFrame()
{
    if (menu_open) DrawMenu();

    LONG cur_ex_style = GetWindowLong(window_handle, GWL_EXSTYLE);
    if (menu_open)
    {
        if ((cur_ex_style & WS_EX_TRANSPARENT))
            SetWindowLong(window_handle, GWL_EXSTYLE, cur_ex_style & ~WS_EX_TRANSPARENT);
    }
    else
    {
        if (!(cur_ex_style & WS_EX_TRANSPARENT))
            SetWindowLong(window_handle, GWL_EXSTYLE, cur_ex_style | WS_EX_TRANSPARENT);
    }
    
    ImGui::Render();
    
    static const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(render_target_view, clear_color);
    
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    // Present with VSync OFF for maximum FPS
    swapchain->Present(0, 0);
}

// Custom UI Components using public API only

static bool ToggleSwitch(const char* label, bool* v) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    
    // Strip ## suffix for display (like ImGui does)
    std::string display_label = label;
    size_t hash_pos = display_label.find("##");
    if (hash_pos != std::string::npos) {
        display_label = display_label.substr(0, hash_pos);
    }
    
    float height = 20.0f;
    float width = 38.0f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 label_size = ImGui::CalcTextSize(display_label.c_str(), NULL, true);
    
    // Invisible button for interaction (use full label with ## for unique ID)
    ImGui::InvisibleButton(label, ImVec2(width + 12 + label_size.x, height + 4));
    bool pressed = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    
    if (pressed) *v = !*v;
    
    // Animations
    static std::map<std::string, float> anim_map;
    static std::map<std::string, float> hover_map;
    float& anim = anim_map[label];
    float& hover_anim = hover_map[label];
    
    float target = *v ? 1.0f : 0.0f;
    anim = Anim::Lerp(anim, target, 0.18f);
    hover_anim = Anim::Lerp(hover_anim, hovered ? 1.0f : 0.0f, 0.2f);
    
    // Adjust Y for vertical centering
    float y_offset = 2.0f;
    ImVec2 p = ImVec2(cursor.x, cursor.y + y_offset);
    
    // Glow effect when enabled
    if (*v) {
        for (int i = 2; i > 0; i--) {
            draw->AddRectFilled(
                ImVec2(p.x - i, p.y - i),
                ImVec2(p.x + width + i, p.y + height + i),
                Colors::WithAlpha(Colors::Primary, 0.08f * (3 - i)),
                height * 0.5f + i
            );
        }
    }
    
    // Background pill - color transition
    float r = Anim::Lerp(35.0f, Colors::Primary.x * 255, anim);
    float g = Anim::Lerp(35.0f, Colors::Primary.y * 255, anim);
    float b = Anim::Lerp(40.0f, Colors::Primary.z * 255, anim);
    
    // Lighten on hover
    if (hover_anim > 0.01f && !*v) {
        r = Anim::Lerp(r, 50.0f, hover_anim);
        g = Anim::Lerp(g, 50.0f, hover_anim);
        b = Anim::Lerp(b, 58.0f, hover_anim);
    }
    
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32((int)r, (int)g, (int)b, 255), height * 0.5f);
    
    // Dot with smooth movement
    float dot_pad = 3.0f;
    float dot_size = height - dot_pad * 2;
    float dot_x = p.x + dot_pad + anim * (width - height);
    
    // Dot shadow
    draw->AddCircleFilled(ImVec2(dot_x + dot_size * 0.5f + 1, p.y + dot_pad + dot_size * 0.5f + 1), dot_size * 0.5f, IM_COL32(0, 0, 0, 40));
    // Dot
    draw->AddCircleFilled(ImVec2(dot_x + dot_size * 0.5f, p.y + dot_pad + dot_size * 0.5f), dot_size * 0.5f, IM_COL32(255, 255, 255, 255));
    
    // Label with hover effect (display clean label without ##)
    ImU32 text_col = hovered ? Colors::ToU32(Colors::TextBright) : Colors::WithAlpha(Colors::TextBright, 0.85f);
    draw->AddText(ImVec2(cursor.x + width + 12, cursor.y + (height + 4 - label_size.y) * 0.5f), text_col, display_label.c_str());

    return pressed;
}

static void SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::TextColored(Colors::Primary, label);
    ImGui::SameLine();
    
    // Line separator
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float line_y = cursor.y + ImGui::GetTextLineHeight() / 2.0f;
    
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(cursor.x + 10, line_y),
        ImVec2(cursor.x + avail.x, line_y),
        Colors::ToU32(Colors::Border),
        1.0f
    );
    ImGui::Spacing();
    ImGui::Spacing();
}

static void TabButton(const char* label, int index, int* active_tab, float width) {
    bool active = (*active_tab == index);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float height = 38.0f;
    
    // Invisible button for click detection
    ImGui::InvisibleButton(label, ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    
    if (clicked) *active_tab = index;
    
    // Background
    ImU32 bg_col = IM_COL32(0, 0, 0, 0);
    if (active) bg_col = Colors::WithAlpha(Colors::Primary, 0.12f);
    else if (hovered) bg_col = IM_COL32(255, 255, 255, 10);
    
    draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height), bg_col, 6.0f);
    
    // Active indicator bar on left
    if (active) {
        draw->AddRectFilled(
            ImVec2(cursor.x + 2, cursor.y + 8),
            ImVec2(cursor.x + 4, cursor.y + height - 8),
            Colors::ToU32(Colors::Primary),
            2.0f
        );
    }
    
    // Text - centered vertically, left-aligned with padding
    ImVec2 text_size = ImGui::CalcTextSize(label);
    float text_x = cursor.x + 20;
    float text_y = cursor.y + (height - text_size.y) * 0.5f;
    
    ImU32 text_col = active ? Colors::ToU32(Colors::TextBright) : Colors::ToU32(Colors::TextDim);
    draw->AddText(ImVec2(text_x, text_y), text_col, label);
}

// Animated slider - smooth fill bar animation
static bool AnimatedSlider(const char* label, float* v, float v_min, float v_max, const char* format) {
    static std::map<std::string, float> anim_fill;
    float& fill_t = anim_fill[label];
    
    // Calculate normalized position
    float t = (*v - v_min) / (v_max - v_min);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    
    // Smooth fill animation
    fill_t = Anim::Lerp(fill_t, t, 0.15f);
    
    // Style
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(22, 22, 28, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(28, 28, 36, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(32, 32, 42, 255));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Colors::ToU32(Colors::Primary));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, IM_COL32(130, 115, 255, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);
    
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::SliderFloat(label, v, v_min, v_max, format);
    
    // Draw animated fill bar behind
    ImVec2 frame_min = ImGui::GetItemRectMin();
    ImVec2 frame_max = ImGui::GetItemRectMax();
    float frame_w = frame_max.x - frame_min.x;
    float frame_h = frame_max.y - frame_min.y;
    
    float fill_w = fill_t * (frame_w - 8);
    if (fill_w > 2.0f) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(frame_min.x + 4, frame_min.y + frame_h * 0.38f),
            ImVec2(frame_min.x + 4 + fill_w, frame_max.y - frame_h * 0.38f),
            Colors::WithAlpha(Colors::Primary, 0.25f),
            3.0f
        );
    }
    
    // Hover glow
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(frame_min.x - 1, frame_min.y - 1),
            ImVec2(frame_max.x + 1, frame_max.y + 1),
            Colors::WithAlpha(Colors::Primary, ImGui::IsItemActive() ? 0.5f : 0.25f),
            7.0f, 0, 1.0f
        );
    }
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    return changed;
}

void Overlay::DrawMenu()
{
    static int active_tab = 0;
    
    ImVec2 windowSize = ImVec2(680, 460);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (ImGui::Begin("##MainUI", &menu_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        ImVec2 size = ImGui::GetWindowSize();
        ImVec2 pos = ImGui::GetWindowPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        float corner_radius = 10.0f;
        float sidebar_w = 180.0f;
        
        // Main background with proper rounded corners (all 4)
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
            IM_COL32(22, 22, 28, 252), corner_radius);
        
        // Sidebar background (left side with left corners rounded)
        draw->AddRectFilled(
            pos, ImVec2(pos.x + sidebar_w, pos.y + size.y),
            IM_COL32(16, 16, 20, 255),
            corner_radius, ImDrawFlags_RoundCornersLeft
        );
        
        // Sidebar right divider line
        draw->AddLine(
            ImVec2(pos.x + sidebar_w, pos.y + 10),
            ImVec2(pos.x + sidebar_w, pos.y + size.y - 10),
            IM_COL32(40, 40, 50, 200)
        );
        
        // Outer border glow (subtle)
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
            IM_COL32(60, 60, 80, 80), corner_radius, 0, 1.0f);
        
        // Logo / Title
        ImGui::SetCursorPos(ImVec2(24, 28));
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(Colors::Primary, "leqit.de");
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::SameLine();
        ImGui::SetCursorPosY(32);
        ImGui::TextColored(Colors::TextDim, "v2.0");
        
        // Navigation
        ImGui::SetCursorPos(ImVec2(10, 80));
        ImGui::BeginGroup();
            TabButton("\xef\x81\x8b Aimbot", 0, &active_tab, sidebar_w - 20);
            ImGui::Dummy(ImVec2(0, 4));
            TabButton("\xef\x81\xae Visuals", 1, &active_tab, sidebar_w - 20);
            ImGui::Dummy(ImVec2(0, 4));
            TabButton("\xef\x80\x93 Movement", 2, &active_tab, sidebar_w - 20);
            ImGui::Dummy(ImVec2(0, 4));
            TabButton("\xef\x80\x93 Settings", 3, &active_tab, sidebar_w - 20);
        ImGui::EndGroup();
        
        // User Info / Bottom sidebar
        ImGui::SetCursorPos(ImVec2(20, size.y - 45));
        ImGui::BeginGroup();
            ImGui::TextColored(Colors::TextDim, "Status:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "Active");
        ImGui::EndGroup();
        
        // Content Area
        ImGui::SetCursorPos(ImVec2(sidebar_w + 24, 24));
        ImGui::BeginChild("##Content", ImVec2(size.x - sidebar_w - 48, size.y - 48), false);
        
        if (active_tab == 0) // Aimbot
        {
            ImGui::Columns(2, "##AimCols", false);
            ImGui::SetColumnWidth(0, (size.x - sidebar_w - 48) * 0.5f);
            
            SectionHeader("AIMBOT");
            ToggleSwitch("Enable", &g_Settings.aimbot_enabled);
            ImGui::Spacing();
            ToggleSwitch("Team Check", &g_Settings.aimbot_team_check);
            ToggleSwitch("Prediction", &g_Settings.aimbot_prediction);
            
            ImGui::Spacing();
            SectionHeader("LEGIT MODE");
            ToggleSwitch("Enable##legit", &g_Settings.aimbot_legit_mode);
            if (g_Settings.aimbot_legit_mode) g_Settings.rage_enabled = false; // Exclusive
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Reaction Time");
            AnimatedSlider("##reaction", &g_Settings.aimbot_legit_reaction, 50.0f, 300.0f, "%.0f ms");
            
            ImGui::TextColored(Colors::TextDim, "Smoothness");
            AnimatedSlider("##smooth", &g_Settings.aimbot_smoothness, 1.0f, 20.0f, "%.1f");
            
            ImGui::Spacing();
            SectionHeader("RAGE MODE");
            ToggleSwitch("Enable##rage", &g_Settings.rage_enabled);
            if (g_Settings.rage_enabled) g_Settings.aimbot_legit_mode = false; // Exclusive
            
            ImGui::Spacing();
            ToggleSwitch("Silent Aim", &g_Settings.rage_silent_aim);
            ToggleSwitch("Auto Shoot", &g_Settings.rage_auto_shoot);
            
            ImGui::Spacing();
            SectionHeader("RESOLVER");
            ToggleSwitch("Enable##resolver", &g_Settings.resolver_enabled);
            ToggleSwitch("Visualize##resolver", &g_Settings.resolver_visualize);
            ToggleSwitch("Prediction##resolver", &g_Settings.resolver_prediction);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Counters anti-aim!");
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Blatant!");

            ImGui::NextColumn();
            
            SectionHeader("TARGET");
            
            ImGui::TextColored(Colors::TextDim, "FOV Radius");
            AnimatedSlider("##fov", &g_Settings.aimbot_fov, 10.0f, 400.0f, "%.0f");
            
            ImGui::Spacing();
            
            ImGui::TextColored(Colors::TextDim, "Bone");
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(25, 25, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(35, 35, 45, 255));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 20, 26, 250));
            ImGui::SetNextItemWidth(-1);
            const char* bones[] = { "Head", "Torso" };
            ImGui::Combo("##bones", &g_Settings.aimbot_target_bone, bones, 2);
            ImGui::PopStyleColor(3);
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Hotkey");
            
            const char* key_text = g_Settings.waiting_for_key ? "..." : GetKeyName(g_Settings.aimbot_key);
            ImGui::PushStyleColor(ImGuiCol_Button, g_Settings.waiting_for_key ? Colors::WithAlpha(Colors::Primary, 0.3f) : IM_COL32(25, 25, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(35, 35, 45, 255));
            if (ImGui::Button(key_text, ImVec2(-1, 28))) {
                g_Settings.waiting_for_key = true;
            }
            ImGui::PopStyleColor(2);
            
            if (g_Settings.waiting_for_key) {
                for (int vk = 0x01; vk <= 0x5D; vk++) {
                    if (vk == VK_INSERT || vk == VK_ESCAPE) continue;
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        g_Settings.aimbot_key = vk;
                        g_Settings.waiting_for_key = false;
                        break;
                    }
                }
            }
            
            ImGui::Spacing();
            ImGui::Spacing();
            SectionHeader("BACKTRACK");
            ToggleSwitch("Enable##bt", &g_Settings.backtrack_enabled);
            ImGui::Spacing();
            ToggleSwitch("Visualize##bt", &g_Settings.backtrack_visualize);
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Time Window (ms)");
            AnimatedSlider("##bttime", &g_Settings.backtrack_time_ms, 10.0f, 200.0f, "%.0f ms");
        }
        else if (active_tab == 1) // Visuals
        {
            ImGui::Columns(2, "##VisCols", false);
            
            SectionHeader("ESP");
            ToggleSwitch("Enable", &g_Settings.esp_enabled);
            ImGui::Spacing();
            ToggleSwitch("Boxes", &g_Settings.esp_box);
            ToggleSwitch("Names", &g_Settings.esp_names);
            ToggleSwitch("Health Bars", &g_Settings.esp_health_bar);
            ToggleSwitch("Distance", &g_Settings.esp_distance);
            ToggleSwitch("Tracers", &g_Settings.esp_tracers);
            ToggleSwitch("Team Check", &g_Settings.esp_team_check);
            
            ImGui::NextColumn();
            
            SectionHeader("OVERLAY");
            ToggleSwitch("FOV Circle", &g_Settings.show_fov_circle);
            ToggleSwitch("Crosshair", &g_Settings.show_crosshair);
            
            ImGui::Spacing();
            SectionHeader("COLORS");
            
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(25, 25, 32, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            ImGui::Text("Enemy");
            ImGui::SameLine(80);
            ImGui::ColorEdit3("##enemycol", g_Settings.esp_color_enemy, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            
            ImGui::Text("Team");
            ImGui::SameLine(80);
            ImGui::ColorEdit3("##teamcol", g_Settings.esp_color_team, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();
            SectionHeader("ABOUT");
            ImGui::TextColored(Colors::TextDim, "Free Lama External");
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 0.8f), "leqit.de");
        }
        else if (active_tab == 2) // Blatant
        {
            ImGui::Columns(2, "##MoveCols", false);
            
            SectionHeader("FLY");
            ToggleSwitch("Enable##fly", &g_Settings.fly_enabled);
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Speed");
            AnimatedSlider("##flyspeed", &g_Settings.fly_speed, 0.1f, 5.0f, "%.1f");
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Fly Key (hold)");
            
            const char* fly_key_text = g_Settings.waiting_for_fly_key ? "..." : GetKeyName(g_Settings.fly_key);
            ImGui::PushStyleColor(ImGuiCol_Button, g_Settings.waiting_for_fly_key ? Colors::WithAlpha(Colors::Primary, 0.3f) : IM_COL32(25, 25, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(35, 35, 45, 255));
            if (ImGui::Button(fly_key_text, ImVec2(-1, 28))) {
                g_Settings.waiting_for_fly_key = true;
            }
            ImGui::PopStyleColor(2);
            
            if (g_Settings.waiting_for_fly_key) {
                for (int vk = 0x01; vk <= 0x5D; vk++) {
                    if (vk == VK_INSERT || vk == VK_ESCAPE) continue;
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        g_Settings.fly_key = vk;
                        g_Settings.waiting_for_fly_key = false;
                        break;
                    }
                }
            }
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "WASD moves where you look");
            ImGui::TextColored(Colors::TextDim, "Space = Up, Ctrl = Down");
            
            ImGui::NextColumn();
            
            SectionHeader("TELEPORT");
            ImGui::TextColored(Colors::TextDim, "Teleport to nearest player:");
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::WithAlpha(Colors::Primary, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::WithAlpha(Colors::Primary, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Primary);
            
            if (ImGui::Button("Teleport to Closest", ImVec2(-1, 32))) {
                g_Settings.teleport_to_closest = true;
            }
            
            ImGui::PopStyleColor(3);
            
            ImGui::Spacing();
            ImGui::Spacing();
            SectionHeader("HVH - ANTI-AIM");
            ToggleSwitch("Enable##aa", &g_Settings.antiaim_enabled);
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Type");
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(25, 25, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(35, 35, 45, 255));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 20, 26, 250));
            ImGui::SetNextItemWidth(-1);
            const char* aa_types[] = { "Spin", "Jitter", "Random" };
            ImGui::Combo("##aatype", &g_Settings.antiaim_type, aa_types, 3);
            ImGui::PopStyleColor(3);
            
            ImGui::Spacing();
            ImGui::TextColored(Colors::TextDim, "Speed");
            AnimatedSlider("##aaspeed", &g_Settings.antiaim_speed, 1.0f, 30.0f, "%.1f");
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Makes you harder to headshot!");
        }
        else if (active_tab == 3) // Settings
        {
            SectionHeader("CONFIGURATION");
            
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 60, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 80, 80, 255));
            if (ImGui::Button("Reset All", ImVec2(120, 32))) {
                g_Settings = Settings();
            }
            ImGui::PopStyleColor(2);
            
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextColored(Colors::TextDim, "Free Lama External");
            ImGui::TextColored(Colors::TextDim, "leqit.de | Build Dec 2025");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), "For educational purposes only.");
        }

        
        ImGui::EndChild();
    }
    ImGui::End();
    
    ImGui::PopStyleVar();
}

void Overlay::DrawText(Vector2 position, const char* text, unsigned int color)
{
    ImGui::GetForegroundDrawList()->AddText(ImVec2(position.x, position.y), color, text);
}

void Overlay::DrawPlayerCount(int player_count)
{
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    
    // FOV Circle
    if (g_Settings.show_fov_circle)
    {
        draw_list->AddCircle(
            ImVec2(screen_size.x / 2, screen_size.y / 2),
            g_Settings.aimbot_fov,
            Colors::ToU32(Colors::TextDim),
            64,
            1.0f
        );
    }
    
    // Crosshair
    if (g_Settings.show_crosshair)
    {
        float cx = screen_size.x / 2;
        float cy = screen_size.y / 2;
        float sz = g_Settings.crosshair_size;
        
        draw_list->AddLine(ImVec2(cx - sz, cy), ImVec2(cx + sz, cy), Colors::ToU32(Colors::Primary), 2.0f);
        draw_list->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy + sz), Colors::ToU32(Colors::Primary), 2.0f);
    }
    
    // Bottom Watermark - Always visible with solid background
    if (minecraft_font) ImGui::PushFont(minecraft_font);
    
    char watermark[64];
    sprintf_s(watermark, "leqit.de | FPS: %.0f", ImGui::GetIO().Framerate);
    ImVec2 text_size = ImGui::CalcTextSize(watermark);
    
    float pad_x = 16.0f;
    float pad_y = 8.0f;
    float box_w = text_size.x + pad_x * 2;
    float box_h = text_size.y + pad_y * 2;
    float box_x = (screen_size.x - box_w) / 2;
    float box_y = screen_size.y - box_h - 10;
    
    // Solid dark background
    draw_list->AddRectFilled(
        ImVec2(box_x, box_y),
        ImVec2(box_x + box_w, box_y + box_h),
        IM_COL32(12, 12, 16, 240),
        6.0f
    );
    
    // Subtle border
    draw_list->AddRect(
        ImVec2(box_x, box_y),
        ImVec2(box_x + box_w, box_y + box_h),
        IM_COL32(50, 50, 60, 150),
        6.0f
    );
    
    // Top accent line
    draw_list->AddRectFilled(
        ImVec2(box_x, box_y),
        ImVec2(box_x + box_w, box_y + 2),
        Colors::ToU32(Colors::Primary),
        6.0f, ImDrawFlags_RoundCornersTop
    );
    
    // Text
    ImVec2 text_pos = ImVec2(box_x + pad_x, box_y + pad_y);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), watermark);
    
    if (minecraft_font) ImGui::PopFont();
}

bool Overlay::ShouldExit()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) return true;
    }
    return false;
}

bool Overlay::IsRunning()
{
    return !ShouldExit();
}

LRESULT CALLBACK Overlay::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;
    
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Overlay::Cleanup()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    if (render_target_view) render_target_view->Release();
    if (swapchain) swapchain->Release();
    if (context) context->Release();
    if (device) device->Release();
    
    if (window_handle) DestroyWindow(window_handle);
}
